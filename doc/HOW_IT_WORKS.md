# How it works

Technical details of this fork of the Prusa ESP32 Cam firmware. For flashing, setup and use, see the [README](../README.md).

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
| Flash LED | off | **removed** (c10) | The Core One has its own LED lighting in the print chamber. The XIAO has no flash on the board |
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

1. Capture and discard a "training" frame: the first frame from the sensor is old.
2. Capture the real photo and check that it is a valid JPEG. A bad frame goes back to the driver
   and the capture is retried, up to 5 times.
3. Add the EXIF header (rotation, sensor model).

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

When the stream starts, the camera gives its photo buffer back to the driver. The camera keeps a
copy of the last good photo in PSRAM, so `/saved-photo.jpg` (Home Assistant, for example) still
gets a photo until the next one is taken. The upload to Prusa Connect never uses this copy, it
always sends a new photo.

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
| Highest ESP32 temperature since start | Use it to tune the [thermal limits](../README.md#thermal-guard) |
| Internal RAM, PSRAM | Free memory and largest free block |

The telemetry log line every 30 s also shows the largest free block of each memory.
