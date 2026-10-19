package com.millie.two;

import java.util.*;

/** Organization attribution from IEEE and Bluetooth SIG assignments; never a police-presence claim. */
final class PublicSafetyMatcher {
    static final class Match {
        final String brand,basis,evidence; final boolean cameraMaker,dock;
        Match(String brand,String basis,String evidence,boolean cameraMaker){this(brand,basis,evidence,cameraMaker,false);}
        Match(String brand,String basis,String evidence,boolean cameraMaker,boolean dock){this.brand=brand;this.basis=basis;this.evidence=evidence;this.cameraMaker=cameraMaker;this.dock=dock;}
        String label(){return "PUBLIC SAFETY · "+brand;}
    }
    static String organization(String name){return name==null?"":name.toLowerCase(Locale.ROOT).replaceAll("[^a-z0-9]+"," ").trim();}
    static Match ieee(String vendor,String type,String prefix){
        if(prefix==null||prefix.isEmpty()||prefix.equals("LOCAL"))return null;
        String org=organization(vendor),brand;boolean camera;
        switch(org){
            case "axon enterprise inc":case "taser international inc":brand="AXON / TASER";camera=true;break;
            case "flock safety":brand="FLOCK SAFETY";camera=true;break;
            case "motorola solutions inc":case "motorola solutions malaysia sdn bhd":brand="MOTOROLA SOLUTIONS";camera=false;break;
            case "watchguard video":case "watchguard video inc":brand="WATCHGUARD VIDEO";camera=true;break;
            case "digital ally inc":brand="DIGITAL ALLY";camera=true;break;
            default:return null;
        }
        return new Match(brand,"IEEE VENDOR","IEEE "+(type==null?"":type)+" "+SignalClassifier.formatOui(prefix)+" · "+vendor,camera);
    }
    static Match named(Match match,String name,boolean identityName){
        if(match==null||!identityName)return match;
        String normalized=organization(name);boolean dock=normalized.matches(".*(?:^| )(?:dock[0-9]*|docking|dockcontroller)(?: |$).*");
        boolean controller=match.brand.equals("MOTOROLA SOLUTIONS")&&normalized.matches(".*(?:^| )dc ?200(?: |$).*");
        if(!dock&&!controller)return match;
        return new Match(match.brand,match.basis,match.evidence+" · advertised name/SSID "+name,false,true);
    }
    static Match ble(String manufacturers,String services){
        for(String block:manufacturerBlocks(manufacturers)){
            if(block.length()<4||block.length()%2!=0||!block.matches("[0-9A-Fa-f]+"))continue;
            int id=Integer.parseInt(block.substring(0,2),16)|(Integer.parseInt(block.substring(2,4),16)<<8);
            if(id==0x034D)return new Match("AXON / TASER","BLE COMPANY","Bluetooth SIG company 0x034D · TASER International, Inc.",false);
            if(id==0x04EC)return new Match("MOTOROLA SOLUTIONS","BLE COMPANY","Bluetooth SIG company 0x04EC · Motorola Solutions",false);
        }
        for(String id:new String[]{"fe6b","fe6c","fc81"})if(hasService(services,id))return new Match("AXON / TASER","BLE SERVICE","Bluetooth SIG service 0x"+id.toUpperCase(Locale.ROOT)+" · "+(id.equals("fc81")?"Axon Enterprise, Inc.":"TASER International, Inc."),false);
        for(String id:new String[]{"fe04","fd8e"})if(hasService(services,id))return new Match("MOTOROLA SOLUTIONS","BLE SERVICE","Bluetooth SIG service 0x"+id.toUpperCase(Locale.ROOT)+" · Motorola Solutions",false);
        return null;
    }
    static String[] manufacturerBlocks(String value){return value==null?new String[0]:value.trim().split("[,;\\s]+");}
    static String uuid(String value){String s=value==null?"":value.toLowerCase(Locale.ROOT).replace("-","").trim();if(s.startsWith("0x"))s=s.substring(2);if(s.matches("[0-9a-f]{4}"))return "0000"+s+"00001000800000805f9b34fb";if(s.matches("[0-9a-f]{8}"))return s+"00001000800000805f9b34fb";return s.matches("[0-9a-f]{32}")?s:"";}
    static boolean hasService(String services,String id){String target=uuid(id);if(target.isEmpty()||services==null)return false;for(String service:services.split("[,;\\s]+"))if(target.equals(uuid(service)))return true;return false;}
}
