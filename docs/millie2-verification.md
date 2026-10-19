# MILLIE 2.0.13: parallel scans and deduplication

Installed and tested on the Moto G Play 2024 with the CYD connected through USB and debugging over Wi-Fi.

## Behavior

- Phone BLE scanning stays active alongside CYD USB reception. Phone Wi-Fi requests run on their independent 30-second timer.
- Header and merged records show PHONE + CYD when both receivers contribute.
- Identity is radio type plus canonical address: Wi-Fi BSSID or BLE address. Case and separator differences are normalized.
- Separate BSSIDs sharing an SSID remain distinct. Wi-Fi and BLE are not combined just because their addresses happen to match.
- Each record retains both sources' metadata, RSSI, observation counts and ages. Phone capabilities/frequency data are preserved when a CYD frame arrives.
- Phone scan timestamps and CYD session/sequence IDs prevent repeated cached results and USB replay from inflating observations or graph points. Cached Wi-Fi ages are preserved.
- Detail graphs separate phone readings from CYD readings using solid and dashed traces in the radio's existing color.

## Verification

15 controlled merge checks passed on the phone: cross-source address normalization, shared source attribution, independent reading counts, preserved metadata and name, true timestamps, cached-result suppression, replay suppression, probe-target handling, same-name/different-BSSID separation, radio separation, retained BLE evidence, old-result rejection, invalid-address rejection and cached-age handling.

The connected-CYD integration test also passed: real phone BLE and CYD observations continued simultaneously; the phone Wi-Fi request counter advanced; phone Wi-Fi results were present; closing/reopening the screen preserved the USB connection; a forced serial read failure recovered automatically; repeated connect requests retained one reader.

One live snapshot during the test showed 29 phone Wi-Fi observations, 185 phone BLE observations, 33 CYD Wi-Fi observations, and 10 CYD BLE observations. The combined list held 64 unique records, including 18 records shared between receivers. These are observation and record counts from that snapshot, not a count of physical devices or confirmed cameras. The USB test completed after 257 received JSON lines and exactly two opens, including the deliberate recovery test.

Android controls Wi-Fi scan frequency and may return cached results. MILLIE keeps requesting scans and uses new result timestamps rather than pretending an unchanged cache is a new measurement. See [Android Wi-Fi scanning](https://developer.android.com/develop/connectivity/wifi/wifi-scan).

The tests establish concurrent operation while the app is running. They do not establish unrestricted background Wi-Fi scanning beyond Android's limits.
