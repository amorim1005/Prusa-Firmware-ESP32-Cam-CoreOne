# Development

How to build the firmware and follow the original project. For flashing, setup and use, see the [README](../README.md).

<a name="compile"></a>
## How to compile

The firmware compiles with the **ESP32 core 3.3.12** and the exact libraries in this repository's
`libraries/` folder:

| Library | Version | License |
|---|---|---|
| ArduinoJson | 7.4.3 | MIT |
| ArduinoUniqueID | 1.3.0 | MIT |
| AsyncTCP (ESP32Async) | 3.5.0 | LGPL-3.0 |
| ESPAsyncWebServer (ESP32Async) | 3.12.1 | LGPL-3.0 |
| OV5640 Auto Focus for ESP32 Camera | 0.1.1 | MIT |

> Use **these versions**. Newer versions of the ESP32 core or of ESPAsyncWebServer may not compile
> or may behave differently. Until version c15 the firmware used core 3.1.0 with ArduinoJson 7.3.0,
> AsyncTCP 3.3.1 and ESPAsyncWebServer 3.4.5.

The firmware supports only the XIAO ESP32-S3 Sense, its pins are in
`ESP32_PrusaConnectCam/module_XIAO_ESP32-S3-cam.h`. There is no board to select.

### Option A: Arduino IDE 2

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software).
2. **File → Preferences → Additional boards manager URLs**, add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Tools → Board → Boards Manager**: search for **esp32 by Espressif Systems** and install
   **version 3.3.12** (choose the version in the list before pressing Install).
