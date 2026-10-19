# MILLIE 2.0: optional CYD firmware

Use **MILLIE-CYD-2.0.4.bin** with the **MILLIE Android 2.0.14** app. The phone app works without this hardware.

## Supported hardware

This image was verified on the single-USB **ESP32-2432S028R CYD**, ESP32-D0WD-V3 revision 3.1, **4 MB flash**, CH340 serial interface, and the ILI9341 display profile in [CYD_Setup.h](../millie2/firmware/CYD_Setup.h). Other USB serial CH340/CP210x interfaces are supported by the Android app, but that does not make every ESP32/display board compatible with this binary.

The display uses SCK 14, MOSI 13, MISO 12, CS 15, DC 2, and backlight 27, with portrait rotation. Do not use this binary for an ESP32-S3/C3/C5 or a different display/pinout.

## What the CYD does

- Scans locally without a phone and displays a radar and scrolling live log.
- Receives 2.4 GHz Wi-Fi management/data frames and BLE advertisements. Wi-Fi control frames are reported as aggregate counts, not invented device identities.
- Sends raw observations over USB serial at **115200 baud** for the phone to classify.
- Displays Wi-Fi in orange and BLE in blue. **Touch is not required or used.**

This build does not provide 5 GHz Wi-Fi, Bluetooth Classic, GPS, UWB, cellular, NFC, sub-GHz reception, wireless app transport, or direction/distance measurements. Wi-Fi listens on one channel at a time and hops; brief broadcasts can be missed.

## Flashing this download

**This is an app-only image for address `0x10000`. It is not a full-flash image and must not be written at address zero.** It does not contain the bootloader or partition table needed for a blank board.

The tested board already has a compatible bootloader and `min_spiffs` partition layout: app0 at `0x10000` (size `0x1e0000`), app1 at `0x1f0000`, filesystem at `0x3d0000`, and coredump at `0x3f0000`. Confirm this layout and the board/display before using the binary. If you cannot confirm them, do not flash this image yet.

Download the firmware and `SHA256SUMS.txt` from the [release](https://github.com/IDEAGREY/MILLIE/releases/tag/v2.0.14). Use Espressif’s [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32/). Replace `YOUR_PORT` below with this CYD’s actual serial port. Close any serial monitor first.

Back up the confirmed 4 MB board before writing:

```sh
python -m esptool --chip esp32 --port YOUR_PORT read-flash 0x0 0x400000 cyd-backup.bin
```

Write only the app partition:

```sh
python -m esptool --chip esp32 --port YOUR_PORT --baud 115200 write-flash 0x10000 MILLIE-CYD-2.0.4.bin
```

Do **not** run a whole-chip erase. Keep the backup private. After a successful write/verification, reset the board and check that the radar and scrolling log appear. Connect it to the phone by USB OTG, accept USB access, and check **PHONE + CYD** in MILLIE while phone scanning is enabled.

The published image previously passed esptool hash verification on the supported board and booted as `civops-2.0.4`. A 15-second runtime check recorded Wi-Fi, BLE, and aggregate control-frame messages with zero queue drops in that check. This is a tested configuration, not validation of every CYD sold under a similar name.
