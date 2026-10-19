package com.millie.two;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.hardware.usb.*;
import android.os.*;
import android.util.Log;
import com.hoho.android.usbserial.driver.*;
import com.hoho.android.usbserial.util.SerialInputOutputManager;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** Owns the USB transport independently of any Activity instance. */
public final class CydLinkService extends Service {
    static final String PERMISSION="com.millie.civops2.USB_PERMISSION", STOP="com.millie.civops2.STOP_LINK";
    static final String TAG="MillieCydLink", CHANNEL="cyd_link";
    interface Listener { void onLines(List<String> lines); void onLinkState(boolean live,String message); }
    final class LocalBinder extends Binder { CydLinkService service(){return CydLinkService.this;} }
    final Handler main=new Handler(Looper.getMainLooper());
    final HashSet<Listener> listeners=new HashSet<>();
    final LinkedHashMap<String,String> latest=new LinkedHashMap<>();
    final ArrayList<String> pending=new ArrayList<>();
    final ByteArrayOutputStream buffer=new ByteArrayOutputStream();
    PowerManager.WakeLock cpu; HandlerThread thread; Handler worker; UsbManager usb; UsbSerialPort port; UsbDeviceConnection connection;
    SerialInputOutputManager serial; String deviceName="",state="CYD disconnected"; boolean live,enabled,permissionPending,permissionDenied,flushQueued; int generation;
    final String sampleSession=UUID.randomUUID().toString(); long lastData,openedAt,received,reconnects; boolean overflow;
    @Override public void onCreate(){super.onCreate();usb=(UsbManager)getSystemService(USB_SERVICE);cpu=((PowerManager)getSystemService(POWER_SERVICE)).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,"MILLIE:CYD");cpu.setReferenceCounted(false);thread=new HandlerThread("MILLIE USB");thread.start();worker=new Handler(thread.getLooper());
        getSystemService(NotificationManager.class).createNotificationChannel(new NotificationChannel(CHANNEL,"CYD connection",NotificationManager.IMPORTANCE_LOW));
        IntentFilter permission=new IntentFilter(PERMISSION);IntentFilter devices=new IntentFilter();devices.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);devices.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        if(Build.VERSION.SDK_INT>=33){registerReceiver(permissionReceiver,permission,Context.RECEIVER_NOT_EXPORTED);registerReceiver(deviceReceiver,devices,Context.RECEIVER_NOT_EXPORTED);}else{registerReceiver(permissionReceiver,permission);registerReceiver(deviceReceiver,devices);}
    }
    @Override public int onStartCommand(Intent intent,int flags,int startId){if(intent!=null&&STOP.equals(intent.getAction())){worker.post(()->{enabled=false;worker.removeCallbacks(tick);closePort();setState(false,"CYD disconnected");});stopForeground(STOP_FOREGROUND_REMOVE);stopSelf();return START_NOT_STICKY;}
        Notification n=notification(live?state:"Connecting CYD");if(Build.VERSION.SDK_INT>=29)startForeground(20,n,ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE);else startForeground(20,n);
        worker.post(()->{enabled=true;permissionDenied=false;worker.removeCallbacks(tick);worker.post(tick);});return START_STICKY;}
    Notification notification(String message){PendingIntent open=PendingIntent.getActivity(this,0,new Intent(this,MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);PendingIntent stop=PendingIntent.getService(this,1,new Intent(this,CydLinkService.class).setAction(STOP),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this,CHANNEL).setSmallIcon(R.drawable.ic_millie_status).setContentTitle("MILLIE · CYD").setContentText(message).setContentIntent(open).setOngoing(true).setOnlyAlertOnce(true).addAction(new Notification.Action.Builder(null,"Disconnect",stop).build()).build();}
    @Override public IBinder onBind(Intent intent){return new LocalBinder();}
    void register(Listener listener){listeners.add(listener);worker.post(()->{ArrayList<String> copy=new ArrayList<>(latest.values());boolean ready=live;String message=state;main.post(()->{if(listeners.contains(listener)){listener.onLines(copy);listener.onLinkState(ready,message);}});});}
    void unregister(Listener listener){listeners.remove(listener);}
    void send(String command){worker.post(()->{if(port==null)return;try{port.write((command+"\n").getBytes(StandardCharsets.UTF_8),1000);}catch(Exception e){failed("write: "+e.getMessage());}});}
    final Runnable tick=new Runnable(){public void run(){if(!enabled)return;try{if(port==null)tryOpen();else if(!usb.getDeviceList().containsKey(deviceName))failed("USB detached");else if(SystemClock.elapsedRealtime()-Math.max(lastData,openedAt)>20000)failed("CYD stream timed out");}catch(Exception e){failed(e.getClass().getSimpleName()+": "+e.getMessage());}if(enabled)worker.postDelayed(this,2000);}};
    @android.annotation.SuppressLint("WakelockTimeout")
    void tryOpen(){List<UsbSerialDriver> drivers=UsbSerialProber.getDefaultProber().findAllDrivers(usb);UsbSerialDriver chosen=null;for(UsbSerialDriver d:drivers){int vendor=d.getDevice().getVendorId();if(vendor==0x1a86||vendor==0x10c4){chosen=d;break;}}if(chosen==null){setState(false,"Waiting for CYD USB");return;}UsbDevice device=chosen.getDevice();
        if(!usb.hasPermission(device)){if(!permissionPending&&!permissionDenied){permissionPending=true;PendingIntent permission=PendingIntent.getBroadcast(this,0,new Intent(PERMISSION).setPackage(getPackageName()),PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_MUTABLE);usb.requestPermission(device,permission);setState(false,"USB permission required");}return;}
        permissionPending=false;UsbDeviceConnection candidate=usb.openDevice(device);if(candidate==null)throw new IllegalStateException("USB open failed");connection=candidate;port=chosen.getPorts().get(0);try{port.open(connection);port.setParameters(115200,8,UsbSerialPort.STOPBITS_1,UsbSerialPort.PARITY_NONE);}catch(Exception e){closePort();throw new IllegalStateException(e);}
        deviceName=device.getDeviceName();if(!cpu.isHeld())cpu.acquire();buffer.reset();overflow=false;lastData=0;openedAt=SystemClock.elapsedRealtime();final int token=++generation;
        serial=new SerialInputOutputManager(port,new SerialInputOutputManager.Listener(){public void onNewData(byte[] bytes){byte[] copy=bytes.clone();worker.post(()->{if(token==generation)read(copy);});}public void onRunError(Exception error){worker.post(()->{if(token==generation)failed("serial: "+error.getMessage());});}});serial.setReadTimeout(1000);serial.start();reconnects++;setState(false,"CYD serial connected");Log.i(TAG,"OPEN generation="+generation+" opens="+reconnects+" device="+deviceName);}
    void read(byte[] bytes){for(byte b:bytes){if(b=='\n'){if(!overflow&&buffer.size()>0)accept(new String(buffer.toByteArray(),StandardCharsets.UTF_8).trim());buffer.reset();overflow=false;}else if(b!='\r'){if(buffer.size()<32768&&!overflow)buffer.write(b);else overflow=true;}}}
    void accept(String line){try{JSONObject json=new JSONObject(line);lastData=SystemClock.elapsedRealtime();received++;json.put("_source","CYD");json.put("_receivedAt",System.currentTimeMillis());json.put("_sampleSession",sampleSession);json.put("_sampleOrder",received);json.put("_sampleId",sampleSession+":"+received);line=json.toString();String type=json.optString("t");if(type.equals("wifi")||type.equals("ble")){String key=type+"|"+ObservationState.address(json.optString("mac"));latest.remove(key);latest.put(key,line);while(latest.size()>300)latest.remove(latest.keySet().iterator().next());}
        if(!live)setState(true,"CYD receiving");synchronized(pending){pending.add(line);if(pending.size()>2000)pending.remove(0);if(!flushQueued){flushQueued=true;main.postDelayed(flush,100);}}
    }catch(Exception ignored){Log.w(TAG,"Malformed serial line discarded");}}
    final Runnable flush=()->{ArrayList<String> batch;synchronized(pending){batch=new ArrayList<>(pending);pending.clear();flushQueued=false;}for(Listener l:new ArrayList<>(listeners))l.onLines(batch);};
    void failed(String reason){closePort();setState(false,"CYD reconnecting · "+reason);Log.w(TAG,"RECONNECT "+reason+" lines="+received);}
    void closePort(){generation++;SerialInputOutputManager old=serial;serial=null;UsbSerialPort oldPort=port;port=null;if(old!=null)old.stop();try{if(oldPort!=null)oldPort.close();}catch(Exception ignored){}try{if(connection!=null)connection.close();}catch(Exception ignored){}connection=null;if(cpu.isHeld())cpu.release();deviceName="";buffer.reset();}
    void setState(boolean ready,String message){if(live==ready&&state.equals(message))return;live=ready;state=message;Log.i(TAG,"STATE "+message);if(enabled)getSystemService(NotificationManager.class).notify(20,notification(message));main.post(()->{for(Listener listener:new ArrayList<>(listeners))listener.onLinkState(ready,message);});}
    final BroadcastReceiver permissionReceiver=new BroadcastReceiver(){public void onReceive(Context context,Intent intent){worker.post(()->{permissionPending=false;permissionDenied=!intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED,false);if(permissionDenied)setState(false,"USB permission denied");else if(enabled){worker.removeCallbacks(tick);worker.post(tick);}});}};
    final BroadcastReceiver deviceReceiver=new BroadcastReceiver(){public void onReceive(Context context,Intent intent){UsbDevice device=intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);worker.post(()->{if(UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())){permissionPending=false;permissionDenied=false;if(device!=null&&device.getDeviceName().equals(deviceName))failed("USB detached");}else if(enabled){permissionDenied=false;worker.removeCallbacks(tick);worker.post(tick);}});}};
    @Override public void onDestroy(){try{unregisterReceiver(permissionReceiver);unregisterReceiver(deviceReceiver);}catch(Exception ignored){}worker.post(()->{enabled=false;worker.removeCallbacksAndMessages(null);closePort();thread.quitSafely();});listeners.clear();main.removeCallbacksAndMessages(null);super.onDestroy();}
}