4. Copy **the 5 folders** inside this repository's `libraries/` to the Arduino libraries folder:
   - Windows: `Documents\Arduino\libraries\`
   - Linux: `~/Arduino/libraries/`
   - macOS: `~/Documents/Arduino/libraries/`

   (If you already have different versions of these libraries there, remove them first.)
5. Open `ESP32_PrusaConnectCam/ESP32_PrusaConnectCam.ino`.
6. In **Tools**, choose:

   | Option | Value |
   |---|---|
   | Board | **XIAO_ESP32S3** |
   | USB CDC On Boot | Enabled |
   | CPU Frequency | 160MHz (WiFi) |
   | Core Debug Level | None |
   | USB DFU On Boot | Disabled |
   | Erase All Flash Before Sketch Upload | **Enabled only the first time**, then Disabled |
   | Events Run On | Core 1 |
   | Flash Mode | QIO 80MHz |
   | Flash Size | 8MB |
   | JTAG Adapter | Disabled |
   | Arduino Runs On | Core 1 |
   | USB Firmware MSC On Boot | Disabled |
   | Partition Scheme | **Default with spiffs (3MB APP/1.5MB SPIFFS)** |
   | PSRAM | **OPI PSRAM** |
   | Upload Mode | UART0 / Hardware CDC |
   | Upload Speed | 921600 |
   | USB Mode | Hardware CDC and JTAG |

7. Connect the XIAO over USB-C, choose the port in **Tools → Port**, and press **Upload**.

> If you leave "Erase All Flash" enabled, you **erase the configuration** (Wi-Fi, token) on every
> upload. The token is bound to the board by the fingerprint, so erasing the flash does not break
> the registration, but you will have to set up Wi-Fi and the token again.

### Option B: VS Code (with arduino-cli in the terminal)

Works on Linux, macOS and Windows. This is how the build was tested.

1. Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/) (tested with version 1.1.1).
2. In the VS Code terminal, once:
   ```bash
   arduino-cli config init
   arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install esp32:esp32@3.3.12
   ```
   No need to install libraries: the script uses the ones in `libraries/`.
3. Compile:
   - **Linux / macOS:**
     ```bash
     ./build_xiao.sh
     ```
   - **Windows (PowerShell)**, the same without the script:
     ```powershell
     arduino-cli compile -b "esp32:esp32:XIAO_ESP32S3:USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=160,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,DebugLevel=none,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default" --libraries libraries --output-dir build/xiao-esp32-s3 ESP32_PrusaConnectCam
     ```
4. The `.bin` files are in `build/xiao-esp32-s3/`. To flash them, see [how to flash](../README.md#flash).

You can also flash directly with arduino-cli (change the port):
```bash
arduino-cli upload -p /dev/ttyACM0 -b esp32:esp32:XIAO_ESP32S3 --input-dir build/xiao-esp32-s3 ESP32_PrusaConnectCam
```

### If you change the web page

The web page files are in `webpage/`. After changing them, or after changing `SW_VERSION` in
`mcu_cfg.h`, regenerate `WebPage.h`:
```bash
python3 webpage/webpage_h_generator.py
```
It copies the web files into `WebPage.h` and sets the `?v=` version in `webpage/*.html` (browser
cache) to `SW_VERSION`. `--check` only reports what is out of date. Build **after** the version change.

### New version

1. Change `SW_VERSION` in `mcu_cfg.h` and run `python3 webpage/webpage_h_generator.py`.
2. Update the one-line [version history](../README.md#version-history).
3. Build, run the [web page test](#tests), commit and push.
4. Publish a GitHub release `v1.1.2-cNN` with the four `.bin` files and their `SHA256SUMS.txt`
   (the binaries are not kept in git).

### Tests

| Test | What it checks | How to run |
|---|---|---|
| `test/web/` | All web pages in headless Chromium with fake camera data: values shown, switches, photo size estimate and the 1 MB warning, Wi-Fi table, tabs, collapsible sections, no JavaScript errors. With `SCREENSHOTS=<folder>` it saves screenshots of the estimate | Install Playwright (see `test/web/run.sh`), then `PYTHON=<venv>/bin/python test/web/run.sh` |

---

<a name="changed-files"></a>
## Changed files

| File | Change |
|---|---|
| `ESP32_PrusaConnectCam/camera.cpp`, `camera.h` | Bugs 1, 3, 7. Safe copy of the photo (`GetPhotoCopy`), semaphore in every operation on the buffers. (c3) Sensor detection, resolution table, safe start. (c5) OV5640 autofocus. (c6) Stream capture task |
| `ESP32_PrusaConnectCam/exif.cpp`, `exif.h` | (c3) EXIF model from the detected sensor |
| `ESP32_PrusaConnectCam/connect.cpp`, `connect.h`, `connect_types.h` | Bugs 2, 4, 5, 6, 10. New Camera Service API, backoff, error codes. (c6) HTTPS keep-alive. (c7) Camera health in the name, upload counters. (c11) Size, resolution and quality of the last photo |
| `ESP32_PrusaConnectCam/Certificate.h` | GTS Root R4 and R1 certificates |
| `ESP32_PrusaConnectCam/WebServer.cpp`, `WebServer.h` | Bugs 8, 9. Token and interval validation, `/set_prusalink` route, new status fields. (c2) Page caching and sending. (c8) Firmware update from file, with checks of the file before writing |
| `ESP32_PrusaConnectCam/WebStream.cpp`, `WebStream.h` | Buffer over-reads, clean end of the stream, thermal guard. (c6) Sends the frames of the capture task |
| `ESP32_PrusaConnectCam/wifi_mngt.cpp` | Bug 11 |
| `ESP32_PrusaConnectCam/system.cpp`, `system.h` | Thermal guard, interval calculation, PrusaLink. (c2) TLS timeouts, verbose logs. (c7) Low memory check, restart reasons, camera health |
| `ESP32_PrusaConnectCam/prusalink.cpp`, `prusalink.h` | **New**: PrusaLink client |
| `ESP32_PrusaConnectCam/cfg.cpp`, `cfg.h` | Saving the interval and the PrusaLink settings, token masked in the log. (c4) Detected sensor. (c7) Unplanned restart counter |
| `ESP32_PrusaConnectCam/log.cpp`, `log.h`, `micro_sd.cpp`, `micro_sd.h` | (c2) Batched SD card log writes |
| `ESP32_PrusaConnectCam/mcu_cfg.h` | XIAO board selected, version, new constants, Core One defaults |
| `ESP32_PrusaConnectCam/var.cpp`, `var.h` | Thermal guard state. (c7) Health state, restart reason in RTC memory |
| `ESP32_PrusaConnectCam/ESP32_PrusaConnectCam.ino` | Photo task on core 1, load the PrusaLink configuration |
| `ESP32_PrusaConnectCam/Certificate_ota.h`, cloud update code | **Removed** (c7) |
| `module_*.h` of the other boards, `module_templates.h`, `jquery.h`, LED flash and light code, unused photo copy functions, `gtac`/`privacypolicy`/`cookies` pages, `fw_build.sh`, `doc/` of the other boards | **Removed** (c10). The 3 EEPROM bytes of the flash stay reserved, the settings after them keep their address |
| `ESP32_PrusaConnectCam/ExternalTemperatureSensor*`, `libraries/DHTNEW`, `module_*.h` DHT pins, DHT wiring images in `doc/` | **Removed** (c9). The EEPROM bytes of the sensor stay reserved, the settings after them keep their address |
| `webpage/*`, `ESP32_PrusaConnectCam/WebPage.h` | Interval list, PrusaLink section, sensor and autofocus, Health section, firmware update from file, versioned CSS and JS. (c9) Temperature page removed. (c11) Estimated photo size |
| `libraries/` | **New**: libraries with the exact versions |
| `build_xiao.sh` | **New**: build script |
| `firmware/` | Compiled firmware of c1–c9. **Removed** (c10): the binaries are in the GitHub releases |
| `test/web/` | **New** (c10): browser test of the web pages. (c11) Photo size estimate. (c12) Smallest resolution, Saved confirmation, status LED, UTF-8 check mark, footer. (c13) Stream page messages when the camera is hot |

To see every change line by line against the original:
```bash
git diff 136c91c -- ESP32_PrusaConnectCam webpage
```

---

<a name="upstream"></a>
## Pulling updates from the original project

This repository has the Prusa project configured as `upstream`:

```bash
git remote add upstream https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam.git   # if it does not exist yet
git fetch upstream
git merge upstream/master
```

If there are conflicts in the files changed by this fork, resolve them keeping the fixes described
above.
