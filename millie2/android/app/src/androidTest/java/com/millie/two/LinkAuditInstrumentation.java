package com.millie.two;

import android.app.*;
import android.content.*;
import android.os.*;
import java.util.concurrent.*;

/** Hardware integration check: requires the user's connected, permitted CYD. */
public final class LinkAuditInstrumentation extends Instrumentation {
    CydLinkService link; boolean pipelineOnly; int mergeChecks; long[] concurrent;
    void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    long[] snapshot() throws Exception {CountDownLatch done=new CountDownLatch(1);long[] result=new long[3];link.worker.post(()->{result[0]=link.received;result[1]=link.reconnects;result[2]=link.live?1:0;done.countDown();});check(done.await(5,TimeUnit.SECONDS),"USB worker responds");return result;}
    long[] awaitLive(long minimumOpens,long minimumLines) throws Exception {long end=SystemClock.elapsedRealtime()+20000;while(SystemClock.elapsedRealtime()<end){long[] s=snapshot();if(s[2]==1&&s[1]>=minimumOpens&&s[0]>minimumLines)return s;Thread.sleep(500);}throw new AssertionError("CYD failed to resume data within 20 seconds");}
    Intent screen(){return new Intent().setClassName(getTargetContext(),"com.millie.two.MainActivity").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);}
    @Override public void onCreate(Bundle arguments){super.onCreate(arguments);pipelineOnly=arguments!=null&&("public-safety".equals(arguments.getString("mode"))||"merge".equals(arguments.getString("mode")));start();}
    void checkSafetyPipeline() throws Exception {
        MainActivity activity=new MainActivity();MainActivity.Row axon=new MainActivity.Row();axon.kind="BLE ";axon.vendor="LOCAL / RANDOMIZED";axon.oui="LOCAL";
        org.json.JSONObject first=new org.json.JSONObject().put("manufacturer","4C0000,4D0301");
        check(activity.classifyInApp(axon,"ble",first).publicSafety,"Phone manufacturer-block boundaries reach classifier");
        check(activity.classifyInApp(axon,"ble",new org.json.JSONObject()).publicSafety,"Verified identifier retained across sparse packet");
        MainActivity.Row fresh=new MainActivity.Row();fresh.kind="BLE ";fresh.vendor="UNASSIGNED";
        check(!activity.classifyInApp(fresh,"ble",new org.json.JSONObject()).publicSafety,"No identity transfer to another address");
        check(activity.classifyInApp(fresh,"ble",new org.json.JSONObject().put("services","0xFE6C")).publicSafety,"CYD short service reaches classifier");
        MainActivity.Row serviceData=new MainActivity.Row();serviceData.kind="BLE ";serviceData.vendor="UNASSIGNED";
        org.json.JSONObject data=new org.json.JSONObject().put("serviceData",new org.json.JSONObject().put("0000fc81-0000-1000-8000-00805f9b34fb","01"));
        check(activity.classifyInApp(serviceData,"ble",data).publicSafety,"Android service-data UUID reaches classifier");
    }
    void merged(boolean ok,String message){mergeChecks++;check(ok,message);}
    org.json.JSONObject observation(String kind,String source,String mac,long order,long seen)throws Exception{return new org.json.JSONObject().put("t",kind).put("_source",source).put("mac",mac).put("rssi",-65).put("_sampleSession",source).put("_sampleOrder",order).put("_observedAt",seen);}
    void checkMergePipeline() throws Exception {
        MainActivity a=new MainActivity();long now=System.currentTimeMillis();String mac="00:11:22:33:44:55";
        org.json.JSONObject phone=observation("wifi","PHONE","00-11-22-33-44-55",100,now-5000).put("ftype","ACCESS_POINT").put("name","SharedNetwork").put("frequencyMHz",2412).put("capabilities","WPA2");
        a.parse(phone.toString(),false);a.parse(observation("wifi","CYD",mac.toLowerCase(),1,now).put("ftype","data").put("ch",1).toString(),false);
        MainActivity.Row r=a.rows.get("WIFI|"+mac);merged(a.rows.size()==1,"Cross-source MAC format deduplication");merged(r.source.equals("PHONE + CYD"),"Combined source attribution");merged(r.count==2,"Independent receiver readings counted");merged(r.raw.optInt("frequencyMHz")==2412&&r.raw.optString("capabilities").equals("WPA2"),"Phone metadata retained after CYD frame");merged(r.name.equals("SharedNetwork"),"Learned name retained");merged(r.firstSeen==now-5000&&r.lastSeen==now,"True sample ages retained");
        a.parse(phone.toString(),false);merged(r.count==2&&a.signalHistory.size()==2,"Cached Android result does not repeat graph/count");
        a.parse(observation("wifi","CYD",mac,1,now).toString(),false);merged(r.count==2,"Service replay does not repeat a measurement");
        a.parse(observation("wifi","CYD",mac,2,now+1).put("ftype","probe_req").put("ssid","SpectrumSetup-test").toString(),false);merged(r.name.equals("SharedNetwork"),"Probe target does not replace network identity");
        a.parse(observation("wifi","PHONE","00:11:22:33:44:66",101,now).put("name","SharedNetwork").put("ftype","ACCESS_POINT").toString(),false);merged(a.rows.size()==2,"Different BSSIDs with same SSID stay distinct");
        a.parse(observation("ble","PHONE",mac,200,now).put("manufacturer","4D0301").toString(),false);merged(a.rows.size()==3,"WiFi and BLE remain distinct identities");
        a.parse(observation("ble","CYD",mac,3,now).toString(),false);MainActivity.Row b=a.rows.get("BLE |"+mac);merged(b.source.equals("PHONE + CYD")&&b.publicSafety,"BLE sources merge without losing public-safety evidence");
        a.parse(observation("wifi","PHONE",mac,99,now-10000).put("name","Wrong old name").put("ftype","ACCESS_POINT").toString(),false);merged(r.name.equals("SharedNetwork")&&r.count==3,"Older cached sample cannot replace fresh identity");
        a.parse(observation("wifi","PHONE","not-a-mac",1,now).toString(),false);merged(a.rows.size()==3,"Invalid addresses do not become shared records");
        ObservationState age=new ObservationState();age.accept(new org.json.JSONObject().put("t","wifi").put("_source","PHONE").put("ageMs",120000),now);merged(age.lastSeen==now-120000,"Cached WiFi age is not relabeled live");
    }
    long[] scanState(MainActivity a){long[] state=new long[9];runOnMainSync(()->{state[0]=a.phoneActive?1:0;state[1]=a.phoneBleActive?1:0;state[2]=a.phoneWifiAccepted;state[3]=a.phoneBleAccepted;state[4]=a.cydWifiAccepted;state[5]=a.cydBleAccepted;state[6]=a.wifiScanRequests;for(MainActivity.Row row:a.snapshot())if(row.observations.sources.size()>1)state[7]++;state[8]=a.gotAlive?1:0;});return state;}
    void checkConcurrentScan(MainActivity a)throws Exception{long[] before=scanState(a);long end=SystemClock.elapsedRealtime()+45000;while(SystemClock.elapsedRealtime()<end){Thread.sleep(1000);long[] current=scanState(a);if(current[3]>before[3]&&current[4]+current[5]>before[4]+before[5]&&current[6]>before[6])break;}long[] after=scanState(a);check(after[0]==1&&after[1]==1&&after[8]==1,"Phone BLE scanner and CYD both active");check(after[3]>before[3],"Real phone BLE observations continue while CYD attached");check(after[4]+after[5]>before[4]+before[5],"Real CYD observations continue alongside phone");check(after[6]>before[6],"Android WiFi scan timer continues alongside CYD");check(after[2]>0,"Phone WiFi results are present");concurrent=new long[]{after[2]-before[2],after[3]-before[3],after[4]+after[5]-before[4]-before[5],after[7]};}
    @Override public void onStart(){Bundle result=new Bundle();ServiceConnection connection=null;try{
        final Throwable[] pipelineError=new Throwable[1];runOnMainSync(()->{try{checkSafetyPipeline();checkMergePipeline();}catch(Throwable error){pipelineError[0]=error;}});if(pipelineError[0]!=null)throw new AssertionError("Public-safety pipeline",pipelineError[0]);if(pipelineOnly){result.putString("result","PASS: 5 public-safety and 15 dual-source merge checks on device; USB hardware tests not requested");finish(Activity.RESULT_OK,result);return;}
        Activity first=startActivitySync(screen());CountDownLatch bound=new CountDownLatch(1);
        connection=new ServiceConnection(){public void onServiceConnected(ComponentName n,IBinder binder){link=((CydLinkService.LocalBinder)binder).service();bound.countDown();}public void onServiceDisconnected(ComponentName n){}};
        getTargetContext().bindService(new Intent(getTargetContext(),CydLinkService.class),connection,Context.BIND_AUTO_CREATE);check(bound.await(10,TimeUnit.SECONDS),"Service binds");long[] initial=awaitLive(1,0);checkConcurrentScan((MainActivity)first);initial=snapshot();
        runOnMainSync(first::finish);Thread.sleep(6000);long[] background=snapshot();check(background[2]==1&&background[0]>initial[0],"CYD continues receiving after Activity finishes");check(background[1]==initial[1],"Closing Activity does not reopen USB");
        startActivitySync(screen());Thread.sleep(2000);check(snapshot()[1]==initial[1],"Reopening screen reuses USB connection");
        long[] before=snapshot();link.worker.post(()->link.failed("audit simulated read failure"));long[] recovered=awaitLive(before[1]+1,before[0]);check(recovered[1]==before[1]+1,"One failure creates exactly one replacement reader");
        getTargetContext().startForegroundService(new Intent(getTargetContext(),CydLinkService.class));getTargetContext().startForegroundService(new Intent(getTargetContext(),CydLinkService.class));Thread.sleep(4000);long[] stable=snapshot();check(stable[2]==1&&stable[1]==recovered[1],"Repeated connect requests retain one live reader");check(stable[0]>recovered[0],"Data continues after recovery");
        result.putLongArray("concurrent_scan_deltas_wifi_ble_cyd_shared",concurrent);result.putInt("merge_checks",mergeChecks);result.putString("result","PASS: phone WiFi/BLE plus CYD concurrent scan; dual-source merge; 5 public-safety pipeline checks; Activity close/reopen, live background stream, simulated serial failure recovery, and duplicate-connect protection");result.putLong("lines",stable[0]);result.putLong("opens",stable[1]);finish(Activity.RESULT_OK,result);
    }catch(Throwable error){result.putString("error",error.toString());finish(Activity.RESULT_CANCELED,result);}finally{if(connection!=null)try{getTargetContext().unbindService(connection);}catch(Exception ignored){}}}
}
