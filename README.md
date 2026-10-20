# Prusa ESP32 Cam: XIAO ESP32-S3 Sense edition for the Prusa Core One

A monitoring camera for a **Prusa Core One**, built on a **Seeed Studio XIAO ESP32-S3 Sense**
(OV2640, OV3660 or OV5640 camera). It uploads photos to **Prusa Connect**, shows a live stream on
your local network, and runs for weeks inside the printer without supervision.

This repository is a **fork** of the official firmware
[prusa3d/Prusa-Firmware-ESP32-Cam](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam)
(base version **v1.1.2**, commit `136c91c`, by Miroslav Pivovarsky). The full original history is
kept, and the changes of this fork are in the commits on top of it.

Firmware version: **1.1.2-c9** (see the [version history](#version-history)).
The compiled firmware is in [`firmware/xiao-esp32-s3/`](firmware/xiao-esp32-s3/).

> **Important:** this firmware **compiles**, but it has **not been tested on a real board yet**.
> The first tests on your printer are the final check. Read the
> [known limitations](#known-limitations).

**Main differences from the original firmware:**

- 11 bugs fixed (hangs, reboots, lost photos, the token in the log, the stream without login);
- the new **Prusa Camera Service** API (`camera-service.prusa3d.com`);
- factory settings for the Core One, a **thermal guard** and optional **PrusaLink** integration
  (frequent photos only while printing);
- **OV2640, OV3660 and OV5640** detected automatically, continuous autofocus on the OV5640;
- faster stream (separate capture task, 2 clients) and HTTPS keep-alive for the uploads;
- health checks: automatic restart after low memory, camera problems shown in Prusa Connect;
- no update from the internet: update from a `.bin` file on the System page, or over USB;
- no external DHT temperature sensor: the Core One measures the chamber temperature itself;
- a reproducible build: exact libraries in `libraries/`, a build script, compiled binaries.

---

## Contents

**Using the camera**
- [Quick start](#quick-start)
- [How to flash the firmware](#flash)
- [First setup](#first-setup)
- [The web interface](#web-interface)
- [PrusaLink: frequent photos only while printing](#prusalink)
- [Thermal guard and mounting in the Core One](#thermal-guard)
- [Camera sensors and focus](#camera-sensors)
- [Service AP, status LED and factory reset](#service-ap)
- [Debug logs, serial console and web API](#debug-logs)
- [Troubleshooting](#troubleshooting)
- [Known limitations](#known-limitations)

**How it works**
- [The 11 bugs fixed](#bugs-fixed)
- [Migration to the Prusa Camera Service](#camera-service)
- [Core One factory settings](#core-one-defaults)
- [How the photo and stream paths work](#photo-stream-paths)
- [Speed and efficiency](#performance)
- [Health checks](#health-checks)
- [Firmware update from file](#firmware-update)

**Development**
- [How to compile (Arduino IDE or VS Code)](#compile)
- [Changed files](#changed-files)
- [Version history](#version-history)
- [Pulling updates from the original project](#upstream)
- [Other boards](#other-boards)

---

<a name="quick-start"></a>
## Quick start

1. Flash `firmware/xiao-esp32-s3/ESP32_PrusaConnectCam.ino.merged.bin` at address 0x0 from the
   browser ([how to flash](#flash)).
2. Connect to the Wi-Fi network `ESP32_camera_...` (password `12345678`), open
   <http://192.168.0.1> and set up your Wi-Fi.
3. Create the camera in Prusa Connect and paste its token on the **Camera** page
   **within 15 minutes** ([first setup](#first-setup)).
4. Optional: enable the login, and set up [PrusaLink](#prusalink).

---

<a name="flash"></a>
## How to flash the firmware

The compiled files are in **`firmware/xiao-esp32-s3/`**. You do not need to compile to use the
camera.

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

**Over Wi-Fi:** once the camera runs, later versions can be installed from the System page with
the file `ESP32_PrusaConnectCam.ino.bin` (see [firmware update from file](#firmware-update)).

If the board is not detected: hold the **B** (boot) button, plug in the USB cable and release the
button.

| File | Address | Use |
|---|---|---|
| `ESP32_PrusaConnectCam.ino.merged.bin` | 0x0 | USB, everything in one file |
| `ESP32_PrusaConnectCam.ino.bootloader.bin` | 0x0 | USB |
| `ESP32_PrusaConnectCam.ino.partitions.bin` | 0x8000 | USB |
| `ESP32_PrusaConnectCam.ino.bin` | 0x10000 | USB, or the System page over Wi-Fi |

---

<a name="first-setup"></a>
## First setup

> **Follow this order:** first the camera's Wi-Fi, and only then create the camera in Prusa
> Connect. This way you do not risk missing the token's 15-minute window.

1. After flashing, the camera creates a Wi-Fi network **`ESP32_camera_` followed by numbers**
   (the first numbers of the chip ID), password `12345678`. Your computer may complain about
   "No Internet" on that network: that is normal.
2. Connect to that network and open <http://192.168.0.1> (or `http://prusa-esp32cam.local`).
3. **Wi-Fi** page: choose your network, enter the password and press **Save & Connect**. The
   camera connects to your network (the IP is shown on the same page; `http://prusa-esp32cam.local`
   also works).
4. In **Prusa Connect** (<https://connect.prusa3d.com>): open your Core One, go to **Camera →
   Add new other camera**, and copy the **token**.
5. **Within the next 15 minutes**, paste the token on the camera's **Camera** page and press
   **Save**. The camera sends its data immediately.
6. The first photo should appear in Prusa Connect within a few seconds.
7. **Recommended:** **Authentication** page, enable the login with your own password (default
   `admin` / `admin`). It protects the stream too.
8. Optional: set up [PrusaLink](#prusalink).

**Remove the protective film from the lens.** New camera modules have a transparent film on the
lens, which makes the image blurry.

<img src="doc/cam_prot_film.jpg" width=40% height=40%>

---

<a name="web-interface"></a>
## The web interface

| Page | What you find there |
|---|---|
| Main page | Latest photo, live stream, current upload interval (it already includes the backoff, PrusaLink and the thermal guard) |
| **Camera** | Token, upload interval (10 s, 30 s, 60 s or 10 min), resolution (only the ones your sensor supports), quality, image settings, flash, detected sensor, autofocus state, PrusaLink |
| **Wi-Fi** | Network, static IP, service AP on/off |
| **Authentication** | Login for the web page and the stream |
| **System** | Prusa Connect status, uptime, temperature, [Health](#health-checks) section, camera name (mDNS), log level and logs, micro SD card, [firmware update from file](#firmware-update) |

Error messages explain what to do, for example a 403 from Prusa Connect explains the
15-minute window.

---

<a name="prusalink"></a>
## PrusaLink: frequent photos only while printing

**Optional, off by default.** The Core One has a local API (PrusaLink) that tells whether it is
printing. With this feature on:

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
4. Turn the switch on and press **Save**. The *PrusaLink status* field should show `OK`, and
   *Printer state* the current state.

The camera requests `GET /api/v1/status` every 10 s, with *digest* authentication (the one
PrusaLink uses). While the printer does not respond, it asks only every 60 s. The password is
never sent to the web page.

---

<a name="thermal-guard"></a>
## Thermal guard and mounting in the Core One

**The problem:** the XIAO ESP32-S3 is tiny and gets very hot. Inside the Core One, which is
**enclosed** and has a heated print chamber (ASA, ABS, PC...), the surrounding air is already hot.
With Wi-Fi and the video stream running, the chip can overheat: noisy images, unstable Wi-Fi,
reboots or long-term damage.

**The solution:** the camera reads the chip's internal temperature every second and:

| Chip temperature | What happens |
|---|---|
| **≥ 75 °C** | Protection on: the **stream is stopped** (new streams get a 503 answer), and photos to Prusa Connect are limited to **at most 1 per minute**. Prusa Connect shows the camera as `[HOT]` |
| **≤ 68 °C** | Back to normal |

The gap between 75 and 68 (*hysteresis*) prevents the protection from switching on and off every
second near the limit. On the **System** page, the temperature shows a "TOO HOT" warning while
the protection is active, and the Health section shows the highest temperature since the start.

The limits are in `mcu_cfg.h` (`THERMAL_THROTTLE_TEMP`, `THERMAL_RESUME_TEMP`,
`THERMAL_THROTTLE_INTERVAL`). This is the chip's **internal** temperature, which is always higher
than the air temperature. The sensor is accurate up to 80 °C. If the protection switches on too
early or too late in your tests, adjust these values.

**Mounting:**
- If possible, mount the camera **outside the heated chamber** (looking through the door or the
  glass), or at a spot with moving air.
- Stick a **small aluminium heatsink** on the back of the XIAO.
- The Core One's steel frame blocks Wi-Fi: place the **external antenna outside** the metal frame.
- The camera module and the processor board are joined by a connector. If the camera or the SD
  card is not detected, check that the boards are pressed together well.
- Power: 5 V over USB-C, from a good supply (the original documentation asks for one rated for 2 A).

---

<a name="camera-sensors"></a>
## Camera sensors and focus

Seeed has shipped the XIAO ESP32-S3 Sense with three different camera sensors: first the OV2640
(now discontinued), then the OV3660, and now the OV5640 with autofocus. During the transition,
boards can come with either the OV3660 or the OV5640
([Seeed wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)). All three use the same
connector and the same pins.

**The sensor is detected automatically**, so there is nothing to select. The **Camera** page shows
the detected sensor. On the first start the sensor is saved in the EEPROM, and later starts
initialize the camera only once, directly with the right settings. If you change the camera
module, the new sensor is detected and saved.

| Sensor | Max. resolution | XCLK | Focus |
|---|---|---|---|
| OV2640 | 1600×1200 | 15 MHz | Fixed lens |
| OV3660 | 2048×1536 | 20 MHz | Fixed lens |
| OV5640 | 2592×1944 | 20 MHz | **Continuous autofocus** |

**What the firmware does for each sensor:**
- **Resolutions:** the Camera page shows only the resolutions the detected sensor supports. New
  resolutions: 1280×720, 1920×1080, 2048×1536 and 2592×1944.
- **Safe start:** a saved resolution above the sensor maximum is lowered to the sensor maximum
  by the camera driver.
- **Settings:** every sensor setting is applied only if the sensor driver implements it.
- **Metadata:** the EXIF "Model" field and the camera info sent to Prusa Connect contain the
  detected sensor name.

**Practical advice:**
- The Prusa Camera Service accepts photos up to **1 MB**. At 2048×1536 and above, keep the image
  quality moderate (the default 12 or higher). Photos over 1 MB are not uploaded and the status
  shows a clear message.
- Very high JPEG quality at high resolutions can make a photo larger than the frame buffer (the
  driver logs `FB-OVF`). The quality slider stops at 10, which avoids the worst cases.
- If the image is upside down or mirrored, use **Vertical flip** / **Horizontal mirror** on the
  Camera page. The firmware does not flip any sensor by itself.

**OV5640 autofocus.** After every camera start the firmware loads the OmniVision autofocus firmware
(4 KB) into the OV5640 and starts **continuous autofocus**. The Camera page shows the autofocus
state (focused / focusing / idle). It uses the library
[OV5640 Auto Focus for ESP32 Camera](https://github.com/0015/ESP32-OV5640-AF) 0.1.1 (MIT), the same
approach as Seeed's examples.
- *Why not a newer ESP32 core?* The esp32-camera driver has an autofocus API (`esp_camera_af.h`),
  but the official Arduino core builds have it switched off: in the newest core (3.3.12, checked
  2026-10) `CONFIG_CAMERA_AF_SUPPORT` is not set, so every call returns `ESP_ERR_NOT_SUPPORTED`.
- Loading the autofocus firmware takes about 1–2 s and is repeated after every camera re-init (for
  example when you change the resolution), because the sensor loses it.

**Manual focus (OV2640, OV3660).** These lenses are usually focused well. They can be focused by
hand with a [focus wrench](https://www.printables.com/cs/model/877739-esp32-cam-ov2640-focus-adjustment-wrench):
hold the camera, put the wrench on the lens and turn it a few degrees at a time.
**Manual focusing can permanently damage the camera module.**

<img src="doc/focus_2.jpg" width=40% height=40%>

---

<a name="service-ap"></a>
## Service AP, status LED and factory reset

**Service AP.** After every start the camera opens its own Wi-Fi network **`ESP32_camera_UID`**
(password `12345678`, IP **192.168.0.1**, or `http://prusa-esp32cam.local`), for the first setup.
When a Wi-Fi network is configured, the camera also connects to it, and the service AP switches
itself off **5 minutes** after the start if no device is connected to it (and no stream is open).
You can switch the service AP off completely on the Wi-Fi page. If the service AP does not work for
you, use the [serial console](#serial-console).

**Status LED** (GPIO21, the small LED on the XIAO). It lights up at power on, then blinks:

| Blink interval | Meaning |
|---|---|
| 400 ms | Service AP only |
| 800 ms | Connecting to Wi-Fi |
| 4000 ms | Connected to Wi-Fi |
| 100 ms | A problem |

The camera needs about 15–20 s to start.

**Factory reset:**
1. Connect pin **GPIO2** to **GND**.
2. Power on and wait about 10 s, until the LED blinks.
3. Disconnect GPIO2 from GND. The LED stops blinking and the configuration is back to the factory
   settings (also the unplanned restart counter).

The board's fingerprint for Prusa Connect is computed from the chip, so it survives a factory reset.

**External flash LED.** The XIAO has no flash LED. An external LED (or an LED strip with a relay)
can be connected to **GPIO4**. The Core One has its own chamber lighting, so the flash is off by
default.

More board details (pinout, schematics, photos): [doc/XIAO_ESP32S3](doc/XIAO_ESP32S3/README.md)
(original documentation, it describes the original firmware).

---

<a name="debug-logs"></a>
## Debug logs, serial console and web API

**Debug logs.** The log can be saved to a micro SD card formatted as **FAT32** (tested up to
16 GB). The card is detected only at start-up: insert it, then restart the camera. The log file is
`SysLog.log`, and you can download it from the System page (**Get logs**). The log level is set on
the System page (Error, Warning, Info, Verbose). The token is never written to the log.

<a name="serial-console"></a>
**Serial console.** USB serial, **115200 8N1**. A command has the form `command:value;`
(for example `setwifissid:MyNetwork;`), with any line ending.

| Command | Description |
|---|---|
| `setwifissid:SSID;` | Set the Wi-Fi network name |
| `setwifipass:PASSWORD;` | Set the Wi-Fi password |
| `wificonnect;` | Connect to the Wi-Fi network |
| `setauthtoken:TOKEN;` | Set the Prusa Connect token |
| `mcureboot;` | Restart the camera |
| `commandslist;` | List the commands |
| `getwifimode;` | Print the Wi-Fi mode (STA/AP/AP+STA) |
| `getwifistastatus;` | Print the Wi-Fi status |
| `getwifistaip;` | Print the IP address |
| `getserviceapssid;` | Print the service AP name |
| `resolution:INDEX;` | Set the photo resolution, index 0–6 (320×240 to 1600×1200). Higher resolutions: Camera page |
| `photoquality:VALUE;` | Set the photo quality, 10–63 (lower = better) |
| `setflash;` | Switch the flash on or off (toggle) |
| `setlight;` | Switch the light on or off (toggle) |
| `loglevel:LEVEL;` | Log level: 0=Error, 1=Warning, 2=Info, 3=Verbose |

Basic setup over the serial console: `setwifissid:SSID;` → `setwifipass:PASSWORD;` →
`wificonnect;` → `setauthtoken:TOKEN;` → `mcureboot;`.

**Web API.** With the login enabled, every address needs it.

| Address | Description |
|---|---|
| `http://IP/stream.mjpg` | Live stream (MJPEG), at most 2 clients |
| `http://IP/saved-photo.jpg` | Latest photo |
| `http://IP/action_capture` | Capture a photo |
| `http://IP/action_send` | Capture a photo and send it to Prusa Connect |
| `http://IP/light?on`, `http://IP/light?off` | Light on / off |
| `http://IP/flash?on`, `http://IP/flash?off` | Flash on / off |
| `http://IP/action_reboot` | Restart the camera |
| `http://IP/get_logs` | Download the log from the micro SD card |
| `http://IP/json_input` | All the status values (JSON), including the Health section |

---

<a name="troubleshooting"></a>
## Troubleshooting

| Symptom | What to do |
|---|---|
| Prusa Connect status **403** | The token is wrong, or the first upload did not arrive within 15 minutes of creating the camera. Create a new camera in Prusa Connect and paste the new token |
| Prusa Connect status **401 / 422** | The token is missing or does not have 20 characters A-Z, a-z, 0-9 |
| Prusa Connect status **413** | The photo is larger than 1 MB: lower the resolution or the quality |
| Status 429 / 503 / 500 / no connection | Temporary: the camera waits longer between attempts, up to 10 min, and recovers by itself |
| Camera name in Prusa Connect ends with `[...]` | A camera problem, see [Health checks](#health-checks) |
| The login `admin` / `admin` does not work after the first flash | [Factory reset](#service-ap) |
| Cannot connect to the service AP (authentication error) | Erase the whole flash and flash again ([how to flash](#flash)) |
| *PrusaLink status* is not `OK` | Check the IP, user and password. `[PRUSALINK LOGIN]` means a wrong user or password |
| Camera or SD card not detected | Press the camera board and the processor board together; check the ribbon cable |
| Purple or noisy image, unstable Wi-Fi | Too hot: see the [thermal guard](#thermal-guard) |
| The web page loads slowly in the first minute | Normal right after the start |

---

<a name="known-limitations"></a>
## Known limitations

- **Not tested on a real board.** It compiles without errors, but the real behavior (camera,
  Wi-Fi, temperature, PrusaLink) can only be confirmed on your Core One.
- **PrusaLink on the Core One:** the PrusaLink specification defines `/api/v1/status` and *digest*
  authentication. It is not confirmed that the Core One firmware answers exactly like that. If
  *PrusaLink status* does not show `OK`, open an issue with the message shown.
- **Thermal limits (75 / 68 °C)** are an estimate for the chip's internal temperature. Adjust them
  after seeing real values on the System page.
- **At most 2 stream clients** at a time.
- **No firmware update from the internet.** Update from a `.bin` file on the System page, or over
  USB.
- The only possible intervals are 10 s, 30 s, 60 s and 10 min (a Prusa rule).
- Built and checked only for the XIAO ESP32-S3 Sense (see [other boards](#other-boards)).

---

<a name="bugs-fixed"></a>
## The 11 bugs fixed

A review of the whole original code found 11 bugs that, in long use (prints of many hours), cause
hangs, reboots, lost photos or security problems. **All 11 are fixed.**

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

**Also fixed:** two more buffer over-reads (`AsyncBufferResponse` and `AsyncFrameResponse` copied
too many bytes), the camera can no longer be re-initialized in the middle of a capture (when you
change the resolution in the browser), and the "Save network ip method" log always showed "DHCP"
(operator precedence error).

The XIAO status LED polarity was checked too: the original behavior is correct and was not changed.

---

<a name="camera-service"></a>
## Migration to the Prusa Camera Service

Prusa created a new service for cameras: <https://camera-service.prusa3d.com/docs/>. The old
addresses (`connect.prusa3d.com/c/snapshot` and `/c/info`) still work, but the documentation says
new code should use the new ones. What changed:

| New API rule | Before | Now |
|---|---|---|
| `/v1/snapshot` and `/v1/camera-info` on `camera-service.prusa3d.com` | `/c/snapshot` and `/c/info` on `connect.prusa3d.com` | New addresses. Old configurations with `connect.prusa3d.com` are **migrated automatically** |
| The server certificate is issued by Google Trust Services | Trusted only ISRG Root X1 (Let's Encrypt), so **the connection would fail** | Added GTS Root R4 and GTS Root R1 (ISRG Root X1 kept). Verified against the real server |
| Camera info with `device: {type: "ESP32", firmware_version, hardware_version}`. Unknown fields are **rejected** | Old format, which would be rejected (error 422) | New format, exactly as documented |
| Camera name of at most 32 characters | Up to 40 | Cut to 32 |
| `User-Agent` with name and version | `ESP32-CAM` | `PrusaESP32Cam/1.1.2-c8` |
| Only 4 intervals: 10 s, 30 s, 60 s, 10 min. At most 6 uploads per minute | Any value from 10 to 240 s | A list with the 4 values. Old values are converted to the nearest one |
| Photos of at most 1 MB | Not checked | Checked before uploading |
| Each error code has its own action | All errors handled the same way | See below |

**What the camera does for each server response:**

- **200**: photo accepted.
- **429 / 503 / 500 / no connection**: temporary problem. Increasing wait: the normal interval,
  then double, up to a maximum of 10 min. Back to normal after a successful upload.
- **401 / 403 / 422 / other 4xx**: the request is wrong (for example an invalid token). Retries
  only every 10 min, until you change the token. A rejected camera info is sent again only after a
  configuration change.
- **413**: photo too large. A message asks you to lower the resolution or the quality.

**Two important rules of the new service:**

1. **15-minute window:** after you add the camera in Prusa Connect, the camera must make its first
   upload **within 15 minutes**, otherwise the token stops working. When you save the token, the
   camera sends its data immediately (it does not wait for the interval).
2. **Fingerprint:** the first upload binds the token to the board's "fingerprint" forever. The
   firmware computes it from the chip ID and the MAC address, so it **survives a factory reset**.
   It is about 48 characters long (the limit is 16 to 64).

The token is validated on the web page: it must have exactly **20 characters A-Z, a-z, 0-9**.

---

<a name="core-one-defaults"></a>
## Core One factory settings

These are the settings the camera uses on its first start (or after a factory reset). You can
change everything later on the camera's web page.

| Setting | Original | Now | Why |
|---|---|---|---|
| Resolution | 320×240 (QVGA) | **1024×768 (XGA)** | 320×240 is too small to see details of the part. XGA shows the part well and stays far below the 1 MB limit |
| JPEG quality | 10 | **12** | Slightly smaller files and faster uploads, with almost no visible difference (scale 10–63: lower = better) |
| Server | `connect.prusa3d.com` | **`camera-service.prusa3d.com`** | The new API |
| Interval | 30 s | 30 s | Same |
| Flash LED | off | off | The Core One has its own LED lighting in the print chamber. The XIAO has no flash on the board anyway |
| White balance | automatic | **automatic** | Without testing under the real light of your Core One, automatic is safer. You can change it in *Advanced settings* after seeing the images |

---

<a name="photo-stream-paths"></a>
## How the photo and stream paths work

The board has one camera and two users of it: the **photo task** (uploads to Prusa Connect) and the
**live stream** (web page). Who owns the camera depends on whether the stream is on. The photo,
TLS, PrusaLink and stream capture tasks run on core 1; Wi-Fi and the other tasks on core 0.

### Photo path (Prusa Connect)

The photo task wakes up once per second:

```
every 1 s
 ├─ read the MCU temperature (thermal guard)
 ├─ PrusaLink: read the printer state (every 10 s; 60 s when the printer does not answer)
 │     └─ printing started or finished -> upload a photo now
 ├─ update the camera health shown in Prusa Connect
 └─ upload interval expired?
       ├─ send the camera information   (/v1/camera-info, only after a change)
       ├─ capture the photo
       ├─ make a private copy of the photo (PSRAM)
       ├─ upload the copy over HTTPS     (/v1/snapshot, 16 KB chunks, keep-alive)
       ├─ save it to the SD card (when enabled and the stream is off)
       └─ free the copy
```

The upload interval is the **longest** of:

- the configured interval (10 s, 30 s, 60 s or 10 min);
- the backoff after a failed upload (doubles up to 10 min);
- 10 min when PrusaLink says the printer is idle;
- 60 s while the thermal guard is active (above 75 °C, until 68 °C).

Capturing a photo with the stream off:

1. Turn on the flash, if enabled.
2. Capture and discard a "training" frame: the first frame from the sensor is old.
3. Capture the real photo and check that it is a valid JPEG. A bad frame goes back to the driver
   and the capture is retried, up to 5 times.
4. Add the EXIF header (rotation, sensor model).
5. Always turn off the flash.

The upload works on its own copy, so the camera is free again at once and the photo cannot change
in the middle of an upload.

### Stream path

```
 [Sensor] --> stream capture task --> 3 PSRAM buffers --> web server --> browser 1
               (core 1)                A   B   C                    \-> browser 2
```

- **Stream capture task:** sleeps and frees the buffers while nobody watches. With a client it
  picks a free buffer (not the latest frame, not one being sent), takes a frame from the sensor,
  copies it, gives the frame back to the driver at once and publishes the buffer as the latest
  frame with a sequence number. It waits up to 300 ms when nobody has taken the latest frame yet,
  so it does not capture frames that nobody sees.
- **Web server, per client:** takes the latest frame (if it is newer than the last one sent),
  marks the buffer as in use, sends it at the network speed, then releases it.
- **3 buffers:** one holds the latest frame, one is being sent, one is being filled. Capture and
  sending run at the same time, and the web server never waits for the camera.
- **At most 2 clients.** Each client runs at its own speed: a slow phone gets fewer frames but
  does not slow down the PC.

### When both paths meet (stream on)

While the stream is on, the camera belongs to the stream capture task. The photo task does not
capture: it copies the latest stream frame and uploads it. "Get photo" and `/saved-photo.jpg` do
the same. The stream is not interrupted and no extra CPU is used. The cost: that photo has no EXIF
header and is not saved to the SD card.

### Safety nets

- Nobody waits for the camera forever: 5 s for the camera, 3 s for a stream frame.
- Above 75 °C the stream stops and uploads slow down until the MCU is back at 68 °C.
- A failed upload waits longer before the next try, without dropping Wi-Fi.
- Low memory for a long time leads to a controlled restart (see [Health checks](#health-checks)).
- The task watchdog restarts the board if a task is stuck for more than 40 s.

---

<a name="performance"></a>
## Speed and efficiency

**Live stream and web page**

| Before | Now | Effect |
|---|---|---|
| The web server task captured each frame itself and waited for the camera (one frame time) before sending it. While it waited, **no other page or request was answered** | A separate capture task (core 1) captures the frames into 3 rotating buffers in PSRAM. The web server only sends the latest complete frame | The next frame is captured **while** the current one is being sent. The web page stays responsive while the stream is open |
| Three `delay(1)` per sent chunk (10–17 ms per frame) and a serial print per frame | Removed | More frames per second |
| Every stream frame (about 100 KB) was copied into a second buffer for Prusa Connect | Prusa Connect, the SD card and `/saved-photo.jpg` copy the latest stream frame only when they need it | Less CPU and PSRAM traffic per frame |
| One stream client | Up to **2 clients** (`CAMERA_STREAM_MAX_CLIENTS`) | PC and phone at the same time |
| `scripts.js` (19 KB) downloaded again on every tab click and copied into the internal RAM | Sent directly from flash, cached by the browser, with the firmware version in its URL | Faster tab switches, less RAM use |
| HTML pages cached for 24 h: after a firmware update the browser showed the old pages | HTML pages are not cached; CSS and JS URLs carry the firmware version | Always the current pages |
| Sending a page measured the whole file for every chunk (about 5 MB read per jQuery load) | Size measured once, the response carries `Content-Length` | Pages open faster |

**Uploads to Prusa Connect**

| Before | Now | Effect |
|---|---|---|
| A new HTTPS connection for every upload: a full TLS handshake (about 1 s of CPU time) every 10–60 s | The connection stays open between uploads when the interval is at most 90 s (`BACKEND_KEEPALIVE_MAX_INTERVAL`) | Much less CPU time and heat per upload |
| Photo sent in 2 KB chunks (about 60 TLS records) | 16 KB chunks, the maximum of one TLS record | Faster upload, less CPU |
| The TLS handshake could wait up to 120 s, but the watchdog reboots at 40 s | 15 s limit (`TLS_HANDSHAKE_TIMEOUT`) | No reboot in the middle of a slow upload |
| With the printer switched off, PrusaLink blocked the photo task for about 3 s every 10 s | While the printer does not respond, it asks only every 60 s | No delayed uploads |

Measured from a PC on 2026-10-19: the Camera Service answers with `Connection: keep-alive` and
`Content-Length`, an idle connection was still open after 65 s, and a request on a reused
connection took about 60 ms instead of about 150 ms. If the server closed the connection in the
meantime, the camera reconnects and sends the photo again once (a snapshot replaces the previous
one, so a repeat is safe). With the 10 min interval the connection is closed after each upload,
which frees about 40 KB of internal RAM.

**Logs and memory**

| Before | Now | Effect |
|---|---|---|
| Every log line forced a write to the SD card: about **86,000 per day** with photos every 10 s | Warnings and errors are written immediately, the rest at most every 5 s (`LOGS_FLUSH_INTERVAL`). The routine messages of each photo moved to the Verbose level | Less SD card wear, less waiting between tasks |
| The debug messages of the task loops were built even when they were not logged | Built only when the Verbose level is active | Less fragmentation of the internal memory |

**How to check it on the board:** the telemetry log line "Stream, average data in 30sec. FPS: ...,
Size: ...KB" shows the stream frame rate. Open the stream and click through the web page tabs: the
page should answer without waiting for the stream.

**Still open, after the first test on the board:** two image buffers in the camera driver
(`fb_count = 2`). The capture task already gets most of the gain, and it would keep the camera
capturing between photos, which means more heat.

---

<a name="health-checks"></a>
## Health checks

The camera runs for weeks inside the printer without anybody looking at it. It recovers by itself
where it can, and shows its problems where you already look: in Prusa Connect.

### Low memory: automatic restart

After a long uptime the memory can get fragmented: there is still free memory, but no block is big
enough for a photo or a TLS connection, and the uploads start to fail. Every 10 s the camera checks
the largest free block:

| Memory | Minimum largest free block | Used for |
|---|---|---|
| Internal RAM | 16 KB (`HEALTH_LOW_MEMORY_INTERNAL`) | Wi-Fi, TLS |
| PSRAM | 512 KB (`HEALTH_LOW_MEMORY_PSRAM`) | Photos, stream frames |

When the memory stays low:

- for **5 minutes**, the camera restarts, but only when **nobody watches the stream** and the
  printer is **not printing** (PrusaLink state; without PrusaLink the printer counts as not printing);
- for **1 hour**, it restarts anyway, also during a print.

A restart takes about 10 s. The reason is saved in the RTC memory (it survives a software restart)
and shown on the System page after the start. The other automatic restarts (Wi-Fi lost for a long
time, camera init failed) save their reason too.

### Camera health in Prusa Connect

The Camera Service API has no status field. The only text that the camera can show in Prusa
Connect is its **name**, so a problem is added to the name, for example `prusa-esp32cam [HOT]`.
When the problem is gone, the normal name comes back. The name is sent with the next photo.

| Name suffix | Meaning |
|---|---|
| `[HOT]` | The thermal guard is active: MCU above 75 °C, stream stopped, uploads slowed down |
| `[LOW MEMORY]` | The memory is low, an automatic restart can follow |
| `[CAMERA ERROR]` | The last photo capture failed |
| `[PRUSALINK LOGIN]` | Wrong PrusaLink user name or password |

When more than one problem is active, the first one in this table is shown. If the camera cannot
upload at all (Wi-Fi down, camera off), it cannot show anything: in that case Prusa Connect shows
that the last photo is old.

### Health on the System page

| Line | What it shows |
|---|---|
| Camera health | The same text as in Prusa Connect, or `OK` |
| Photos uploaded since start | Accepted photos and failed photos (capture or upload failed) |
| Unplanned restarts | Crashes, watchdog, power drops and automatic restarts. Saved in the EEPROM, cleared by a factory reset |
| Last restart | Power on, USB, restart from the web page, crash, watchdog, power drop, or automatic restart and its reason |
| Highest ESP32 temperature since start | Use it to tune the [thermal limits](#thermal-guard) |
| Internal RAM, PSRAM | Free memory and largest free block |

The telemetry log line every 30 s also shows the largest free block of each memory.

---

<a name="firmware-update"></a>
## Firmware update from file

The original firmware could update itself from the **official Prusa releases** ("Update from
cloud"), which would replace this firmware with the original one. That is **removed**: the cloud
routes, the GitHub certificates and the serial command `otaupdate`.

What remains is the **update from a file** on the System page: you choose a `.bin` on your computer
and the camera installs it over Wi-Fi. Any firmware works, from this repository or the original one:

| Firmware | File |
|---|---|
| This repository | `firmware/xiao-esp32-s3/ESP32_PrusaConnectCam.ino.bin` |
| Original Prusa firmware | `XIAO_ESP32S3.bin` from the [Prusa releases](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam/releases) |

How it works:

1. System page → **Firmware update from file** → choose the file → **Upload and install**.
2. Before anything is written, the camera checks the beginning of the file. It rejects:
   - a file that is not a firmware;
   - a firmware for another chip (for example the ESP32-CAM AI Thinker file);
   - the `merged.bin`, `bootloader.bin` and `partitions.bin` files: they are for USB flashing, and
     over Wi-Fi they would make the camera unbootable.
3. The new firmware is written to the second firmware slot of the flash. The running firmware is
   not touched, so a failed or interrupted upload changes nothing.
4. At the end the image hash is checked, and only then the new firmware is set for the next start.
   The camera restarts by itself and the page reloads after 20 s.

During the upload, the camera does not send photos to Prusa Connect, and the low memory check does
not restart it. The upload needs the web page login, when it is enabled.

**From this firmware to the original one:** after the original firmware starts, do a
[factory reset](#service-ap). This firmware saves some settings in a different format (the Camera
Service hostname and the 10 min interval), and the original firmware cannot read them.
**From the original firmware to this one:** nothing to do, the settings are converted.

> The check accepts any application for the ESP32-S3, also one built for another ESP32-S3 board
> (for example ESP32-S3-EYE). Use the XIAO file only.

---

<a name="compile"></a>
## How to compile

The firmware compiles with the **ESP32 core 3.1.0** and the exact libraries in this repository's
`libraries/` folder:

| Library | Version | License |
|---|---|---|
| ArduinoJson | 7.3.0 | MIT |
| ArduinoUniqueID | 1.3.0 | MIT |
| AsyncTCP (mathieucarbou) | 3.3.1 | LGPL-3.0 |
| ESPAsyncWebServer (mathieucarbou) | 3.4.5 | LGPL-3.0 |
| OV5640 Auto Focus for ESP32 Camera | 0.1.1 | MIT |

> Use **these versions**. Newer versions of the ESP32 core or of ESPAsyncWebServer may not compile
> or may behave differently.

Before compiling, check in `ESP32_PrusaConnectCam/mcu_cfg.h` that only the XIAO is enabled (this
repository already has it like that):

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
4. The `.bin` files are in `build/xiao-esp32-s3/`. To flash them, see [how to flash](#flash).

You can also flash directly with arduino-cli (change the port):
```bash
arduino-cli upload -p /dev/ttyACM0 -b esp32:esp32:XIAO_ESP32S3 --input-dir build/xiao-esp32-s3 ESP32_PrusaConnectCam
```

### If you change the web page

The web page files are in `webpage/`. After changing them, regenerate `WebPage.h`:
```bash
cd webpage && bash webpage_h_generator.sh
```

When you change `SW_VERSION` in `mcu_cfg.h`, also change the `?v=` version in the `webpage/*.html`
files (browser cache), regenerate `WebPage.h`, and rebuild `firmware/` **after** the version change.

---

<a name="changed-files"></a>
## Changed files

| File | Change |
|---|---|
| `ESP32_PrusaConnectCam/camera.cpp`, `camera.h` | Bugs 1, 3, 7. Safe copy of the photo (`GetPhotoCopy`), semaphore in every operation on the buffers. (c3) Sensor detection, resolution table, safe start. (c5) OV5640 autofocus. (c6) Stream capture task |
| `ESP32_PrusaConnectCam/exif.cpp`, `exif.h` | (c3) EXIF model from the detected sensor |
| `ESP32_PrusaConnectCam/connect.cpp`, `connect.h`, `connect_types.h` | Bugs 2, 4, 5, 6, 10. New Camera Service API, backoff, error codes. (c6) HTTPS keep-alive. (c7) Camera health in the name, upload counters |
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
| `ESP32_PrusaConnectCam/ExternalTemperatureSensor*`, `libraries/DHTNEW`, `module_*.h` DHT pins, DHT wiring images in `doc/` | **Removed** (c9). The EEPROM bytes of the sensor stay reserved, the settings after them keep their address |
| `webpage/*`, `ESP32_PrusaConnectCam/WebPage.h` | Interval list, PrusaLink section, sensor and autofocus, Health section, firmware update from file, versioned CSS and JS. (c9) Temperature page removed |
| `libraries/` | **New**: libraries with the exact versions |
| `build_xiao.sh` | **New**: build script |
| `firmware/xiao-esp32-s3/` | **New**: compiled firmware |

To see every change line by line against the original:
```bash
git diff 136c91c -- ESP32_PrusaConnectCam webpage
```

---

<a name="version-history"></a>
## Version history

| Version | Changes |
|---|---|
| 1.1.2-c1 | 11 bugs fixed, Prusa Camera Service, Core One defaults, thermal guard, PrusaLink |
| 1.1.2-c2 | TLS timeout, browser cache, page sending, rejected camera info, PrusaLink with the printer switched off, 16 KB TLS chunks, batched SD log writes, debug messages |
| 1.1.2-c3 | OV2640, OV3660 and OV5640 detected automatically, resolutions per sensor up to 2592×1944, safe camera start, EXIF model, correct resolution sent to Prusa Connect |
| 1.1.2-c4 | Camera sensor detected once and saved: later starts initialize the camera only once |
| 1.1.2-c5 | OV5640 continuous autofocus, autofocus state on the Camera page |
| 1.1.2-c6 | Stream capture task with rotating buffers (faster stream, responsive web page, 2 clients), HTTPS keep-alive for uploads, `scripts.js` from flash and cached |
| 1.1.2-c7 | Automatic restart after low memory, camera health shown in Prusa Connect, Health section on the System page, update from the internet removed |
| 1.1.2-c8 | Firmware update from a `.bin` file on the System page (this or the original firmware), the file is checked before writing |
| 1.1.2-c9 | External DHT22/DHT11 temperature sensor removed (code, Temperature page, `/get_temp`, `/get_hum`, DHTNEW library, wiring docs). The Core One has its own chamber temperature sensor |

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

---

<a name="other-boards"></a>
## Other boards

The original project supports several ESP32 camera boards. Their configuration is still in the
code (`module_*.h`, selected in `mcu_cfg.h`), and their original documentation is in `doc/`:

| Board | Original documentation |
|---|---|
| Ai-Thinker ESP32-cam | [doc/AI_Thinker-ESP32-cam](doc/AI_Thinker-ESP32-cam/README.md) |
| ESP32-S3-EYE 2.2 | [doc/ESP32-S3-EYE-22](doc/ESP32-S3-EYE-22/README.md) |
| Freenove ESP32-Wrover cam | [doc/ESP32-Wrover-dev](doc/ESP32-Wrover-dev/README.md) |
| Freenove ESP32-S3-Wroom | [doc/Freenove ESP32-S3-Wroom](doc/Freenove%20ESP32-S3-Wroom/README.md) |
| ESP32-S3-DEV-CAM | [doc/ESP32-S3-DEV-CAM](doc/ESP32-S3-DEV-CAM/README.md) |
| ESP32-S3-CAM | [doc/ESP32-S3-CAM](doc/ESP32-S3-CAM/README.md) |
| Seeed Studio XIAO ESP32S3 | [doc/XIAO_ESP32S3](doc/XIAO_ESP32S3/README.md) |

**This fork is built and checked only for the XIAO ESP32-S3 Sense.** The other boards may compile
with their own `mcu_cfg.h` selection and Arduino settings, but they are not tested, and their
documentation describes the original firmware (for example the cloud update, which no longer
exists here). For those boards, the [original project](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam)
is the better choice.

---

## License and credits

Original firmware by **Miroslav Pivovarsky** for Prusa Research,
[prusa3d/Prusa-Firmware-ESP32-Cam](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam). See
[LICENSE](LICENSE). The libraries in `libraries/` keep their own licenses (table in
[How to compile](#compile)).
