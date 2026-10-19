# MILLIE 2.0.14

Developer: idea grey. App name: MILLIE.

Fresh Android application for the MILLIE CivOps CYD firmware. Package ID: `com.millie.civops2` (separate from earlier MILLIE apps).

## Signal views

Black terminal UI with green structure, orange Wi-Fi, blue BLE, and restrained pink emphasis. Primary navigation groups the live feed, camera-maker matches, tracker signatures, and public-safety vendor matches. Event log and CYD link controls are grouped under More. The app scans phone Wi-Fi/BLE independently and can merge CYD observations over USB OTG serial at 115200 8N1 (CH340 and CP210x supported).

The offline IEEE MA-L, MA-M, MA-S and IAB tables are the source for organization attribution, using longest-prefix matching. Camera and public-safety tabs use those assigned organization names to identify relevant makers; they do not assert an exact product from an OUI alone. Tracker entries require a matching Apple Find My manufacturer signature, Tile or Samsung SmartTag service UUID, or an explicit advertised device name. Hidden SSIDs, RSSI, and scan rate do not create camera or public-safety labels. Tapped records show the classification evidence and available radio metadata.

The phone can display Wi-Fi access-point details and BLE advertisement fields available through Android. The CYD can contribute Wi-Fi frame observations, including frame subtype and SSID, plus BLE advertisement fields. Repeated readings share one signal record and graph by observation order.

## Radar terminal

The native Canvas scope shows the strongest valid RSSI for each radio as separate concentric arcs on a −110 to −20 dBm scale. Active contacts are unique radio/address pairs observed within 60 seconds. The stacked orange/blue histogram shows their RSSI distribution. The scope does not estimate bearing or distance. Signal details retain the real observation-history chart.

Navigation uses fixed category targets and a pink active indicator. Signal rows separate identity, classification, address, source and RSSI.

## Build

Use Android SDK 35 and JDK 17:

```sh
./gradlew assembleRelease
```

This builds an **unsigned** release APK at `app/build/outputs/apk/release/app-release-unsigned.apk`. Sign a local build with your own Android release keystore, for example using Android SDK `apksigner`:

```sh
apksigner sign --ks YOUR_RELEASE_KEYSTORE --ks-key-alias YOUR_ALIAS --out MILLIE.apk app/build/outputs/apk/release/app-release-unsigned.apk
apksigner verify --verbose --print-certs MILLIE.apk
```

The downloadable 2.0.14 APK is signed with idea grey’s dedicated release certificate. Its certificate SHA-256 is `7743003929add747a9eba2714b8f8ba955807dd895db7e5283e5808c65950970`. No signing key or password is included. A build signed with a different key cannot update that APK in place.

## USB connection

A connected-device foreground service owns the serial port independently of the screen. It keeps reception active while the screen closes, retains recent CYD observations for reopening, and retries after transport errors or detach/reattach. A 20-second valid-data watchdog recovers stalled streams. One worker owns each reader; generation checks discard callbacks from replaced readers. A partial wake lock is held only while the USB port is open. Disconnect is available from the connection notification.

## Network identity

SSID from a probe request is a requested network, not the transmitter identity. Only access-point, beacon and probe-response names become the record's network name. Known advertised names persist across unnamed data frames. Explicit router-network names override camera-maker categorization; hidden names alone never promote a device to camera. ARLO_VMB networks are categorized as base station networks.

## Public safety

Exact IEEE assignments cover Axon/TASER, Flock Safety, Motorola Solutions, WatchGuard Video and Digital Ally. Bluetooth SIG company IDs 0x034D and 0x04EC and assigned services FE6B, FE6C, FC81, FE04 and FD8E provide qualified vendor evidence when advertised. Normalization supports CYD short UUIDs and Android 128-bit UUIDs. Manufacturer blocks retain their boundaries; service-data UUID keys are included. Sparse packets retain previously observed identifiers for the same address.

Rows display PUBLIC SAFETY and manufacturer, including charging docks. Verified dock names only prevent a camera classification; there is no separate dock label. Silent wired docks cannot be discovered by passive nearby-radio scanning. Match evidence identifies an organization, not an exact model or operator.

## App icon

The launcher uses a black adaptive radar icon with green rings and a pink sweep. Android themed icons use its monochrome layer. The connection notification uses a separate transparent monochrome radar symbol.

## Parallel receivers and deduplication

Phone BLE scanning stays active alongside CYD USB reception; phone Wi-Fi scan requests continue on their independent 30-second timer under Android scan limits. The header shows PHONE + CYD when both modes are active.

A record is keyed by radio type and canonical device address (Wi-Fi BSSID or BLE address). Case and separator differences are normalized. Different BSSIDs sharing a network name stay separate. The same address on Wi-Fi and BLE stays separate. A shared record retains per-source readings and metadata and shows PHONE + CYD.

Phone scan timestamps and CYD session/sequence IDs prevent repeated cached results or USB replay from inflating counts and graphs. Cached Wi-Fi age remains its actual age. Tapped records show both receivers' signal levels, observation counts and ages; their graph separates phone and CYD readings using solid and dashed traces.
