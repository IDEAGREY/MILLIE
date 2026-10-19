# MILLIE 2.0.11 public-safety coverage

The interface uses **PUBLIC SAFETY · manufacturer**. Charging docks do not receive a separate category or label. Matching evidence appears in the signal details. Classification lives in the Android app; the CYD supplies observations.

## IEEE assignments

Exact normalized organization names match these groups in the bundled IEEE registry:

| Manufacturer | Prefixes |
|---|---:|
| Axon Enterprise / TASER | 1 |
| Flock Safety | 1 |
| Motorola Solutions | 7 |
| WatchGuard Video | 1 |
| Digital Ally | 1 |
| **Total** | **11** |

Motorola Mobility, generic legacy Motorola assignments, Axon Networks, Axona, Axonne, Interaxon, Flock Audio, and WatchGuard Technologies do not become public-safety matches solely from those organization names. WatchGuard Video and the firewall company WatchGuard Technologies are different assignments.

The complete prefix list is in millie2-public-safety-prefixes.csv. IEEE organization assignment is vendor attribution, not proof that a device is a particular model or operated by police.

## Bluetooth SIG assignments

These are recognized when present in captured advertisements:

| Manufacturer | Company identifier | Assigned service UUIDs |
|---|---|---|
| Axon / TASER | 0x034D, TASER International, Inc. | 0xFE6B, 0xFE6C, 0xFC81 |
| Motorola Solutions | 0x04EC | 0xFE04, 0xFD8E |

Company identifiers are decoded from manufacturer-data block boundaries in little-endian order. They are not searched for as arbitrary payload substrings. Services accept short, 0x-prefixed and Bluetooth-base 128-bit UUID forms with exact matching. Phone service-data UUID keys also contribute to this evidence.

Previously captured identifiers are retained for the same signal record when a later phone/CYD advertisement omits them. Different addresses are not linked. Generic names or guessed prefixes do not create public-safety matches.

## Docks

Docks stay under Public Safety with the manufacturer. Where verified vendor evidence is accompanied by an advertised dock name, the app avoids putting that record in the camera tab. A probe request for a dock's network is not a dock identity. An unverified generic dock name does not create a public-safety match.

Passive radio coverage is limited to what actually transmits. Axon's Body 3 dock documentation says each bay functions as an Ethernet interface with its own MAC address, and the dock itself has no IP address. This does not make those wired bay addresses visible in nearby BLE/Wi-Fi scans. Pure power chargers may have no observable network identity at all.

Axon's newer firmware can rotate Bluetooth addresses. An IEEE-only scan can miss such devices; an assigned Bluetooth identifier can identify the vendor only when it remains in the received advertisement. No universal detection, exact camera-model claim or police-presence claim is made.

## Verification

108 plain-Java checks cover earlier camera/tracker rules, router-name distinctions, public-safety assignments, exclusions, UUID formats, manufacturer-data boundaries and dock handling. Five public-safety pipeline checks also passed on the Moto G Play 2024 on October 17, 2026, covering phone manufacturer-block boundaries, retained identifiers, address separation, CYD short-UUID input and Android service-data UUIDs. These use controlled inputs inside the installed app; they do not establish detection of a real Axon device nearby. The CYD was not detected on USB during this validation, so the full connected-CYD lifecycle test was not repeated. Version 2.0.11 was installed and its package version verified.

## Primary sources

- [IEEE Registration Authority](https://standards.ieee.org/products-programs/regauth/). Prefix counts refer to MILLIE's bundled IEEE snapshot.
- [Bluetooth SIG company identifiers](https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/company_identifiers/company_identifiers.yaml), retrieved October 16, 2026.
- [Bluetooth SIG member service UUIDs](https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/uuids/member_uuids.yaml), retrieved October 16, 2026.
- [Axon: TASER International changes name to Axon](https://investor.axon.com/2017-04-05-Axon-Offers-Free-Body-Cameras-for-Every-Police-Officer-in-the-U-S).
- [Axon: July 2026 Bluetooth privacy updates](https://www.axon.com/help/release-notes/cameras-and-sensors/2026/07-2026.htm).
- [Axon Body 3 dock network configuration](https://www.axon.com/help/axon-body-3/cameras-and-sensors/docks/network.htm).
- [Motorola Solutions: WatchGuard Video acquisition](https://www.motorolasolutions.com/newsroom/press-releases/motorola-solutions-acquires-watchguard-inc-leader-in-mobile-video-for-publi.html).
- [WatchGuard Technologies firewalls](https://www.watchguard.com/wgrd-products/firewalls).
- [Digital Ally: video solutions](https://www.digitalally.com/).
- [Flock Safety product portfolio](https://www.flocksafety.com/products).
