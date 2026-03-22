# ESP32-CAM Rover Camera Firmware

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Board](https://img.shields.io/badge/board-AI%20Thinker%20ESP32--CAM-00979D)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![Networking](https://img.shields.io/badge/networking-STA%20%2B%20Rescue%20AP-success)
![Status](https://img.shields.io/badge/status-Project%20specific-orange)

ESP32-CAM firmware for a rover-mounted camera node built around the **AI Thinker ESP32-CAM** board.

This firmware is designed for a **fixed rover network topology**:

- the camera normally joins the rover in **Wi-Fi STA mode**,
- the camera always uses a **static IP** on the rover network,
- a **rescue access point** is started only when the rover cannot be reached,
- the firmware periodically retries the rover connection and exits rescue mode automatically.

The design goal is deterministic behavior, low startup complexity, and easier field recovery.

---

## Highlights

- Targeted at **AI Thinker ESP32-CAM**
- Camera initialized with **JPEG output** and **VGA default frame size**
- **Static STA IP** on the rover network: `192.168.4.2`
- **Fallback rescue AP** on a different subnet: `192.168.8.1`
- Automatic **reconnection every 10 seconds** while in rescue mode
- PSRAM-aware configuration for improved frame buffering and capture behavior
- Optional flash LED setup through `setupLedFlash()`
- Avoids `WIFI_AP_STA` during normal boot to reduce complexity and current spikes

---

## Network topology

### Normal mode

The camera tries to join the rover access point as a station.

| Parameter | Value |
|---|---|
| Rover SSID | `ROVER_DIEGO` |
| Rover password | `12341234` |
| Camera STA IP | `192.168.4.2` |
| Gateway / rover AP | `192.168.4.1` |
| Subnet mask | `255.255.255.0` |

### Rescue mode

If the rover is unavailable during boot, the camera creates its own access point.

| Parameter | Value |
|---|---|
| Rescue SSID | `CAMARA_RESCATE` |
| Rescue password | `12341234` |
| Rescue AP IP | `192.168.8.1` |
| Subnet mask | `255.255.255.0` |

### ASCII network diagram

```text
Normal operation
----------------
Phone / PC / Rover controller
            |
            v
   [ Rover AP / Gateway ]   SSID: ROVER_DIEGO
        192.168.4.1
            |
            v
   [ ESP32-CAM camera ]
        192.168.4.2

Fallback / rescue operation
---------------------------
Phone / PC
    |
    v
[ ESP32-CAM Rescue AP ]   SSID: CAMARA_RESCATE
       192.168.8.1
```

A separate rescue subnet avoids routing ambiguity with the rover network.

---

## Boot and runtime behavior

### Boot sequence

1. Start serial debug output at `115200` baud.
2. Configure the AI Thinker camera pins through `camera_pins.h`.
3. Initialize the camera sensor and frame buffers.
4. Apply board/sensor-specific adjustments.
5. Initialize flash LED support if `LED_GPIO_NUM` is available.
6. Try to connect to the rover network in **STA mode**.
7. If STA fails, start the **rescue AP**.
8. Start the camera web server.

### Runtime logic

- If STA is connected, the camera stays on the rover network.
- If STA is not available, the rescue AP keeps the camera reachable.
- Every `10000 ms`, the firmware retries the rover connection.
- If STA comes back, the rescue AP is disabled automatically.
- If an established STA connection is lost, the firmware re-enters the reconnect cycle.

---

## Camera configuration

The sketch configures the camera as follows:

- **Model:** `AI Thinker ESP32-CAM`
- **Pixel format:** `JPEG`
- **Default frame size:** `VGA`
- **XCLK:** `10 MHz`
- **Default framebuffer location:** PSRAM when available
- **Default JPEG quality:** `12`
- **PSRAM mode:** improved quality and double buffering when available

When PSRAM is detected, the firmware switches to a more capable setup:

- JPEG quality improved to `10`
- frame buffer count increased to `2`
- grab mode changed to `CAMERA_GRAB_LATEST`

If PSRAM is not available, the sketch falls back to a more conservative configuration.

---

## Accessing the web interface

Open the camera from a browser using one of these addresses:

- **Normal mode:** `http://192.168.4.2`
- **Rescue mode:** `http://192.168.8.1`

The exact UI and endpoint set depend on the camera server sources linked into the build. In a standard ESP32 camera web server integration, the root page typically exposes preview, stream, capture, and camera controls.

---

## Repository layout

A typical repository layout for this firmware would look like this:

```text
.
├── CameraWebServer.ino
├── camera_pins.h
├── app_httpd.cpp          # if using the standard ESP32 camera web UI/server
├── camera_index.h         # if required by the selected server implementation
├── README.md
└── LICENSE
```

At minimum, the build must provide:

- `CameraWebServer.ino`
- `camera_pins.h`
- an implementation of:
  - `startCameraServer()`
  - `setupLedFlash(int pin)`

---

## Build requirements

Recommended environment:

- **Arduino IDE** or compatible ESP32 Arduino workflow
- **ESP32 board package by Espressif**
- **Board selection:** `AI Thinker ESP32-CAM`
- PSRAM enabled where applicable
- a partition scheme with enough application space for camera firmware

### Required headers

- `esp_camera.h`
- `WiFi.h`
- `soc/soc.h`
- `soc/rtc_cntl_reg.h`

---

## Configuration values in the sketch

The current version is intentionally **project-specific** and uses hardcoded network settings:

```cpp
char wifi_ssid[33] = "ROVER_DIEGO";
char wifi_pass[65] = "12341234";

static const char* RESCUE_AP_SSID = "CAMARA_RESCATE";
static const char* RESCUE_AP_PASS = "12341234";

IPAddress sta_local_IP(192, 168, 4, 2);
IPAddress sta_gateway(192, 168, 4, 1);
IPAddress ap_local_IP(192, 168, 8, 1);
```

### Remote Wi-Fi reconfiguration

Remote Wi-Fi changes are intentionally disabled in this sketch.

The function:

```cpp
void saveWiFiConfig(const String& s, const String& p)
```

logs the request but does not apply it.

This protects the fixed rover/camera topology from accidental remote changes.

---

## Serial diagnostics

Useful information is printed to the serial console, including:

- Wi-Fi status transitions
- associated SSID and BSSID
- STA IP and gateway
- RSSI values
- rescue AP activation
- reconnection attempts
- camera initialization failures

Recommended monitor speed:

```text
115200
```

---

## Troubleshooting

### 1. The camera does not connect to the rover

Check:

- the rover AP is actually running on `192.168.4.1`
- SSID and password match the sketch
- the rover subnet is `192.168.4.0/24`
- the camera static IP `192.168.4.2` is not already in use

If connection fails, the camera should expose the rescue AP `CAMARA_RESCATE` on `192.168.8.1`.

### 2. The web page does not load even though ping works

This usually points to the **camera web server sources** rather than the Wi-Fi link.

Check that your build includes the correct server files implementing:

- `startCameraServer()`
- the HTTP handlers required by the chosen UI/server code
- `camera_index.h` or equivalent assets if your server expects them

### 3. Build error such as `index_handler was not declared`

That typically means the selected `app_httpd.cpp` or associated UI source files are incomplete, mismatched, or out of sync.

Make sure the web server source files all belong to the same implementation set.

### 4. The board boots but camera init fails

Check:

- the board is set to **AI Thinker ESP32-CAM**
- camera pin mapping matches that board
- the power supply is stable
- PSRAM support is correctly configured

### 5. Rescue AP appears too often

That usually means STA association is unstable or the rover is not ready when the camera boots.

Common causes:

- rover AP starts too late
- weak signal
- wrong credentials
- power instability during Wi-Fi startup

### 6. Browser works in rescue mode but not on rover network

That usually indicates a rover-side networking issue:

- wrong gateway/subnet on the rover
- another device already using `192.168.4.2`
- firewall, captive portal, or routing logic on the controller side

---

## Design rationale

The sketch intentionally avoids running AP and STA together during the normal boot path.

Benefits:

- more deterministic network behavior
- reduced startup complexity
- lower risk of transient current spikes
- easier field troubleshooting
- clearer separation between **normal operation** and **recovery mode**

This is not a generic Wi-Fi provisioning sketch. It is a camera node tailored to a rover system.

---

## Limitations

- hardcoded Wi-Fi credentials
- hardcoded static IP scheme
- remote Wi-Fi updates disabled
- behavior depends on the linked camera web server implementation
- designed for a specific rover network topology, not for general consumer deployment

---

## Suggested repository description

**ESP32-CAM rover camera firmware with static STA networking, rescue AP fallback, and automatic reconnection.**

---

## Roadmap ideas

Possible future improvements:

- move credentials to non-volatile configuration storage
- add a status page with current mode and RSSI
- expose health/diagnostic JSON endpoints
- add watchdog-backed recovery logic
- add optional authenticated configuration mode

---

## License

Add the license that matches your repository, for example:

- MIT
- GPL-3.0
- Apache-2.0

If you do not already have one, add a `LICENSE` file at the repository root.
