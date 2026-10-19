package com.millie.two;

import java.util.Locale;

/** Explainable app-side labels derived from IEEE assignment names or explicit BLE signatures. */
final class SignalClassifier {
    static final class Result {
        final String label, evidence;
        final boolean camera, tracker, publicSafety;
        Result(String label, String evidence, boolean camera, boolean tracker, boolean publicSafety) {
            this.label = label;
            this.evidence = evidence;
            this.camera = camera;
            this.tracker = tracker;
            this.publicSafety = publicSafety;
        }
    }

    static Result classify(String kind, String vendor, String ouiType, String ouiPrefix,
                           String frameType, String name, String manufacturer, String services) {
        return classify(kind,vendor,ouiType,ouiPrefix,frameType,name,manufacturer,services,
                "BLE".equals(safe(kind).trim()) || isNetworkIdentityFrame(frameType));
    }

    static Result classify(String kind, String vendor, String ouiType, String ouiPrefix,
                           String frameType, String name, String manufacturer, String services, boolean identityName) {
        String v = safe(vendor).toLowerCase(Locale.ROOT);
        String prefix = safe(ouiPrefix);
        String evidence = prefix.isEmpty() || prefix.equals("LOCAL") ? "" :
                "IEEE " + safe(ouiType) + " " + formatOui(prefix) + " · " + safe(vendor);
        boolean camera = any(v, "flock safety", "axon enterprise", "verkada inc", "axis communications",
                "arlo technology", "wyze labs", "blink by amazon", "nest labs", "hikvision",
                "zhejiang dahua", "vivotek", "lorex technology", "bosch security systems", "hanwha vision",
                "reolink innovation", "flir systems", "hangzhou ezviz software", "hangzhou huacheng network technology", "amcrest technologies", "swann communications", "zmodo technology corporation", "shanghai xiaoyi technology") || v.trim().equals("ring llc");
        PublicSafetyMatcher.Match safety=PublicSafetyMatcher.named(PublicSafetyMatcher.ieee(vendor,ouiType,ouiPrefix),name,identityName);
        boolean publicSafety=safety!=null;
        if(safety!=null){if(safety.dock)camera=false;else camera|=safety.cameraMaker;}

        if ("BLE ".equals(kind) || "BLE".equals(kind)) {
            String data = safe(manufacturer).toUpperCase(Locale.ROOT);
            String svc = safe(services).toLowerCase(Locale.ROOT);
            String n = safe(name).toLowerCase(Locale.ROOT);
            PublicSafetyMatcher.Match radioMatch=PublicSafetyMatcher.named(PublicSafetyMatcher.ble(manufacturer,services),name,identityName);
            if(safety!=null||radioMatch!=null){PublicSafetyMatcher.Match primary=safety!=null?safety:radioMatch;String detail=primary.evidence;if(safety!=null&&radioMatch!=null)detail+=" · "+radioMatch.evidence;return new Result(primary.label(),detail,primary.dock?false:camera,false,true);}
            boolean findMy=false;for(String block:PublicSafetyMatcher.manufacturerBlocks(data))if(block.startsWith("4C001219")&&block.length()>=58&&block.matches("[0-9A-F]+")&&block.length()%2==0)findMy=true;
            if (findMy)
                return new Result("APPLE FIND MY · TRACKER", "BLE manufacturer data · Apple Find My signature", camera, true, publicSafety);
            if (PublicSafetyMatcher.hasService(svc,"feec") || PublicSafetyMatcher.hasService(svc,"feed"))
                return new Result("TILE · TRACKER SERVICE", "BLE service UUID · Tile", camera, true, publicSafety);
            if (PublicSafetyMatcher.hasService(svc,"fd5a"))
                return new Result("SAMSUNG SMARTTAG · TRACKER SERVICE", "BLE service UUID · Samsung SmartTag", camera, true, publicSafety);
            if (containsWord(n, "airtag"))
                return new Result("AIRTAG · ADVERTISED NAME", "BLE advertised name · " + safe(name), camera, true, publicSafety);
            if (containsWord(n, "smarttag"))
                return new Result("SAMSUNG SMARTTAG · ADVERTISED NAME", "BLE advertised name · " + safe(name), camera, true, publicSafety);
            if (containsWord(n, "tile"))
                return new Result("TILE · ADVERTISED NAME", "BLE advertised name · " + safe(name), camera, true, publicSafety);
            if (publicSafety) return new Result("PUBLIC SAFETY VENDOR · " + safe(vendor), evidence, camera, false, true);
            if (camera) return new Result("CAMERA MAKER · " + safe(vendor), evidence, true, false, false);
            return new Result("BLE ADVERTISEMENT", evidence, false, false, false);
        }

        if (identityName && routerNetworkName(name))
            return new Result("ROUTER-NAMED NETWORK", evidence + " · advertised SSID " + safe(name), false, false, publicSafety);
        if (identityName && safe(name).toUpperCase(Locale.ROOT).startsWith("ARLO_VMB"))
            return new Result("ARLO · BASE STATION NETWORK", evidence + " · advertised SSID " + safe(name), false, false, false);
        if (safety!=null)return new Result(safety.label(),safety.evidence,camera,false,true);
        if (camera) return new Result("CAMERA MAKER · " + safe(vendor), evidence, true, false, false);
        String ft = safe(frameType);
        return new Result("ACCESS_POINT".equals(ft) ? "WI-FI ACCESS POINT" : "WI-FI FRAME · " + ft,
                evidence, false, false, false);
    }

    static boolean isNetworkIdentityFrame(String frame) {
        String f=safe(frame).toUpperCase(Locale.ROOT);
        return f.equals("ACCESS_POINT") || f.equals("BEACON") || f.equals("PROBE_RESP");
    }
    static boolean routerNetworkName(String name) {
        String n=safe(name).trim().toUpperCase(Locale.ROOT);
        for(String prefix:new String[]{"SPECTRUMSETUP-","SPECTRUM-","SPECTRUMWIFI","TP-LINK_","TP-LINK-","TPLINK_","NETGEAR","ASUS_","ASUS-","LINKSYS","EERO-"})
            if(n.startsWith(prefix))return true;
        return n.equals("EERO");
    }

    static String formatOui(String prefix) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < prefix.length(); i += 2) {
            if (i > 0) b.append(':');
            b.append(prefix, i, Math.min(i + 2, prefix.length()));
        }
        return b.toString();
    }

    private static boolean any(String value, String... terms) {
        for (String term : terms) if (value.contains(term)) return true;
        return false;
    }

    private static boolean containsWord(String value, String word) {
        return (" " + value.replaceAll("[^a-z0-9]+", " ") + " ").contains(" " + word + " ");
    }

    private static String safe(String value) { return value == null ? "" : value; }
}
