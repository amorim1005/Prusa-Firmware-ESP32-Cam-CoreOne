# Prusa ESP32 Cam: XIAO ESP32-S3 Sense edition for the Prusa Core One

A monitoring camera for a **Prusa Core One**, built on a **Seeed Studio XIAO ESP32-S3 Sense**
(OV2640, OV3660 or OV5640 camera). It uploads photos to **Prusa Connect**, shows a live stream on
your local network, and runs for weeks inside the printer without supervision.

This repository is a **fork** of the official firmware
[prusa3d/Prusa-Firmware-ESP32-Cam](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam)
(base version **v1.1.2**, commit `136c91c`, by Miroslav Pivovarsky). The full original history is
kept, and the changes of this fork are in the commits on top of it.

Firmware version: **1.1.2-c21** (see the [version history](#version-history)).
The compiled firmware is in the [latest release](https://github.com/amorim1005/Prusa-Firmware-XIAO-ESP32-S3-Sense-Webcam-Core-One/releases/latest).

> **Tested** on a Seeed XIAO ESP32S3 Sense with the **OV3660** sensor (version c17, including a
> 2-hour print on a Prusa Core One): web pages,
> photos, Prusa Connect, PrusaLink, stream, thermal guard and firmware update from file work. See the
> [test report](#test-report). The OV2640 and OV5640 sensors have not been tested on a real board
> yet. Read the [known limitations](#known-limitations).

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
- only the XIAO ESP32-S3 Sense: the other boards, the LED flash, jQuery and the Prusa legal pages
  were removed, the firmware is about 100 KB smaller than c9;
- a reproducible build: exact libraries in `libraries/`, a build script, compiled binaries.

---

## Contents

**Using the camera**
- [Quick start](#quick-start)
- [The board: XIAO ESP32-S3 Sense](#board)
- [How to flash the firmware](#flash)
- [First setup](#first-setup)
- [The web interface](#web-interface)
- [PrusaLink: frequent photos only while printing](#prusalink)
- [Thermal guard and mounting in the Core One](#thermal-guard)
- [Camera sensors and focus](#camera-sensors)
- [Service AP, status LED and factory reset](#service-ap)
- [Home Assistant](#home-assistant)
- [Debug logs, serial console and web API](#debug-logs)
- [Troubleshooting](#troubleshooting)
- [Test report](#test-report)
- [Known limitations](#known-limitations)
- [Firmware update from file](#firmware-update)
- [Version history](#version-history)

**More documentation**
- [How it works](doc/HOW_IT_WORKS.md): the 11 bugs fixed, the Prusa Camera Service, Core One factory
  settings, the photo and stream paths, speed and efficiency, health checks
- [Development](doc/DEVELOPMENT.md): how to compile, changed files, pulling updates from the original project

---

<a name="quick-start"></a>
## Quick start

1. Download `ESP32_PrusaConnectCam.ino.merged.bin` from the [latest release](https://github.com/amorim1005/Prusa-Firmware-XIAO-ESP32-S3-Sense-Webcam-Core-One/releases/latest) and flash it
   at address 0x0 from the browser, Chrome or Edge ([how to flash](#flash)).
2. Connect to the Wi-Fi network `ESP32_camera_...` (open network, no password), open
   <http://192.168.0.1> and set up your Wi-Fi.
3. Create the camera in Prusa Connect and paste its token on the **Camera** page
   **within 15 minutes** ([first setup](#first-setup)).
4. Optional: enable the login, and set up [PrusaLink](#prusalink).

---

<a name="board"></a>
## The board: XIAO ESP32-S3 Sense

The camera is two small boards joined by a board-to-board connector:

- the **XIAO ESP32-S3**: processor, 8 MB flash, 8 MB PSRAM, USB-C, Wi-Fi antenna connector,
  **R** (reset) and **B** (boot) buttons, a red charge LED and the orange user LED (the status
  LED of this firmware);
- the **Sense** expansion board: camera connector and micro SD card slot.

<img src="doc/XIAO_ESP32S3/boards.jpg" width=35%> <img src="doc/XIAO_ESP32S3/desription.png" width=50%>

If the camera or the SD card is not detected, the two boards are usually not pressed together
well at the connector.

**Camera module.** The standard module (OV2640) sees about 66°. In the Core One the camera is
close to the bed, so a **wide-angle module (120° or 160°)** with the same connector shows the
whole bed. The original documentation tested OV2640 modules with 66°, 120° and 160°. OV3660 and
OV5640 modules work too, see [camera sensors](#camera-sensors).

**Pins used by this firmware:**

| Pin (pad) | Use |
|---|---|
| GPIO2 (D1) | Factory reset: connect to GND during the start |
| GPIO21 (orange LED) | Status LED |
| GPIO7, GPIO8, GPIO9 (D8, D9, D10) | Micro SD card |
| GPIO12, GPIO13 (D11, D12 on the Sense board) | Camera |
| GPIO10, 11, 14–18, 38–40, 47, 48 | Camera, inside the connector |
| GPIO1, GPIO3–6, GPIO43, GPIO44 (D0, D2–D7) | **Free**, not used |

The power is 5 V over USB-C. The original documentation asks for a supply rated for 2 A.

<details>
<summary>Pinout and schematics</summary>

<img src="doc/XIAO_ESP32S3/pinoutF.png" width=60%>

<img src="doc/XIAO_ESP32S3/pinoutB.png" width=60%>

Schematics (PDF): [XIAO ESP32-S3](doc/XIAO_ESP32S3/XIAO_ESP32S3_SCH_v1.1.pdf),
[Sense expansion board](doc/XIAO_ESP32S3/XIAO_ESP32S3_ExpBoard_v1.0_SCH.pdf).
OV2640 [datasheet](doc/XIAO_ESP32S3/OV2640-datasheet.pdf).
</details>

---

<a name="flash"></a>
## How to flash the firmware

The easiest way needs only a **browser**: nothing to install and nothing to compile.

1. **Download** `ESP32_PrusaConnectCam.ino.merged.bin` from the
   **[latest release](https://github.com/amorim1005/Prusa-Firmware-XIAO-ESP32-S3-Sense-Webcam-Core-One/releases/latest)**
   (the release also has a `SHA256SUMS.txt` to check the download).
2. **Recommended: install the USB-serial drivers** before you connect the board (Windows; on
   macOS and Linux they are usually not needed):
   - CH340 / CH341: [CH341SER](https://www.wch-ic.com/downloads/CH341SER_EXE.html) (WCH)
   - CH9102 / CH343: [CH343SER](https://www.wch-ic.com/downloads/CH343SER_EXE.html) (WCH)
   - CP210x: [CP210x VCP drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) (Silicon Labs)
   - FTDI: [VCP drivers](https://ftdichip.com/drivers/vcp-drivers/) (FTDI)

   The XIAO ESP32-S3 itself uses the USB of the chip (Windows 10/11 shows it as *USB Serial
   Device* or *USB JTAG/serial debug unit* without a driver), but with these drivers installed,
   other ESP32 boards and USB-serial adapters work too.
3. **Connect** the XIAO to the computer with a USB-C **data** cable (some cables only charge).
   **No port appears?** Try another cable, or hold the **B** (boot) button, plug in the cable and
   release the button.
4. Open <https://espressif.github.io/esptool-js/> in **Chrome or Edge** (Firefox and Safari cannot
   access USB ports).
5. Leave **WebUSB (CH340)** off, **Baudrate** 921600, press **Connect** and choose the XIAO's port.
6. In the file table, change **Flash Address** from `0x1000` (the default, for the older ESP32) to
   **`0x0`**: with `0x1000` the camera does not start. Choose the `merged.bin` file next to it.
7. Leave *Flash Mode*, *Flash Frequency* and *Flash Size* as they are and *Erase all* unticked,
   press **Program** and wait for 100 %.
8. The camera restarts by itself after the programming (only if it does not: press the **R**
   button, or unplug and plug in the cable).
9. On your phone or computer, connect to the camera's Wi-Fi network **`ESP32_camera_...`** (open
   network, no password) and open <http://192.168.0.1>. Do the whole setup there: your Wi-Fi, the
   Prusa Connect token and the other settings (see [first setup](#first-setup)).

The `merged.bin` file writes the whole flash, so the settings go back to the factory settings (no
**Erase Flash** needed). **Later versions** are easier: install `ESP32_PrusaConnectCam.ino.bin` from
the System page over Wi-Fi, without a cable, and the settings stay (see
[firmware update from file](#firmware-update)).

<details>
<summary>Other ways to flash (esptool, Espressif Flash Download Tool) and the files</summary>

**With esptool** (Linux/macOS/Windows), first flash (erases everything):
```bash
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 921600 --before default_reset --after hard_reset \
  write_flash --erase-all --flash_mode qio --flash_size 8MB --flash_freq 80m \
  0x0 ESP32_PrusaConnectCam.ino.bootloader.bin \
  0x8000 ESP32_PrusaConnectCam.ino.partitions.bin \
  0x10000 ESP32_PrusaConnectCam.ino.bin
```
To **update without losing the configuration**, use the same command **without** `--erase-all`.

**Windows, Flash Download Tool** from [Espressif](https://www.espressif.com/en/support/download/other-tools):
choose the chip **ESP32-S3**, then add the three files with their addresses (0x0 bootloader,
0x8000 partitions, 0x10000 firmware), SPI speed 80 MHz, SPI mode QIO, and the COM port. The first
time, press **ERASE** before **START**.

<img src="doc/XIAO_ESP32S3/chip_select.jpg" width=25%> <img src="doc/XIAO_ESP32S3/flash.jpg" width=30%>

| File | Address | Use |
|---|---|---|
| `ESP32_PrusaConnectCam.ino.merged.bin` | 0x0 | USB, everything in one file |
| `ESP32_PrusaConnectCam.ino.bootloader.bin` | 0x0 | USB |
| `ESP32_PrusaConnectCam.ino.partitions.bin` | 0x8000 | USB |
| `ESP32_PrusaConnectCam.ino.bin` | 0x10000 | USB, or the System page over Wi-Fi |

</details>

---

<a name="first-setup"></a>
## First setup

> **Follow this order:** first the camera's Wi-Fi, and only then create the camera in Prusa
> Connect. This way you do not risk missing the token's 15-minute window.

1. After flashing, the camera creates a Wi-Fi network **`ESP32_camera_` followed by numbers**
   (the first numbers of the chip ID). It is an open network, without a password. Your computer may complain about
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

**Security.** Even without the login, other web sites cannot change the camera from your browser:
every change must come from the camera page itself (it sends a header that other sites cannot
send), and other sites cannot read the stream either. The camera also answers only to its IP address
and its camera name (`prusa-esp32cam` by default, alone or with `.local`, `.lan`, `.home`,
`.home.arpa`, `.localdomain`, `.internal` or `.fritz.box`); any other host name gets "Forbidden".

**Several cameras.** Give every camera its own name on the **System** page (*Camera name & mDNS
record*), for example `core-one-left` (letters, digits and `-`). After the restart the camera uses it as its mDNS name
(`core-one-left.local`) and as its DHCP name in the router (`core-one-left.lan` on many routers),
and Prusa Connect shows it as the camera name. Until you rename them, cameras share the default
name `prusa-esp32cam.local`: open them by IP address. The service AP names (`ESP32_camera_...`)
are already different for every camera. The login uses Digest authentication, so the password is not sent
in clear text, and the passwords are sent in the request body, never in the URL. The Prusa Connect
token is shown only partly (`****************Uhq?`): paste a new token to change it.
8. Optional: set up [PrusaLink](#prusalink).

**Wi-Fi.** The camera also connects to a **hidden** network (type its name), and it keeps trying
when the router starts after the camera, for example after a power cut. After 3 failed attempts
(network not found, wrong password) it tries once every 28 s, so the service AP stays easy to find.

**Wi-Fi Enterprise** (company or school network, WPA2/WPA3-Enterprise with PEAP, user name and
password): Wi-Fi page → Advanced Wi-Fi settings → *Wi-Fi Enterprise*: network name, user name and
password, then **Save & Connect**. The server certificate is not checked. Saving a normal Wi-Fi
network, or switching *Use Wi-Fi Enterprise* off, goes back to the normal Wi-Fi. Not tested on a
real Enterprise network yet.

**Remove the protective film from the lens.** New camera modules have a transparent film on the
lens, which makes the image blurry.

<img src="doc/cam_prot_film.jpg" width=40% height=40%>

---

<a name="web-interface"></a>
## The web interface

| Page | What you find there |
|---|---|
| Main page | Latest photo, live stream, current upload interval (it already includes the backoff, PrusaLink and the thermal guard) |
| **Camera** | Every change is saved at once (a short *Saved ✔* at the bottom of the page confirms it, without a popup); only the text fields have a Save button. Token, upload interval (10 s, 30 s, 60 s or 10 min), resolution (only the ones your sensor supports), quality, image settings, detected sensor, autofocus state, PrusaLink |
| **Wi-Fi** | Network, static IP, service AP on/off |
| **Authentication** | Login for the web page and the stream |
| **System** | Prusa Connect status, uptime, temperature, [Health](doc/HOW_IT_WORKS.md#health-checks) section, installed version (with a notice when a newer release exists on GitHub) and the original version it is based on, camera name (mDNS), log level and logs, status LED, micro SD card, [firmware update from file](#firmware-update) |

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

**The solution:** the camera reads the chip's internal temperature every second (an average of the
readings, so one noisy reading does not switch anything) and:

| Chip temperature | What happens |
|---|---|
| below 80 °C | Normal. The stream sends **at most about 5 frames/s**: more frames only heat the chip |
| **≥ 80 °C** | The stream slows down to **1 frame/s** (back to normal at 75 °C) |
| **≥ 88 °C** | Protection on: the **stream is stopped** (new streams get a 503 answer), and photos to Prusa Connect are limited to **at most 1 per minute**. Prusa Connect shows the camera as `[HOT]` (back to normal at 80 °C) |

The gaps between the on and off values (*hysteresis*) prevent the protection from switching on and
off every second near a limit. **Start stream** opens a page that shows why the stream is slow or
stopped, and starts it again when the camera cools down. On the **System** page, the temperature
shows a "TOO HOT" warning while the protection is active, and the Health section shows the highest
temperature since the start.

This is the chip's **internal** temperature, which is much higher than the air temperature: in the
first test (c11, no heatsink, room temperature) the stream heated it above 75 °C within seconds.
The firmware reads the sensor in its 20–100 °C range (error below 2 °C); `temperatureRead()` of the
Arduino core uses the -10–80 °C range, below the limits. The limits are in `mcu_cfg.h`
(`THERMAL_STREAM_SLOW_TEMP`, `THERMAL_STREAM_NORMAL_TEMP`, `THERMAL_THROTTLE_TEMP`,
`THERMAL_RESUME_TEMP`, `THERMAL_THROTTLE_INTERVAL`, `CAMERA_STREAM_FRAME_INTERVAL`).

**Mounting:**
- If possible, mount the camera **outside the heated chamber** (looking through the door or the
  glass), or at a spot with moving air.
- Stick a **small aluminium heatsink** on the back of the XIAO, on the **thermal pad** under the
  chip (see the back pinout in [the board](#board)).
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
  resolutions: 1280×720, 1920×1080, 2048×1536 and 2592×1944. The smallest resolution is
  1024×768 (c12): 320×240 to 800×600 are too small to see the part in Prusa Connect. A camera
  that had one of them saved switches to 1024×768.
- **Safe start:** a saved resolution above the sensor maximum is lowered to the sensor maximum
  by the camera driver.
- **Settings:** every sensor setting is applied only if the sensor driver implements it.
- **Metadata:** the EXIF "Model" field and the camera info sent to Prusa Connect contain the
  detected sensor name.

**Practical advice:**
- The Prusa Camera Service accepts photos up to **1 MB**. At 2048×1536 and above, keep the image
  quality moderate (the default 12 or higher). Photos over 1 MB are not uploaded and the status
  shows a clear message.
- The **Estimated photo size** below the resolution on the Camera page shows the size of the photo
  with the chosen resolution and image quality, while you move the slider:

  | Color | Meaning |
  |---|---|
  | Green | Well below 1 MB |
  | Orange | Close to 1 MB: a photo with more detail can be larger |
  | Red | Over 1 MB, with the warning *this firmware only supports photos up to 1 MB*. Lower the resolution or the quality |

  After the first photo, the estimate scales the real size of the last photo (pixels × JPEG
  quality factor), so it already includes your scene and light. Every photo counts: upload,
  **Refresh Snapshot**, SD card and stream (c12). A dark or plain scene gives small photos; the
  same settings with the lit, detailed print chamber give larger ones. Before the first photo it uses a
  typical scene and a larger safety margin. Brightness, contrast and the other settings change the
  size only a little and are not part of the estimate.
- Very high JPEG quality at high resolutions can make a photo larger than the frame buffer (the
  driver logs `FB-OVF`). The quality slider stops at 10, which avoids the worst cases.
- If the image is upside down or mirrored, use **Vertical flip** / **Horizontal mirror** on the
  Camera page. The firmware does not flip any sensor by itself.
- **OV3660 brightness (c19).** The original settings were written for the OV2640: on the OV3660 they
  limited the automatic gain to "no gain" and wrote the manual exposure and gain also with the
  automatic control on, so the picture was dark (fix from the fork of
  [dawidkulpa](https://github.com/dawidkulpa/Prusa-Firmware-ESP32-Cam)). If you raised
  **Brightness** to compensate, set it back to the middle.

**OV5640 autofocus.** After every camera start the firmware loads the OmniVision autofocus firmware
(4 KB) into the OV5640 and starts **continuous autofocus**. The Camera page shows the autofocus
state (focused / focusing / idle). It uses the library
[OV5640 Auto Focus for ESP32 Camera](https://github.com/0015/ESP32-OV5640-AF) 0.1.1 (MIT), the same
approach as Seeed's examples.
- *Why not the autofocus of the ESP32 core?* The esp32-camera driver has an autofocus API
  (`esp_camera_af.h`), but the official Arduino core builds have it switched off: in core 3.3.12
  (used since c16) `CONFIG_CAMERA_AF_SUPPORT` is not set, so every call returns `ESP_ERR_NOT_SUPPORTED`.
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
(open network without a password, IP **192.168.0.1**, or `http://prusa-esp32cam.local`), for the first setup.
When a Wi-Fi network is configured, the camera also connects to it, and the service AP switches
itself off about **1 minute** after the camera is connected to your network, when no device is
connected to the service AP (c13; a stream goes over your network and does not keep it on). If the
camera cannot connect to your network, the service AP stays on.
Because the network is open, anyone near the camera can join it while it is on: enable the login
(Authentication page) if that matters to you.
You can switch the service AP off completely on the Wi-Fi page. If the service AP does not work for
you, use the [serial console](#serial-console).

**Status LED** (GPIO21, the small orange LED next to the USB-C connector). It lights up at power
on, then blinks:

| Blink interval | Meaning |
|---|---|
| 400 ms | Service AP only |
| 800 ms | Connecting to Wi-Fi |
| 4000 ms | Connected to Wi-Fi |
| 100 ms | A problem |

**System → Status LED** (c12) chooses when it blinks: **On** (as above), **Errors only** (dark while
connected to Wi-Fi, it blinks only in the other states) or **Off** (always dark). The factory reset
still blinks it. The red charge LED is controlled by the battery charger chip, not by the firmware:
only tape or removing it makes it dark.

The camera needs about 15–20 s to start.

**Factory reset:**

<img src="doc/XIAO_ESP32S3/factory_cfg.png" width=25%> <img src="doc/XIAO_ESP32S3/status_led.png" width=25%>

*Left: the pins for the factory reset, GPIO2 (left arrow) and GND (right arrow). Right: the status LED.*

1. Connect pin **GPIO2** to **GND**.
2. Power on and wait about 10 s, until the LED blinks.
3. Disconnect GPIO2 from GND. The LED stops blinking and the configuration is back to the factory
   settings (also the unplanned restart counter).

The board's fingerprint for Prusa Connect is computed from the chip, so it survives a factory reset.

Pins, photos and schematics: [the board](#board).

---

<a name="home-assistant"></a>
## Home Assistant

The camera works in Home Assistant as an MJPEG camera: the dashboard shows the latest photo, and the
live stream opens when you tap it.

1. In the router, give the camera a **fixed IP address** (DHCP reservation), so the address does
   not change.
2. In Home Assistant: **Settings → Devices & services → Add integration → MJPEG IP Camera**.
3. Fill in (replace `IP` with the camera address):

   | Field | Value |
   |---|---|
   | Name | `Prusa Webcam` |
   | MJPEG URL | `http://IP/stream.mjpg` |
   | Still image URL | `http://IP/saved-photo.jpg` |
   | Username / Password | empty, unless you enabled the login on the camera |
   | Verify SSL certificate | off |

4. Add a card to the dashboard (**Edit dashboard → Add card → Manual**):

   ```yaml
   type: picture-entity
   entity: camera.prusa_webcam
   camera_view: auto
   show_name: false
   show_state: false
   ```

The card shows the latest photo (the one sent to Prusa Connect), which does not heat the camera.
Tap it to open the live stream; the stream stops when you close the window. With the stream open the
camera gets warmer, see [Thermal guard](#thermal-guard).

> Before version c18 the still image URL could fail with "Cannot connect" after a stream. With an
> older version, leave the still image URL empty.

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
| `resolution:INDEX;` | Set the photo resolution: 4 = 1024×768, 5 = 1280×1024, 6 = 1600×1200, 7 = 1280×720, 8 = 1920×1080, 9 = 2048×1536, 10 = 2592×1944 (only up to the sensor maximum) |
| `photoquality:VALUE;` | Set the photo quality, 10–63 (lower = better) |
| `loglevel:LEVEL;` | Log level: 0=Error, 1=Warning, 2=Info, 3=Verbose |

Basic setup over the serial console: `setwifissid:SSID;` → `setwifipass:PASSWORD;` →
`wificonnect;` → `setauthtoken:TOKEN;` → `mcureboot;`.

**Web API.** With the login enabled, every address needs it.

| Address | Description |
|---|---|
| `http://IP/stream.mjpg` | Live stream (MJPEG), at most 2 clients |
| `http://IP/saved-photo.jpg` | Latest photo. After a stream, the last saved photo until the next one is taken |
| `http://IP/action_capture` | Capture a photo |
| `http://IP/action_send` | Capture a photo and send it to Prusa Connect |
| `http://IP/action_reboot` | Restart the camera |
| `http://IP/get_logs` | Download the log from the micro SD card |
| `http://IP/json_input` | All the status values (JSON), including the Health section |

For Home Assistant, see [Home Assistant](#home-assistant).

---

<a name="troubleshooting"></a>
## Troubleshooting

| Symptom | What to do |
|---|---|
| Prusa Connect status **403** | The token is wrong, or the first upload did not arrive within 15 minutes of creating the camera. Create a new camera in Prusa Connect and paste the new token |
| Prusa Connect status **401 / 422** | The token is missing or does not have 20 characters A-Z, a-z, 0-9 |
| Prusa Connect status **413** | The photo is larger than 1 MB: lower the resolution or the quality |
| Status 429 / 503 / 500 / no connection | Temporary: the camera waits longer between attempts, up to 10 min, and recovers by itself |
| Camera name in Prusa Connect ends with `[...]` | A camera problem, see [Health checks](doc/HOW_IT_WORKS.md#health-checks) |
| The login `admin` / `admin` does not work after the first flash | [Factory reset](#service-ap) |
| Cannot connect to the service AP (authentication error) | Erase the whole flash and flash again ([how to flash](#flash)) |
| *PrusaLink status* is not `OK` | Check the IP, user and password. `[PRUSALINK LOGIN]` means a wrong user or password |
| Camera or SD card not detected | Press the camera board and the processor board together; check the ribbon cable |
| Purple or noisy image, unstable Wi-Fi | Too hot: see the [thermal guard](#thermal-guard) |
| The web page loads slowly in the first minute | Normal right after the start |

---

<a name="test-report"></a>
## Test report

**Version c17, 2026-11-04**, on a real camera during a real print. The camera status was read from
the camera's own JSON page (`/json_input`) once a minute, from a PC on the same network.

| Item | Setup |
|---|---|
| Board | Seeed XIAO ESP32S3 Sense, **OV3660** sensor, **without any heatsink** |
| Printer | Prusa Core One, PETG print with a filament change, about 2 hours |
| Firmware | c17, installed over the web page (*Firmware update from file*), settings kept |
| Settings | Upload every 10 s while printing (PrusaLink), 1280×1024 at the start, 2048×1536 from the 3rd minute |
| Position | Camera on the desk next to the printer, **not inside the print chamber**, at room temperature. In the final mounting inside the Core One the chamber is warmer, but the camera has **2 heatsinks**; those temperatures will be added after a test there |

| Result | Value |
|---|---|
| Photos uploaded to Prusa Connect | **764 OK, 1 failed** (a single failed upload after 617 OK; the next one was OK again, no backoff) |
| Restarts | **None**: 2 h 25 min without a restart, 0 unplanned restarts |
| Upload interval | 10 s while PRINTING, about 6 photos per minute; back to 600 s (10 min) when FINISHED |
| PrusaLink | Printer states read correctly: IDLE → PRINTING (with progress %) → ATTENTION (filament change) → FINISHED |
| Camera temperature | **62.7–64.7 °C** during the whole print; **74.5 °C** for about a minute with the live stream open, back to about 65 °C after closing it. The thermal guard (80 °C) was never reached |
| Photo size | about 43 KB at 1280×1024, about 96 KB at 2048×1536 (scene with little detail) |
| Memory | Internal RAM 110 KB free (largest block 57 KB), PSRAM about 7.5 MB free, stable during the print |
| Wi-Fi | Service AP switched off by itself about 1 minute after the connection to the home Wi-Fi (*AP + Client* → *Client*) |
| Live stream | About 2 frames per second at 2048×1536, photos kept uploading while the stream was open |
| Prusa Connect | The camera shows next to the printer on the Dashboard with the current picture |

After the print, still with c17: the camera added to **Home Assistant** as an *MJPEG IP Camera*
(still image URL left empty, see [Home Assistant](#home-assistant)) worked at the first try.

<a name="known-limitations"></a>
## Known limitations

- **Tested with the OV3660 sensor only.** The OV2640 and OV5640 sensors (and the OV5640 autofocus)
  have not been tested on a real board yet.
- **PrusaLink** is tested with a Core One (status and printer state). If *PrusaLink status* does
  not show `OK` on another printer, open an issue with the message shown.
- **Thermal limits (80 / 88 °C)** are an estimate for the chip's internal temperature, set after the
  first test without a heatsink. Adjust them after seeing real values on the System page.
- **At most 2 stream clients** at a time.
- **No firmware update from the internet.** Update from a `.bin` file on the System page, or over
  USB.
- The only possible intervals are 10 s, 30 s, 60 s and 10 min (a Prusa rule).
- Only for the XIAO ESP32-S3 Sense. For other boards use the
  [original project](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam).

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
| This repository | `ESP32_PrusaConnectCam.ino.bin` from the [latest release](https://github.com/amorim1005/Prusa-Firmware-XIAO-ESP32-S3-Sense-Webcam-Core-One/releases/latest) |
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

<a name="version-history"></a>
## Version history

Newest first. Every version is based on the original Prusa firmware v1.1.2.

**1.1.2-c21** (2026-11-06)
- A photo is sent as soon as the camera is connected to the Wi-Fi (after a start or a lost
  connection). Before, with the printer idle, Prusa Connect waited up to 10 minutes for the first
  photo

**1.1.2-c20** (2026-11-05)
- OV5640: back to the simple automatic focus (continuous autofocus and its state on the Camera
  page); the focus buttons and the exposure lock of c19 are removed
- Wi-Fi Enterprise: one form with network name, user name and password, and *Save & Connect*
- Wi-Fi: a wrong password is also retried slowly (every 28 s after 3 failures)
- Fixes of a code review: error messages in red, the autofocus state text protected between tasks,
  an empty Wi-Fi password is refused again, log texts use "Wi-Fi"
- Tested on the OV3660: installed over the network, photo upload, PrusaLink, service AP off after
  the Wi-Fi connection

**1.1.2-c19** (2026-11-05)
- Wi-Fi from the coming original 1.1.3: hidden networks, reconnect after a start without network
  (for example after a power cut), one attempt every 28 s after 3 failures, Wi-Fi Enterprise (PEAP)
- OV3660: brighter picture, the automatic gain is no longer limited (fix from the fork of dawidkulpa)
- OV5640 only: Focus once / Continuous / Pause / Release lens buttons and Lock exposure (ideas from
  the fork of SamLovick, removed again in c20)
- Not tested on a real board yet

**1.1.2-c18** (2026-11-04)
- `/saved-photo.jpg` always returns the latest photo, also after a stream (Home Assistant)
- README: Home Assistant setup

**Up to 1.1.2-c17**: 11 bugs fixed, new Prusa Camera Service API, only the XIAO ESP32-S3 Sense
(OV2640, OV3660 and OV5640 detected automatically, OV5640 autofocus), faster stream, thermal guard
in two steps, PrusaLink, health checks, firmware update from file, open service AP for the setup, a
simpler web page, protection against other web sites (CSRF, DNS rebinding), ESP32 core 3.3.12 with
current libraries, and the fixes of a code review (login settings, settings shared between tasks,
checked input values). c17 was tested on the OV3660 during a 2-hour print.

---

## License and credits

Original firmware by **Miroslav Pivovarsky** for Prusa Research,
[prusa3d/Prusa-Firmware-ESP32-Cam](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam). See
[LICENSE](LICENSE). The libraries in `libraries/` keep their own licenses (table in
[How to compile](doc/DEVELOPMENT.md#compile)).
