# ESP32-CAM camera firmware for rover

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Board](https://img.shields.io/badge/board-AI%20Thinker%20ESP32--CAM-00979D)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![Networking](https://img.shields.io/badge/networking-STA%20%2B%20rescue%20AP-success)
![Status](https://img.shields.io/badge/status-Project--specific-orange)

Firmware for a rover-mounted camera based on the **AI Thinker ESP32-CAM**.

This firmware is designed for a **fixed rover network topology**:

- the camera normally connects to the rover in **Wi-Fi STA mode**,
- the camera always uses a **static IP** inside the rover network,
- a **rescue AP** is only started when the rover is not reachable,
- the firmware periodically retries the rover connection and exits rescue mode automatically.

The goal of this design is to provide deterministic behavior, lower startup complexity, and easier field recovery.

---

## Highlights

- Targeted at **AI Thinker ESP32-CAM**
- Camera initialized in **JPEG** mode with default **VGA** frame size
- Fixed **STA IP** on the rover network: `192.168.4.2`
- **Rescue AP** on a different subnet: `192.168.8.1`
- Automatic reconnection every **10 seconds** while in rescue mode
- Configuration adapted to PSRAM availability
- Optional flash LED support through `setupLedFlash()`
- Avoids `WIFI_AP_STA` during normal startup to reduce complexity and current spikes

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
| Netmask | `255.255.255.0` |

### Rescue mode

If the rover is not available at boot, the camera creates its own access point.

| Parameter | Value |
|---|---|
| Rescue SSID | `CAMARA_RESCATE` |
| Rescue password | `12341234` |
| Rescue AP IP | `192.168.8.1` |
| Netmask | `255.255.255.0` |

### ASCII network diagram

```text
Normal operation
----------------
Phone / PC / Rover control
               |
               v
   [ Rover AP / Gateway ]   SSID: ROVER_DIEGO
          192.168.4.1
               |
               v
      [ ESP32-CAM camera ]
          192.168.4.2

Rescue operation
----------------
Phone / PC
    |
    v
[ ESP32-CAM rescue AP ]   SSID: CAMARA_RESCATE
         192.168.8.1
```

A separate rescue subnet is used to avoid routing ambiguity with the rover network.

---

## Boot and runtime behavior

### Boot sequence

1. Starts serial debug output at `115200` baud.
2. Configures AI Thinker camera pins through `camera_pins.h`.
3. Initializes the camera sensor and frame buffers.
4. Applies board/sensor-specific adjustments when needed.
5. Initializes the flash LED if `LED_GPIO_NUM` exists.
6. Tries to connect to the rover in **STA mode**.
7. If STA fails, starts the **rescue AP**.
8. Starts the camera web server.

### Runtime logic

- If STA is connected, the camera stays on the rover network.
- If STA is not available, the rescue AP keeps the camera reachable.
- Every `10000 ms`, the firmware retries the connection to the rover.
- If STA becomes available again, the rescue AP is automatically disabled.
- If an already-established STA connection is lost, the firmware returns to the reconnection cycle.

---

## Camera configuration

The sketch configures the camera as follows:

- **Model:** `AI Thinker ESP32-CAM`
- **Pixel format:** `JPEG`
- **Default frame size:** `VGA`
- **XCLK:** `10 MHz`
- **Default framebuffer location:** PSRAM when available
- **Default JPEG quality:** `12`
- **PSRAM mode:** improved quality and double buffering when present

When PSRAM is detected, the firmware switches to a more capable configuration:

- JPEG quality improved to `10`
- frame buffer count increased to `2`
- capture mode changed to `CAMERA_GRAB_LATEST`

If PSRAM is not available, the sketch uses a more conservative configuration.

---

## Accessing the web interface

Open the camera from a browser using one of these addresses:

- **Normal mode:** `http://192.168.4.2`
- **Rescue mode:** `http://192.168.8.1`

The exact interface and available endpoints depend on the camera server files linked into the build. In a standard ESP32 camera web server integration, the root page usually exposes preview, streaming, capture, and camera controls.

---

## Repository structure

A typical repository layout for this firmware could be:

```text
.
├── CameraWebServer.ino
├── camera_pins.h
├── app_httpd.cpp          # if you use the standard ESP32 camera UI/server
├── camera_index.h         # if your selected implementation needs it
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

- **Arduino IDE** or a compatible ESP32 Arduino workflow
- **Espressif ESP32 board package**
- **Selected board:** `AI Thinker ESP32-CAM`
- PSRAM enabled when applicable
- a partition scheme with enough space for camera firmware

### Required headers

- `esp_camera.h`
- `WiFi.h`
- `soc/soc.h`
- `soc/rtc_cntl_reg.h`

---

## Configuration values in the sketch

The current version is intentionally **project-specific** and uses embedded network values:

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

logs the request, but does not apply it.

This protects the fixed rover/camera topology against accidental remote changes.

---

## Serial diagnostics

The serial port provides useful information, including:

- Wi-Fi state transitions
- associated SSID and BSSID
- STA IP and gateway
- RSSI values
- rescue AP activation
- connection retries
- camera initialization errors

Recommended serial monitor speed:

```text
115200
```

---

## Troubleshooting

### 1. The camera does not connect to the rover

Check that:

- the rover AP is actually active at `192.168.4.1`
- the SSID and password match the sketch
- the rover subnet is `192.168.4.0/24`
- the static IP `192.168.4.2` is not already in use by another device

If the connection fails, the camera should expose the `CAMARA_RESCATE` AP at `192.168.8.1`.

### 2. The web page does not load even though ping works

This usually points to the **camera web server files** rather than the Wi-Fi link itself.

Check that the build includes the correct files implementing:

- `startCameraServer()`
- the HTTP handlers required by the selected UI/server
- `camera_index.h` or equivalent resources if your server requires them

### 3. Compilation error such as `index_handler was not declared`

This usually means that `app_httpd.cpp` or the associated UI files are incomplete, mixed from different versions, or out of sync.

Make sure all web server files belong to the same implementation set.

### 4. The board boots but camera initialization fails

Check that:

- the selected board is **AI Thinker ESP32-CAM**
- the camera pin mapping matches that board
- the power supply is stable
- PSRAM is configured correctly

### 5. The rescue AP appears too often

This usually indicates that STA association is unstable or that the rover is not yet ready when the camera boots.

Typical causes:

- the rover AP starts too late
- weak signal
- incorrect credentials
- power instability during Wi-Fi startup

### 6. The browser works in rescue mode but not on the rover network

This usually indicates a network problem on the rover side:

- incorrect gateway or netmask on the rover
- another device already using `192.168.4.2`
- firewall, captive portal, or routing logic on the controller side

---

## Design rationale

The sketch intentionally avoids running AP and STA at the same time in the normal boot path.

Advantages:

- more deterministic network behavior
- lower startup complexity
- less risk of transient current spikes
- simpler field diagnostics
- clearer separation between **normal operation** and **recovery mode**

This is not a generic Wi-Fi provisioning sketch. It is a camera node tailored to a specific rover system.

---

## Limitations

- embedded Wi-Fi credentials
- embedded static IP scheme
- remote Wi-Fi changes disabled
- behavior depends on the linked camera web server implementation
- designed for a specific rover network topology, not for generic consumer deployment

---

## Suggested repository description

**ESP32-CAM firmware for a rover camera with fixed STA networking, rescue AP, and automatic reconnection.**

---

## Future ideas

Possible improvements:

- move credentials to non-volatile storage
- add a status page showing current mode and RSSI
- expose JSON health/diagnostic endpoints
- add watchdog-backed recovery logic
- add an optional authenticated configuration mode

---

## License

Add the license that best fits your repository, for example:

- MIT
- GPL-3.0
- Apache-2.0

If you do not have one yet, add a `LICENSE` file at the repository root.
