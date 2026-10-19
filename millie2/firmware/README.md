# MILLIE CYD firmware 2.0.4

Developer: **idea grey**. Optional companion for MILLIE Android **2.0.14**.

The bare CYD scans locally, shows a radar and scrolling logs, and streams raw Wi-Fi/BLE observations to Android over USB serial at 115200 baud. Wi-Fi is orange and BLE is blue. Touch input is not used. Camera, tracker, and public safety classification belongs to the Android app.

See the [hardware and flashing guide](../../docs/millie2-cyd.md) before writing any image. The published binary is app-only at `0x10000`, for the confirmed single-USB ESP32-2432S028R with ESP32-D0WD-V3, 4 MB flash, the included display setup, and an existing compatible `min_spiffs` bootloader/partition layout. No bootloader or partition table is included in the binary.

## Build from source

The verified build environment used arduino-cli, ESP32 Arduino core 2.0.11, TFT_eSPI 2.5.34, NimBLE-Arduino 2.5.0, and XPT2046_Touchscreen 1.4. Preferences and SPI come with the ESP32 core. The touch library is a build dependency; the product does not depend on working touch input.

After installing those dependencies, run from this directory:

```sh
python3 build.py
```

The script compiles only; it does not flash or erase a connected board. Output: `firmware/millie-standalone.ino.bin`. The build uses the included `CYD_Setup.h`, 4 MB flash, DIO mode, and `min_spiffs` partition configuration.

## Local checks

The C++ model/layout checks can run without an ESP32. From this directory, with a C++17 compiler:

```sh
c++ -std=c++17 tests/core_test.cpp -o /tmp/millie-core-test
/tmp/millie-core-test
```

The onboard ESP32 radio is limited to 2.4 GHz Wi-Fi and BLE reception. It shares Wi-Fi/BLE time, and Wi-Fi hops one channel at a time. The radar does not estimate direction or distance. There is no 5 GHz, Bluetooth Classic, GPS, NFC, UWB, cellular, Zigbee, or sub-GHz receiver in this build.

Product reference: [SquachWatch-CYD](https://github.com/skizzophrenic/SquachWatch-CYD). Its source code was not imported; this firmware was developed from MILLIE’s local firmware.
