# Prusa ESP32 Cam: XIAO ESP32-S3 Sense edition for the Prusa Core One

This repository is a **fork** of the official firmware
[prusa3d/Prusa-Firmware-ESP32-Cam](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam)
(base version **v1.1.2**, commit `136c91c`). The full original history is kept and our
changes are in the commits on top of it.

Goal: a camera you can rely on every day, on a **Prusa Core One**, with a
**Seeed Studio XIAO ESP32-S3 Sense** (OV2640 camera).

Firmware version: **1.1.2-c5** (see the [version history](#version-history)).

> **Important:** this firmware **compiles**, but it has **not been tested on a real board yet**.
> The first tests on your printer are the final check. Read
> [Known limitations](#11-known-limitations).

---

## Contents

1. [What we did (summary)](#1-what-we-did-summary)
2. [The 11 bugs fixed](#2-the-11-bugs-fixed)
3. [Migration to the Prusa Camera Service](#3-migration-to-the-prusa-camera-service)
4. [What the "Core One defaults" are](#4-what-the-core-one-defaults-are)
5. [What the "thermal guard" is](#5-what-the-thermal-guard-is)
6. [PrusaLink: frequent photos only while printing](#6-prusalink-frequent-photos-only-while-printing)
7. [Other improvements](#7-other-improvements)
8. [Optimizations in version 1.1.2-c2](#8-optimizations-in-version-112-c2)
8b. [Camera sensors: OV2640, OV3660, OV5640](#8b-camera-sensors-ov2640-ov3660-ov5640)
9. [How to compile (Arduino IDE or VS Code)](#9-how-to-compile)
10. [How to flash the firmware](#10-how-to-flash-the-firmware)
11. [Known limitations](#11-known-limitations)
12. [First setup](#12-first-setup)
13. [Changed files](#13-changed-files)
14. [Version history](#version-history)
15. [Pulling updates from the original project](#15-pulling-updates-from-the-original-project)

---

## 1. What we did (summary)

1. **Reviewed the whole code** of the original firmware and found 11 real bugs that, in long
   use (prints of many hours), cause hangs, reboots, lost photos or security problems.
   **All 11 are fixed.**
2. **Migrated to the new Prusa API** (`camera-service.prusa3d.com`), following the official
   documentation: new data format, new TLS certificates, the allowed upload intervals and the
   correct handling of each error code.
3. **Factory settings for the Core One** (better resolution, flash off).
4. **Thermal guard**: if the chip gets too hot inside the printer's closed chamber, the camera
   slows down by itself.
5. **Optional integration with the printer's PrusaLink**: photos every 10 s while printing,
   one photo every 10 min when the printer is idle.
6. **Reproducible build**: the exact libraries are in the `libraries/` folder, the
   `build_xiao.sh` script builds the firmware, and the compiled files are in `firmware/xiao-esp32-s3/`.
7. **A second review (version 1.1.2-c2)**: more fixes and optimizations, see [section 8](#8-optimizations-in-version-112-c2).
8. **OV2640, OV3660 and OV5640 camera sensors (version 1.1.2-c3)**: detected automatically, see [section 8b](#8b-camera-sensors-ov2640-ov3660-ov5640).

---

## 2. The 11 bugs fixed

| # | Where | Problem | What happened | Fix |
|---|------|---------|---------------|-----|
| 1 | `camera.cpp` (stream) | When a frame capture failed during the stream, the code returned **without releasing the semaphore** (the lock of the camera buffer) | The photo task was blocked forever and the watchdog **rebooted the board** about 40 s later | The semaphore is always released, and the stream ends cleanly when a capture fails |
| 2 | `connect.cpp` | Reading the server response had **no time limit** and kept the CPU busy in a loop | On a slow or unstable network: hang and watchdog reboot | 10 s limit, with 10 ms pauses while waiting |
| 3 | `camera.cpp` | When memory ran out, the buffer was freed but the pointer still pointed to it | Double free on the next frame, memory corruption and crash | Pointer cleared, and the buffer is reused instead of being allocated for every frame |
| 4 | `connect.cpp` | Uploading during the stream used a buffer that could be **NULL** | Crash when uploading a photo right after the stream was opened | The upload now uses its own copy of the photo (see bug 8) |
| 5 | `connect.cpp` | When the photo size was an **exact multiple of 2048 bytes**, the last 2 KB were not sent | The server rejected the photo (wrong size). Rare, but it happened | Upload loop rewritten, it also handles partial writes |
| 6 | `connect.cpp` | **4 extra bytes** were sent after the HTTP request body | HTTP protocol violation (the new server is stricter) | Removed |
| 7 | `camera.cpp` | Retrying a bad photo **did not work** (the bad frame was not returned to the driver), and on that path the **flash stayed on** | Failed photos and a flash LED that stayed lit | The bad frame is returned before the next attempt, and the flash is always switched off |
| 8 | `WebServer.cpp` | `/saved-photo.jpg` marked the photo as "free" **before** the browser finished downloading it | Corrupted or mixed image if the camera took another photo in the meantime | The web server sends a **copy** of the photo, freed only when the transfer ends |
| 9 | `WebServer.cpp` | The **stream** `/stream.mjpg` **ignored the login password** | Anyone on your network could watch the printer, even with login enabled | The stream now asks for the login like every other page |
| 10 | `connect.cpp`, `cfg.cpp` | The Prusa Connect **token** was written to the log, which is stored on the SD card | Token leak (anyone with the token can upload images to your account) | The log shows only the token length |
| 11 | `wifi_mngt.cpp` | When the Prusa server was down, the camera **disconnected and reconnected Wi-Fi every ~28 s** | Local stream interrupted and an unstable network, for a problem that was not Wi-Fi at all | Increasing wait between uploads (*backoff*). Wi-Fi is reconnected only after 10 failed connections in a row |

**Also fixed:** two more buffer over-reads (`AsyncBufferResponse` and `AsyncFrameResponse`
copied too many bytes), and the camera can no longer be re-initialized in the middle of a
capture (when you change the resolution in the browser).

**One of my findings was wrong:** during the review I said that the XIAO status LED polarity
was inverted. Checking the code (`sys_led.cpp`) showed that the original behavior is correct
(short blink, as documented). **It was not changed.**

---

## 3. Migration to the Prusa Camera Service

Prusa created a new service for cameras: <https://camera-service.prusa3d.com/docs/>.
The old addresses (`connect.prusa3d.com/c/snapshot` and `/c/info`) still work, but the
documentation says new code should use the new ones. What changed:

| New API rule | Before | Now |
|---|---|---|
| `/v1/snapshot` and `/v1/camera-info` on `camera-service.prusa3d.com` | `/c/snapshot` and `/c/info` on `connect.prusa3d.com` | New addresses. Old configurations with `connect.prusa3d.com` are **migrated automatically** |
| The server certificate is issued by Google Trust Services | Trusted only ISRG Root X1 (Let's Encrypt), so **the connection would fail** | Added GTS Root R4 and GTS Root R1 (ISRG Root X1 kept). Verified against the real server |
| Camera info with `device: {type: "ESP32", firmware_version, hardware_version}`. Unknown fields are **rejected** | Old format, which would be rejected (error 422) | New format, exactly as documented |
| Camera name of at most 32 characters | Up to 40 | Cut to 32 |
| `User-Agent` with name and version | `ESP32-CAM` | `PrusaESP32Cam/1.1.2-c5` |
| Only 4 intervals: 10 s, 30 s, 60 s, 10 min. At most 6 uploads per minute | Any value from 10 to 240 s | A list with the 4 values. Old values are converted to the nearest one |
| Photos of at most 1 MB | Not checked | Checked before uploading |
| Each error code has its own action | All errors handled the same way | See below |

**What the camera does for each server response:**

- **200**: photo accepted.
- **429 / 503 / 500 / no connection**: temporary problem. Increasing wait: the normal
  interval, then double, up to a maximum of 10 min. Back to normal after a successful upload.
- **401 / 403 / 422 / other 4xx**: the request is wrong (for example an invalid token).
  Retries only every 10 min, until you change the token.
- **413**: photo too large. A message asks you to lower the resolution or the quality.

**Two important rules of the new service:**

1. **15-minute window:** after you add the camera in Prusa Connect, the camera must make its
   first upload **within 15 minutes**, otherwise the token stops working.
   When you save the token, the camera sends its data immediately (it does not wait for the interval).
2. **Fingerprint:** the first upload binds the token to the board's "fingerprint" forever.
   The firmware computes it from the chip ID and the MAC address, so it **survives a factory
   reset**. It is about 48 characters long (the limit is 16 to 64).

The token is validated on the web page: it must have exactly **20 characters A-Z, a-z, 0-9**.

---

## 4. What the "Core One defaults" are

These are the **factory settings**: the settings the camera uses on its first start (or after
a factory reset). They were chosen for the Core One:

| Setting | Original | Now | Why |
|---|---|---|---|
| Resolution | 320×240 (QVGA) | **1024×768 (XGA)** | 320×240 is too small to see details of the part. XGA shows the part well and stays far below the 1 MB limit |
| JPEG quality | 10 | **12** | Slightly smaller files and faster uploads, with almost no visible difference (scale 10–63: lower = better) |
| Server | `connect.prusa3d.com` | **`camera-service.prusa3d.com`** | The new API |
| Interval | 30 s | 30 s | Same |
| Flash LED | off | off | The Core One has its own LED lighting in the print chamber. The XIAO has no flash on the board anyway |
| White balance | automatic | **automatic** | During the review I suggested a fixed mode for LED light. Without testing under the real light of your Core One, automatic is safer. You can change it in *Advanced settings* after seeing the images |

You can change everything later on the camera's web page. These values only apply on the
first start or after a factory reset.

---

## 5. What the "thermal guard" is

**The problem:** the XIAO ESP32-S3 is tiny and gets very hot (the project's own
documentation warns about it). Inside the Core One, which is **enclosed** and has a heated
print chamber (ASA, ABS, PC...), the surrounding air is already hot. With Wi-Fi and the video
stream running, the chip can overheat: noisy images, unstable Wi-Fi, reboots or long-term damage.

**The solution:** the camera reads the chip's internal temperature every second and:

| Chip temperature | What happens |
|---|---|
| **≥ 75 °C** | Protection on: the **stream is stopped** (new streams get a 503 answer), and photos to Prusa Connect are limited to **at most 1 per minute** |
| **≤ 68 °C** | Back to normal |

The gap between 75 and 68 (*hysteresis*) prevents the protection from switching on and off
every second when the temperature is near the limit. On the **System** page, the temperature
shows a "TOO HOT" warning while the protection is active.

The limits are in `mcu_cfg.h` (`THERMAL_THROTTLE_TEMP`, `THERMAL_RESUME_TEMP`,
`THERMAL_THROTTLE_INTERVAL`). This is the chip's **internal** temperature, which is always
higher than the air temperature. The sensor is accurate up to 80 °C. If the protection
switches on too early or too late in your tests, adjust these values.

**Physical recommendations for the Core One:**
- If possible, mount the camera **outside the heated chamber** (looking through the door or the glass), or at a spot with moving air.
- Stick a **small aluminium heatsink** on the back of the XIAO.
- The Core One's steel frame blocks Wi-Fi: place the **external antenna outside** the metal frame.

---

## 6. PrusaLink: frequent photos only while printing

**Optional, off by default.** The Core One has a local API (PrusaLink) that tells whether it
is printing. With this feature on:

| Printer state | Photos to Prusa Connect |
|---|---|
| Printing, paused, waiting for attention, or busy (calibration...) | At the interval you chose (for example 10 s) |
| Idle, or print finished | **1 photo every 10 min** |
| A print starts or ends | **Immediate photo** (you see the final result right away) |
| Printer does not respond | Normal interval (as if the feature were off) |

Benefits: less heat, less traffic, and frequent photos only when they matter.

**How to set it up** (**Camera** page, *PrusaLink printer state* section):
1. **Printer IP address**: the Core One's IP (on the printer screen: *Settings → Network*).
2. **PrusaLink user**: usually `maker`.
3. **PrusaLink password**: on the printer screen, *Settings → Network → PrusaLink*.
4. Turn the switch on and press **Save**. The *PrusaLink status* field should show `OK`,
   and *Printer state* the current state.

The camera requests `GET /api/v1/status` every 10 s, with *digest* authentication (the one
PrusaLink uses). While the printer does not respond, it asks only every 60 s. The password is
never sent to the web page.

---

## 7. Other improvements

- **Capture and upload on core 1:** the ESP32-S3 has 2 cores. Before, everything ran on core 0
  (the same core as Wi-Fi) and core 1 was idle. The photo, TLS and PrusaLink task moved to core 1.
- **Only one stream client at a time:** the camera has a single image buffer, and two streams
  "stole" frames from each other (which caused the corruption problems). A second stream gets
  the message *"The stream is already open in another window"*.
- **Main page:** shows the **current upload interval** (it already includes the backoff, PrusaLink and the thermal guard).
- **System page:** shows the current upload interval.
- **Clear error messages** on the page (for example, for a 403 it explains the 15-minute window).

---

## 8. Optimizations in version 1.1.2-c2

A second review of the code, looking for problems and unnecessary work.

**Problems fixed**

| # | Problem | What happened | Fix |
|---|---|---|---|
| 1 | The start of the HTTPS connection (TLS *handshake*) could wait up to **120 s**, but the watchdog reboots the board at **40 s** | With weak Wi-Fi (the Core One's steel frame), a slow connection rebooted the camera in the middle of an upload | 15 s limit (`TLS_HANDSHAKE_TIMEOUT`), also for cloud updates |
| 1b | The cloud update timeout was **12 ms** instead of 12 s: in the ESP32 3.x core the value is in milliseconds | The cloud update could fail | 12000 ms |
| 2 | Web pages were **cached for 24 h** by the browser | After a firmware update, the browser showed the old pages (without the PrusaLink section, for example) | HTML pages are no longer cached. `styles.css` carries the firmware version in its URL |
| 3 | Sending a page measured the whole file for every chunk sent | jQuery (87 KB) caused about 5 MB to be read per page load, and the page opened slowly | Size measured once, and the response now carries `Content-Length` |
| 4 | If Prusa rejected the camera info (4xx error), the camera repeated it every cycle | Useless requests to the server, and the wait between photo uploads got mixed up | Sent again only after you change the configuration. Photos follow their own logic |
| 5 | With the printer switched off, PrusaLink blocked the photo task for about 3 s every 10 s | Delayed uploads | While the printer does not respond, it asks only every 60 s (`PRUSALINK_RETRY_INTERVAL`). After a configuration change, it asks right away |

**Optimizations**

| # | Before | Now | Gain |
|---|---|---|---|
| 6 | Photo sent in 2 KB chunks (about 60 TLS records) | 16 KB chunks, the maximum of one TLS record (about 8 records) | Faster upload, less CPU and fewer bytes sent |
| 7 | Every log line forced a write to the SD card: about **86,000 per day** with photos every 10 s. The other tasks waited for each write | Warnings and errors are written immediately. The rest is written at most every 5 s (`LOGS_FLUSH_INTERVAL`) and by the SD card task. The routine messages of each photo moved to the Verbose level (one Info line per upload remains, with the result) | Less SD card wear, less waiting between tasks |
| 8 | The debug messages of the task loops were built even when they were not logged (the LED task up to 10 times per second) | Built only when the Verbose level is active | Less fragmentation of the internal memory after weeks of uptime |

Also fixed: the "Save network ip method" log always showed "DHCP", because of an operator
precedence error.

**Left for after the first test on the board** (they need real measurements):
- reuse the HTTPS connection between uploads (*keep-alive*), instead of opening a new one for every photo;
- more frames per second (FPS) in the stream: remove the 1 ms pause per chunk sent, and copy the image for Prusa Connect only when it is needed;
- two image buffers in the camera instead of one.

---

## 8b. Camera sensors: OV2640, OV3660, OV5640

Seeed has shipped the XIAO ESP32-S3 Sense with three different camera sensors: first the
OV2640 (now discontinued), then the OV3660, and now the OV5640 with autofocus. During the
transition, boards can come with either the OV3660 or the OV5640
([Seeed wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)). All three use the
same connector and the same pins.

**The sensor is detected automatically.** The camera driver reads the sensor ID at start-up,
so there is nothing to select. The **Camera** page shows the detected sensor ("Camera sensor").

**Detected once, then saved.** On the first start the sensor is detected and saved in the
EEPROM. Every later start initializes the camera only once, directly with the XCLK and the
resolution limit of the saved sensor. The driver reads the sensor ID on every start anyway (it
costs nothing extra), so if you change the camera module, the new sensor is detected, saved,
and the camera is initialized a second time only if the XCLK is different.

| Sensor | Max. resolution | XCLK | Notes |
|---|---|---|---|
| OV2640 | 1600×1200 | 15 MHz (unchanged) | |
| OV3660 | 2048×1536 | 20 MHz (Espressif reference value) | |
| OV5640 | 2592×1944 | 20 MHz (Espressif reference value) | Continuous autofocus, see below |

**What the firmware does for each sensor:**
- **Resolutions:** the resolution list on the Camera page shows only the resolutions the
  detected sensor supports. New resolutions: 1280×720, 1920×1080, 2048×1536 and 2592×1944.
  The saved resolution index stays compatible with older versions.
- **Safe start:** a resolution above the sensor maximum cannot break the start, because the
  camera driver limits it to the sensor maximum (`esp_camera.c`). If you saved 2592×1944 with an
  OV5640 and later fit an OV2640, the saved resolution is lowered to the OV2640 maximum.
- **Settings:** every sensor setting is applied only if the sensor driver implements it.
- **Photo metadata:** the EXIF "Model" field and the camera info sent to Prusa Connect contain
  the detected sensor name (before, the EXIF always said OV2640).
- **Resolution sent to Prusa Connect:** taken from the camera driver. Before, 400×296 was
  reported as 352×288.

**Practical advice:**
- The Prusa Camera Service accepts photos up to **1 MB**. At 2048×1536 and above, keep the
  image quality moderate (the default 12 or higher). Photos over 1 MB are not uploaded and the
  status shows a clear message.
- Very high JPEG quality at high resolutions can make a photo larger than the camera's frame
  buffer (the driver logs `FB-OVF`). The quality slider stops at 10, which avoids the worst cases.
- **OV5640 autofocus (version 1.1.2-c5).** After every camera start the firmware loads the
  OmniVision autofocus firmware (4 KB) into the OV5640 and starts **continuous autofocus**. The
  Camera page shows the autofocus state (focused / focusing / idle). It uses the library
  [OV5640 Auto Focus for ESP32 Camera](https://github.com/0015/ESP32-OV5640-AF) 0.1.1 (MIT), the
  same approach as Seeed's examples.
  - *Why not a newer ESP32 core?* The esp32-camera driver has an autofocus API
    (`esp_camera_af.h`), but the official Arduino core builds have it switched off: in the newest
    core (3.3.12, checked 2026-10) `CONFIG_CAMERA_AF_SUPPORT` is not set, so every call returns
    `ESP_ERR_NOT_SUPPORTED`. Updating the core would not give autofocus.
  - Loading the firmware takes about 1–2 s and is repeated after every camera re-init (for
    example when you change the resolution), because the sensor loses it.
  - OV2640 and OV3660 have a fixed-focus lens: the Camera page shows "Not supported".
- If the image is upside down or mirrored, use **Vertical flip** / **Horizontal mirror** on the
  Camera page. The firmware does not flip any sensor by itself.

---

## 9. How to compile

The firmware compiles with the **ESP32 core 3.1.0** and the exact libraries in this
repository's `libraries/` folder:

| Library | Version | License |
|---|---|---|
| ArduinoJson | 7.3.0 | MIT |
| ArduinoUniqueID | 1.3.0 | MIT |
| AsyncTCP (mathieucarbou) | 3.3.1 | LGPL-3.0 |
| ESPAsyncWebServer (mathieucarbou) | 3.4.5 | LGPL-3.0 |
| DHTNEW | 0.5.2 | MIT |
| OV5640 Auto Focus for ESP32 Camera | 0.1.1 | MIT |

> Use **these versions**. Newer versions of the ESP32 core or of ESPAsyncWebServer may not
> compile or may behave differently.

Before compiling, check in `ESP32_PrusaConnectCam/mcu_cfg.h` that only the XIAO is enabled
(this repository already has it like that):

```c
#define CAMERA_MODEL_XIAO_ESP32_S3_CAM  true   // all the others: false
```

### Option A: Arduino IDE 2

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software).
2. **File → Preferences → Additional boards manager URLs**, add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Tools → Board → Boards Manager**: search for **esp32 by Espressif Systems** and install
   **version 3.1.0** (choose the version in the list before pressing Install).
4. Copy **the 6 folders** inside this repository's `libraries/` to the Arduino libraries folder:
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

> If you leave "Erase All Flash" enabled, you **erase the configuration** (Wi-Fi, token) on
> every upload. The token is bound to the board by the fingerprint, so erasing the flash does
> not break the registration, but you will have to set up Wi-Fi and the token again.

### Option B: VS Code (with arduino-cli in the terminal)

Works on Linux, macOS and Windows. This is how the build was tested.

1. Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/) (tested with version 1.1.1).
2. In the VS Code terminal, once:
   ```bash
   arduino-cli config init
   arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install esp32:esp32@3.1.0
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
4. The `.bin` files are in `build/xiao-esp32-s3/`. To flash them, see section 10.

You can also flash directly with arduino-cli (change the port):
```bash
arduino-cli upload -p /dev/ttyACM0 -b esp32:esp32:XIAO_ESP32S3 --input-dir build/xiao-esp32-s3 ESP32_PrusaConnectCam
```

### If you change the web page

The web page files are in `webpage/`. After changing them, regenerate `WebPage.h`:
```bash
cd webpage && bash webpage_h_generator.sh
```

---

## 10. How to flash the firmware

The compiled files are in **`firmware/xiao-esp32-s3/`**. You do not need to compile to use
the camera.

**Easiest way (browser, Chrome or Edge):**
1. Open <https://espressif.github.io/esptool-js/>.
2. Baud rate 921600, press **Connect** and choose the XIAO's port.
3. The first time, press **Erase Flash**.
4. Add **a single file**: `ESP32_PrusaConnectCam.ino.merged.bin` at address **0x0**. Press **Program**.

**With esptool** (Linux/macOS/Windows), first flash (erases everything):
```bash
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 921600 --before default_reset --after hard_reset \
  write_flash --erase-all --flash_mode qio --flash_size 8MB --flash_freq 80m \
  0x0 ESP32_PrusaConnectCam.ino.bootloader.bin \
  0x8000 ESP32_PrusaConnectCam.ino.partitions.bin \
  0x10000 ESP32_PrusaConnectCam.ino.bin
```
To **update without losing the configuration**, use the same command **without** `--erase-all`.

If the board is not detected: hold the **B** (boot) button, plug in the USB cable and release the button.

---

## 11. Known limitations

- **Not tested on a real board.** It compiles without errors, but the real behavior (camera,
  Wi-Fi, temperature, PrusaLink) can only be confirmed on your Core One.
- **PrusaLink on the Core One:** the PrusaLink specification defines `/api/v1/status` and
  *digest* authentication. I did not confirm that the Core One firmware answers exactly like
  that. If *PrusaLink status* does not show `OK`, open an issue with the message shown.
- **Thermal limits (75 / 68 °C)** are an estimate for the chip's internal temperature.
  Adjust them after seeing real values on the System page.
- **Only one stream client** at a time (see section 7).
- **"Update from cloud"** (on the System page) downloads the **official** Prusa firmware,
  which **replaces** this version. To update this version, flash over USB or use the file
  update with the `.bin` from this repository.
- The only possible intervals are 10 s, 30 s, 60 s and 10 min (a Prusa rule).

---

## 12. First setup

> **Follow this order:** first the camera's Wi-Fi, and only then create the camera in Prusa
> Connect. This way you do not risk missing the token's 15-minute window.

1. After flashing, the camera creates a Wi-Fi network **`ESP32_camera_` followed by numbers** (password `12345678`).
2. Connect to that network and open <http://192.168.0.1>.
3. **Wi-Fi** page: choose your network, enter the password and save. The camera connects to
   your network (the IP is shown on the same page; `http://prusa-esp32cam.local` also works).
4. In **Prusa Connect** (<https://connect.prusa3d.com>): open your Core One, go to **Camera →
   Add new other camera**, and copy the **token**.
5. **Within the next 15 minutes**, paste the token on the camera's **Camera** page and press **Save**.
6. The first photo should appear in Prusa Connect within a few seconds.
7. **Recommended:** **Authentication** page, enable the login with your own password. It now protects the stream too.
8. Optional: set up **PrusaLink** (section 6).

**Factory reset:** connect pin **GPIO2** to GND, power on, wait 10 s until the LED blinks,
then disconnect GPIO2 from GND.

---

## 13. Changed files

| File | Change |
|---|---|
| `ESP32_PrusaConnectCam/camera.cpp`, `camera.h` | Bugs 1, 3, 7. Safe copy of the photo (`GetPhotoCopy`), semaphore in every operation on the buffers. (c3) Sensor detection, resolution table, safe start |
| `ESP32_PrusaConnectCam/exif.cpp`, `exif.h` | (c3) EXIF model from the detected sensor |
| `ESP32_PrusaConnectCam/connect.cpp`, `connect.h`, `connect_types.h` | Bugs 2, 4, 5, 6, 10. New Camera Service API, backoff, error codes |
| `ESP32_PrusaConnectCam/Certificate.h` | GTS Root R4 and R1 certificates |
| `ESP32_PrusaConnectCam/WebServer.cpp` | Bugs 8, 9. Token and interval validation, `/set_prusalink` route, new status fields. (c2) Page caching and sending |
| `ESP32_PrusaConnectCam/WebStream.cpp` | Buffer over-reads, clean end of the stream, thermal guard |
| `ESP32_PrusaConnectCam/wifi_mngt.cpp` | Bug 11 |
| `ESP32_PrusaConnectCam/system.cpp`, `system.h` | Thermal guard, interval calculation, PrusaLink. (c2) TLS timeouts, verbose logs |
| `ESP32_PrusaConnectCam/prusalink.cpp`, `prusalink.h` | **New**: PrusaLink client |
| `ESP32_PrusaConnectCam/cfg.cpp`, `cfg.h` | Saving the interval and the PrusaLink settings, token masked in the log |
| `ESP32_PrusaConnectCam/log.cpp`, `log.h`, `micro_sd.cpp`, `micro_sd.h` | (c2) Batched SD card log writes |
| `ESP32_PrusaConnectCam/mcu_cfg.h` | XIAO board selected, version 1.1.2-c4, new constants, Core One defaults |
| `ESP32_PrusaConnectCam/cfg.cpp`, `cfg.h` (c4) | Detected camera sensor saved in the EEPROM |
| `ESP32_PrusaConnectCam/var.cpp`, `var.h` | Thermal guard state |
| `ESP32_PrusaConnectCam/ESP32_PrusaConnectCam.ino` | Photo task on core 1, load the PrusaLink configuration |
| `webpage/*`, `ESP32_PrusaConnectCam/WebPage.h` | Interval list, PrusaLink section, current interval, versioned `styles.css` |
| `libraries/` | **New**: libraries with the exact versions |
| `build_xiao.sh` | **New**: build script |
| `firmware/xiao-esp32-s3/` | **New**: compiled firmware |

To see every change line by line against the original:
```bash
git diff 136c91c -- ESP32_PrusaConnectCam webpage
```

---

## Version history

| Version | Changes |
|---|---|
| 1.1.2-c1 | 11 bugs fixed, Prusa Camera Service, Core One defaults, thermal guard, PrusaLink |
| 1.1.2-c2 | TLS timeout, browser cache, page sending, rejected camera info, PrusaLink with the printer switched off, 16 KB TLS chunks, batched SD log writes, debug messages (see [section 8](#8-optimizations-in-version-112-c2)) |
| 1.1.2-c5 | OV5640 continuous autofocus, autofocus state on the Camera page |
| 1.1.2-c4 | Camera sensor detected once and saved: later starts initialize the camera only once |
| 1.1.2-c3 | OV2640, OV3660 and OV5640 detected automatically, resolutions per sensor up to 2592×1944, safe camera start, EXIF model, correct resolution sent to Prusa Connect (see [section 8b](#8b-camera-sensors-ov2640-ov3660-ov5640)) |

---

## 15. Pulling updates from the original project

This repository has the Prusa project configured as `upstream`:

```bash
git remote add upstream https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam.git   # if it does not exist yet
git fetch upstream
git merge upstream/master
```

If there are conflicts in the files we changed, resolve them keeping the fixes described above.
