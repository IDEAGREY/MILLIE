package com.millie.two;

import org.json.*;
import java.util.*;

/** One radio/address identity with independent, ordered observations from each receiver. */
final class ObservationState {
    static final class Sample {
        final String source; JSONObject data=new JSONObject(); String id="",session="";
        long order,observedAt; int rssi,count;
        Sample(String source){this.source=source;}
    }
    final LinkedHashMap<String,Sample> sources=new LinkedHashMap<>();
    long firstSeen,lastSeen;int count,rssi;
    static String address(String input){String hex=input==null?"":input.replace(":","").replace("-","").trim().toUpperCase(Locale.ROOT);if(!hex.matches("[0-9A-F]{12}"))return "";StringBuilder out=new StringBuilder();for(int i=0;i<12;i+=2){if(i>0)out.append(':');out.append(hex,i,i+2);}return out.toString();}
    Sample accept(JSONObject data,long now)throws JSONException {
        String source=data.optString("_source","CYD").toUpperCase(Locale.ROOT);
        if(!source.equals("PHONE")&&!source.equals("CYD"))return null;
        long observed=data.optLong("_observedAt",data.optLong("_receivedAt",now));
        if(source.equals("PHONE")&&data.optString("t").equals("wifi")&&!data.has("_observedAt"))observed=now-Math.max(0,data.optLong("ageMs",0));
        observed=Math.max(1,Math.min(now,observed));
        String id=data.optString("_sampleId"),session=data.optString("_sampleSession",source);
        long order=data.optLong("_sampleOrder",data.optLong("timestampMicros",data.optLong("timestampNanos",0)));
        Sample sample=sources.get(source);
        if(sample!=null){if(!id.isEmpty()&&id.equals(sample.id))return null;if(order>0&&sample.order>0&&session.equals(sample.session)&&order<=sample.order)return null;if(order<=0&&observed<sample.observedAt)return null;}
        else{sample=new Sample(source);sources.put(source,sample);}
        sample.data=merge(sample.data,data);sample.id=id;sample.order=order;sample.session=session;sample.observedAt=observed;sample.rssi=data.optInt("rssi",sample.rssi);sample.count++;count++;
        if(firstSeen==0||observed<firstSeen)firstSeen=observed;
        if(observed>=lastSeen){lastSeen=observed;rssi=sample.rssi;}
        return sample;
    }
    String sourceLabel(){return sources.containsKey("PHONE")?(sources.containsKey("CYD")?"PHONE + CYD":"PHONE"):"CYD";}
    JSONObject metadata()throws JSONException {ArrayList<Sample> sorted=new ArrayList<>(sources.values());sorted.sort(Comparator.comparingLong(s->s.observedAt));JSONObject out=new JSONObject();for(Sample sample:sorted)out=merge(out,sample.data);return out;}
    static JSONObject merge(JSONObject previous,JSONObject incoming)throws JSONException {JSONObject out=new JSONObject(previous.toString());Iterator<String> keys=incoming.keys();while(keys.hasNext()){String key=keys.next();Object value=incoming.opt(key);if(value==null||value==JSONObject.NULL)continue;if(value instanceof String&&((String)value).isEmpty()&&!key.equals("ssid")&&!key.equals("name"))continue;if(value instanceof JSONObject&&out.opt(key) instanceof JSONObject)out.put(key,merge(out.getJSONObject(key),(JSONObject)value));else out.put(key,value);}return out;}
}
