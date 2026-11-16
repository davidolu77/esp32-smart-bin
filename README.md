# ESP32 Smart Bin

PlatformIO firmware for a proximity-operated bin lid. A front-facing VL53L0X
detects a hand, an HC-SR04 estimates how full the bin is, and an ESP32 drives a
servo, status LED, and physical button. The device also offers a local HTTP API
and OTA firmware updates. Local lid control continues if Wi-Fi is unavailable.

This repository contains **only the ESP32 firmware**. Any Raspberry Pi, Bob,
OpenClaw, or Telegram integration is a separate project and is not required to
build or operate the bin.

## Hardware and wiring

- ESP32-WROOM-32-style development board (`esp32dev` PlatformIO target)
- VL53L0X time-of-flight sensor facing outwards
- HC-SR04 ultrasonic sensor pointing down into the bin
- Small hobby servo, LED with current-limiting resistor, and pushbutton

| Connection | ESP32 GPIO | Note |
| --- | ---: | --- |
| VL53L0X SDA | 21 | Firmware calls `Wire.begin(21, 22)` |
| VL53L0X SCL | 22 | GPIO1 and XSHUT are unused |
| HC-SR04 TRIG | 5 | Sensor is powered from 5 V |
| HC-SR04 ECHO | 18 | **Use an external 5 V-to-3.3 V resistor divider** |
| Servo signal | 13 | Provide suitable servo power |
| Status LED | 2 | On when lid is open |
| Pushbutton | 4 | Connect to GND; internal pull-up is enabled |

All modules need a common ground. **ESP32 GPIO is 3.3 V logic and must not
receive the HC-SR04's 5 V ECHO output directly.** Servo current spikes can
reset an ESP32; use appropriate power regulation and decoupling. The power
supply and mechanical mounting must be designed for your own build.

## Lid and fill behavior

The VL53L0X is sampled every 100 ms. A target within **180 mm** for **150 ms**
opens the lid. At **250 mm** or farther, the clear timer starts; the lid closes
after **3 seconds** of clear area. The two thresholds provide hysteresis. A
target returning within the trigger range cancels the close timer. Distances
beyond **500 mm** and timeouts are treated as no nearby target. Three
consecutive malformed readings are required before an invalid ToF state is
reported.

The HC-SR04 is sampled every 250 ms. The temporary calibration maps **28 cm to
0%** and **4 cm to 100%**, clamped to that range. Measure your own bin's empty
and full sensor gaps and adjust `BIN_EMPTY_DISTANCE_CM` and
`BIN_FULL_DISTANCE_CM` in `include/Config.h`. No echo or implausible distance is
reported as **unknown**, never as full.

The button toggles the lid and returns persistent manual control to AUTO. The
LED is on while the lid is open and off while closed. Tunable pins, thresholds,
angles, and timing values are grouped in `include/Config.h`.

## Build and configure

1. Install [VS Code](https://code.visualstudio.com/) and the PlatformIO IDE
   extension, or use PlatformIO Core.
2. Open this folder, which contains `platformio.ini`.
3. Copy `include/secrets.example.h` to `include/secrets.h` and replace all
   placeholders with **your own** Wi-Fi SSID, password, and long random API
   token. `include/secrets.h` is ignored by Git; never commit or share it.
4. Build with `pio run -e esp32dev` or PlatformIO's **Build** button.
5. Connect the ESP32 by USB, find its port with `pio device list`, then use
   `pio run -e esp32dev -t upload --upload-port COMx` (replace `COMx`).
6. View logs with `pio device monitor --port COMx --baud 115200`.

The `esp32dev` environment uses the Arduino framework, ESP32Servo, and the
Pololu VL53L0X library. USB upload uses `esptool`. Placeholder Wi-Fi values
disable networking while local sensing and button control continue. A
placeholder API token disables state-changing requests and OTA.

## Local HTTP API

The ESP32 uses hostname `esp32-smart-bin` and advertises
`http://esp32-smart-bin.local` through mDNS. If `.local` does not resolve, use
the IP printed in Serial. `GET` endpoints are readable on the local network;
all state-changing `POST` endpoints require `X-API-Key`.

| Method | Path | Behavior |
| --- | --- | --- |
| GET | `/health` | Small device-alive JSON response |
| GET | `/status` | Firmware, lid, mode, ToF, fill, RSSI, and uptime JSON |
| POST | `/open` | Open temporarily; remain in AUTO |
| POST | `/close` | Close immediately; remain in AUTO |
| POST | `/hold-open` | Persistently hold the lid open (`manual_open`) |
| POST | `/hold-closed` | Persistently hold the lid closed (`manual_closed`) |
| POST | `/auto` | Restore automatic proximity control |

For example, `curl http://esp32-smart-bin.local/status` reads status. To issue
a control request, send your configured private token in the `X-API-Key`
header, for example `curl -X POST -H "X-API-Key: YOUR_PRIVATE_TOKEN"
http://esp32-smart-bin.local/open`. Avoid putting a real token in a shared
terminal history or screenshots. In `/status`, `mode` is `auto`, `manual_open`,
or `manual_closed`; temporary commands continue to report `auto`. Invalid fill
readings yield `fill_valid: false` and null distance/percentage fields.

## OTA updates and security

The first network-capable firmware must be uploaded over USB. Later updates
can use the `esp32dev-ota` environment, which targets
`esp32-smart-bin.local` via `espota`. OTA uses the same private API token as its
password. The standard `esp32dev` partition layout has two OTA application
slots; this project does not use a custom partition table.

For PlatformIO Core, set `PLATFORMIO_UPLOAD_FLAGS` to
`--auth=YOUR_PRIVATE_TOKEN`, run `pio run -e esp32dev-ota -t upload`, then
clear that environment variable. Set the value privately for the current
shell session; do not store it in `platformio.ini` or publish terminal output.
PlatformIO's OTA debug output can include the authentication argument. If
that output is shared, rotate the token and install the new token over USB.

The OTA environment fixes the computer-side callback port at TCP **32323**.
Windows Firewall may need an inbound rule for that port restricted to the
trusted local subnet. If OTA fails, connect by USB, close Serial Monitor to
release the port, and upload with the `esp32dev` environment.

**Keep the device and its control API on a trusted local network. Do not
forward ports to it or expose its HTTP or OTA services directly to the public
internet.** The HTTP API is not TLS-protected; the token is appropriate for a
trusted local network, not an untrusted one. `/status` is intentionally
unauthenticated and reports sensor and state information to local clients.

## Limitations and future work

- The fill calibration is provisional and depends on actual bin geometry and
  rubbish surfaces.
- The HC-SR04 read uses `pulseIn()` with a timeout of up to 30 ms; the loop is
  primarily, but not completely, non-blocking.
- The servo supply, enclosure, linkage, and battery/charging system need to
  be designed and verified for each physical build.
- Raspberry Pi/Bob integration, cloud services, MQTT, and Bluetooth are not
  part of this firmware.

