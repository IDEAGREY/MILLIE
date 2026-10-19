# MILLIE

**Read what’s broadcasting around you.**

By **idea grey**.

MILLIE is an Android app for nearby Wi-Fi and Bluetooth signals. See live signal strength, inspect camera-maker and tracker signatures, and find public safety manufacturer matches, including Flock Safety and Axon/TASER. Use the phone on its own, or connect a compatible CYD over USB for additional radio observations.

## Download MILLIE 2.0

| Download | What it is |
| --- | --- |
| **[MILLIE.apk](https://github.com/IDEAGREY/MILLIE/releases/download/v2.0.14/MILLIE.apk)** | Signed Android app **2.0.14**. Start here. |
| [MILLIE-CYD-2.0.4.bin](https://github.com/IDEAGREY/MILLIE/releases/download/v2.0.14/MILLIE-CYD-2.0.4.bin) | Optional CYD firmware **2.0.4**, for the hardware and existing flash layout described below. |
| [Release and all downloads](https://github.com/IDEAGREY/MILLIE/releases/tag/v2.0.14) | Source packages, checksums, and installation notes. |

The app and firmware have different version numbers. **Android 2.0.14 + CYD 2.0.4 is the published pair.** This MILLIE 2.0 release is separate from the older 3.x APKs in this repository.

## What it does

| View | What you get |
| --- | --- |
| **Live** | Wi-Fi and BLE observations, a signal-strength scope, and a chart showing how many recently seen signals fall into each strength range. Wi-Fi is orange; BLE is blue. |
| **Cameras** | IEEE manufacturer matches for camera makers such as Ring, Wyze, Axis, Hikvision, Dahua, Reolink, and others. Known advertised router names and Arlo base-station names are excluded from the camera list. |
| **Trackers** | Apple Find My advertisement signatures, Tile and Samsung SmartTag service signatures, and explicitly advertised tracker names. A Find My signature does not prove a device is an AirTag. |
| **Public Safety** | Flock Safety, Axon/TASER, Motorola Solutions, WatchGuard Video, and Digital Ally manufacturer matches. Assigned Bluetooth identifiers provide additional Axon/TASER and Motorola evidence when advertised. |
| **Signal details** | Signal history, address, manufacturer, match evidence, source, observation ages, and the radio fields actually received. Phone and CYD readings are shown separately. |

**A signature is evidence, not a confirmed product.** An IEEE MAC assignment identifies an organization. Some camera makers also sell routers, hubs, alarms, or recorders. Hidden network names and weak signal strength do not prove a camera. Flock or Axon matches do not establish an exact model, a recording state, or the presence of police. Devices that are silent or omit identifiable broadcasts can be missed. The radar shows signal strength; it does not locate devices or measure direction or distance.

## Start with just your phone

1. Use an **Android 8.0+** phone with Wi-Fi and Bluetooth, and download **MILLIE.apk** above. No Termux is needed for this version.
2. Open the APK. If Android asks, allow installation from the browser or file manager you used.
3. Open **MILLIE**. Grant location and nearby-device/Bluetooth scan permissions, and keep Wi-Fi, Bluetooth, and Android location services on.
4. Tap **PHONE SCAN** if scanning has not started. Tap any signal for its details.

The app uses an offline IEEE registry for manufacturer lookups. Phone Wi-Fi scan frequency and cached results are controlled by Android. Bluetooth scanning and Wi-Fi requests run independently; adding a CYD does not turn phone scanning off.

**Updating an older install:** this APK uses a dedicated release certificate for idea grey. Android rejects an update over earlier debug-signed builds of `com.millie.civops2`. Those builds must be removed before installing this release, which can remove their local app data. Future APKs signed with this same release key can update this release normally.

## Add a CYD

The CYD is optional. It can also scan and display its radar and scrolling logs without a phone.

1. Check the [hardware and firmware guide](docs/millie2-cyd.md) before flashing. The download is an **app-partition image**, not a universal full-flash image.
2. Connect the supported CYD to an Android USB host/OTG port with a **data cable**.
3. Open MILLIE, tap **CYD LINK** if needed, and accept Android’s USB permission prompt.
4. Leave phone scanning on. **PHONE + CYD** indicates both are active. Shared Wi-Fi/BLE addresses merge into one record while retaining each receiver’s observations.

The CYD adds **2.4 GHz Wi-Fi management/data frame observations**, aggregated Wi-Fi control-frame counts, and **BLE advertisements**. Its Wi-Fi and BLE reception share one ESP32 radio. The Android app owns all manufacturer, camera, tracker, and public safety classification; the firmware sends raw observations. The CYD display needs **no touch input**.

## Sources and verification

- [Android source and build instructions](millie2/android/README.md)
- [CYD source and build instructions](millie2/firmware/README.md)
- [Connected phone/CYD and deduplication checks](docs/millie2-verification.md)
- [Public safety matching, exclusions, and primary sources](docs/millie2-public-safety.md)
- [IEEE registry provenance](millie2/android/app/src/main/assets/IEEE_REGISTRY_SOURCE.txt)

The app’s scan processing and USB recovery were tested on a Moto G Play 2024 with the supported CYD. Public safety classifier tests used controlled inputs; they do not establish a real nearby Flock or Axon detection.

## Older MILLIE projects

Earlier APKs, the original sketch, Termux setup, and Raspberry Pi project remain in the repository and older releases. Their features and installation instructions do not describe MILLIE 2.0. See the [archived README](docs/legacy-README.md) or [Pi guide](docs/install-pi.md) for those versions.
