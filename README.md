# Nice BiDi-WiFi ESPHome Component

An [ESPHome](https://esphome.io/) external component that replaces the factory firmware on the **Nice BiDi-WiFi** module (ESP32-WROOM-32E), enabling local control of Nice gate automations via Home Assistant — no cloud required.

## Features

- **Full gate control** — Open, Close, Stop, Partial Open, Lock/Unlock, Light Toggle
- **Real-time position tracking** — Encoder-based position with percentage display
- **Configurable parameters** — Force, Pause Time via Home Assistant number entities
- **Feature switches** — Auto Close, Photo Close, Always Close, Standby, Pre-Flash, Key Lock
- **Diagnostics** — Photocell, limit switches, obstacle detection, stop reason, maneuver counters
- **Status LEDs** — Gate state (LED1), bus connectivity (LED2), overall status (LED3)
- **OTA updates** — Wireless firmware updates via ESPHome
- **Web interface** — Built-in web server for status and control
- **Home Assistant** — Full native integration with auto-discovery

## Supported Hardware

### BiDi-WiFi Module
The [Nice BiDi-WiFi](https://www.niceforyou.com/) module is an ESP32-WROOM-32E board that plugs into the IBT4N connector on Nice control units. This component replaces its factory firmware.

### Tested Control Units
| Controller | Type | Status |
|-----------|------|--------|
| **MC800** | Swing gate | Fully tested and working |
| **CL201** | Swing gate | Tested and working |

Other Nice controllers with IBT4N/BusT4 interface should work (Walky, Robus, Road 400, etc.) with device-specific adaptations built in:
- **Walky (WLA)** — 1-byte position values (auto-detected)
- **Robus (ROB)** — Position polling disabled during movement (auto-detected)
- **Road 400** — Alternate status codes 0x83/0x84 handled

## How position tracking works

The hub layers several mechanisms so the cover and percentage sensors stay useful even when encoders are partial or noisy. The idea matches what other Nice Bus-T4 ESPHome projects document—for example [makstech/esphome-BusT4](https://github.com/makstech/esphome-BusT4)—with behaviour tuned to this codebase.

### 1. Encoder position (primary when fresh)

When the controller returns current/encoder position, it updates the cover and optional encoder sensor. While the gate is **opening** or **closing**, the hub polls encoder/current position on an interval (default **500 ms**, configurable with **`position_report_interval`** on the cover; values are clamped between 50 ms and 60 s).

Encoder-derived position is preferred only when a reading arrived within the last **2 seconds**; if it is older, time-based estimation may be used instead.

**Robus** units are detected from the product name; for those, position is **not** polled during movement because the drive does not handle it reliably.

**Walky** controllers use **1-byte** position values; most others use two-byte values (and some use a three-byte layout).

### 2. Time-based estimation (fallback)

If encoder data is unavailable or stale during a move, position is estimated from elapsed time and **learned** open/close durations:

- Durations are captured on **complete** movements (e.g. fully closed → fully open) and **persisted** across reboots when **`auto_learn_timing`** is true (default). With **`auto_learn_timing: false`**, flash is not updated and **`open_duration`** / **`close_duration`** in YAML define the timings used for time-based estimation.
- Before the first successful learn (or when a direction has no stored value yet), **`open_duration`** and **`close_duration`** act as fallbacks (defaults **20 s** each, same idea as [esphome-BusT4](https://github.com/makstech/esphome-BusT4)).
- Only durations between about **3 seconds** and **5 minutes** are accepted; interrupted cycles do **not** update stored timings.
- If a new measurement differs from the stored duration by more than about **10%**, the stored value is updated (adaptive re-learning).

### 3. Limit switch confirmation

Diagnostic **I/O** (`REG_DIAG_IO`) is read after relevant status changes. Limit-switch bits are used to snap position to **fully open** or **fully closed** when the automation status matches (`STA_OPENED` / `STA_CLOSED`), which corrects small drift in the time-based estimate.

### 4. Periodic status refresh

Roughly every **15 seconds** the hub requests **status** again (and maneuver count, using `REG_TOTAL_COUNT` or `REG_NUM_MOVEMENTS` depending on the controller). That recovers from missed packets and picks up changes from remotes or the panel.

## Hardware Setup

### Pin Mapping (BiDi-WiFi Module)

| Function | GPIO | Notes |
|----------|------|-------|
| T4 Bus RX | 18 | Direct connection to T4 bus (BAT54A protection) |
| T4 Bus TX | 21 | Via BC847 open-collector driver |
| LED1 Red | 25 | Gate status (active-low) |
| LED1 Green | 2 | Gate status (active-low, strapping pin) |
| LED2 Red | 27 | Bus connectivity (active-low) |
| LED2 Green | 26 | Bus connectivity (active-low) |
| LED3 | 22 | Overall status (active-low) |
| Reset Button | 5 | — |

### Wiring

Connect a USB-UART adapter (3.3V) to the BiDi-WiFi module:

| Adapter | Module | Notes |
|---------|--------|-------|
| TX | RX (GPIO 3 / RXD0) | |
| RX | TX (GPIO 1 / TXD0) | |
| GND | GND | |
| 3.3V | 3.3V | Do NOT use 5V |
| RTS | EN | Auto-reset (optional) |
| DTR | GPIO 0 | Auto-boot mode (optional) |

### Backup Factory Firmware

**Always backup the original firmware before flashing!** This allows you to restore the Nice factory firmware if needed.

```bash
esptool.py --port COMx --baud 921600 read_flash 0x0 0x800000 bidiwifi-factory-backup.bin
```

This reads the full 8MB flash (bootloader, partitions, app, NVS, SPIFFS). Store the backup safely.

To restore the factory firmware later:
```bash
esptool.py --port COMx --baud 921600 write_flash 0x0 bidiwifi-factory-backup.bin
```

### Flashing

1. **Remove** the BiDi-WiFi module from the gate control unit
2. **Connect** the USB-UART adapter as described above
3. **Enter flash mode:**
   - If your adapter has RTS/DTR connected, `esptool` handles this automatically
   - If not, manually: short **GPIO 0 to GND**, then briefly short **EN to GND** to reset, then release GPIO 0
4. **Flash**: `esphome run nice-bidiwifi-gate.yaml --device COMx`
5. **Reinstall** the module into the control unit's IBT4N connector

After the first flash, subsequent updates can be done via OTA (wireless).

### LED Indicators

**LED1 (Bicolor) — Gate Status:**
| Color | Meaning |
|-------|---------|
| Green (steady) | Gate open |
| Red (steady) | Gate closed |
| Yellow (both) | Gate moving |
| Red (blinking) | Stopped mid-travel or obstacle |

**LED2 (Bicolor) — Connectivity:**
| Color | Meaning |
|-------|---------|
| Green (steady) | Connected to controller, idle |
| Yellow (flash) | Bus activity (TX/RX) |
| Red | No device found |

**LED3 — Status:**
| Pattern | Meaning |
|---------|---------|
| Solid | Fully operational |
| Slow blink | Initializing |
| Fast blink | Searching for device |

## Software Setup

### Prerequisites
- [ESPHome](https://esphome.io/guides/installing_esphome) 2024.x or later
- ESP-IDF framework (configured in YAML)

### Installation

Add to your ESPHome YAML:

```yaml
external_components:
  - source: github://YOUR_USERNAME/esphome-nice-bidiwifi
    components: [nice_bidiwifi]
```

Or for local development:

```yaml
external_components:
  - source: components
```

### Example Configuration

```yaml
esphome:
  name: nice-bidiwifi
  friendly_name: "Nice Gate"

esp32:
  board: esp32dev
  framework:
    type: esp-idf

external_components:
  - source: components

logger:
  level: DEBUG

api:
  encryption:
    key: !secret api_key

ota:
  - platform: esphome
    password: !secret ota_password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    password: !secret fallback_password

captive_portal:

web_server:
  port: 80
  version: 3

# Nice BiDi-WiFi hub (T4 bus communication)
nice_bidiwifi:
  id: my_gate
  uart_rx_pin: 18
  uart_tx_pin: 21
  # hub_address: 0x5090  # default, change only if needed

# Gate cover entity
cover:
  - platform: nice_bidiwifi
    name: "Gate"
    device_class: gate
    id: gate
    auto_learn_timing: true
    open_duration: 20s
    close_duration: 20s
    position_report_interval: 500ms

# Control buttons
button:
  - platform: nice_bidiwifi
    name: "Step by Step"
    command: sbs

  - platform: nice_bidiwifi
    name: "Partial Open 1"
    command: partial_open_1

  - platform: nice_bidiwifi
    name: "Lock"
    command: lock

  - platform: nice_bidiwifi
    name: "Unlock"
    command: unlock

  - platform: nice_bidiwifi
    name: "Light Toggle"
    command: light_toggle

# Feature switches
switch:
  - platform: nice_bidiwifi
    auto_close:
      name: "Auto Close"
    photo_close:
      name: "Close After Photo"
    always_close:
      name: "Always Close"
    standby:
      name: "Standby"
    pre_flash:
      name: "Pre-Flash Warning"
    key_lock:
      name: "Key Lock"

# Sensors
sensor:
  - platform: nice_bidiwifi
    position:
      name: "Gate Position"
    encoder_value:
      name: "Encoder Raw"
    maneuver_count:
      name: "Total Maneuvers"
    maintenance_count:
      name: "Maintenance Counter"

# Info text sensors
text_sensor:
  - platform: nice_bidiwifi
    product:
      name: "Product"
    firmware_version:
      name: "Firmware"
    hardware_version:
      name: "Hardware"
    gate_status:
      name: "Status"
    manufacturer:
      name: "Manufacturer"
    last_stop_reason:
      name: "Last Stop Reason"

# Diagnostics
binary_sensor:
  - platform: nice_bidiwifi
    photocell:
      name: "Photocell"
    limit_switch_open:
      name: "Limit Open"
    limit_switch_close:
      name: "Limit Close"
    obstacle:
      name: "Obstacle Detected"
    locked:
      name: "Gate Locked"

# Configuration numbers
number:
  - platform: nice_bidiwifi
    pause_time:
      name: "Pause Time"
    opening_force:
      name: "Opening Force"
    closing_force:
      name: "Closing Force"
```

### Cover platform options

| Option | Default | Description |
|--------|---------|-------------|
| `auto_learn_timing` | `true` | Learn open/close duration from completed moves and persist to flash. If `false`, YAML `open_duration` / `close_duration` are used and preferences are not updated from the bus. |
| `open_duration` | `20s` | Initial/fallback duration for time-based position (clamped to the same 3 s–5 min band as learning). |
| `close_duration` | `20s` | Same for closing. |
| `position_report_interval` | `500ms` | How often to request encoder/current position while the gate is moving (hub clamps to 50 ms–60 s). |
| `invert_open_close` | `false` | Swap open/close commands and reported position when the physical install is reversed. |
| `supports_position` | `true` | Expose a position slider in Home Assistant; set `false` if the encoder is unusable and you rely on open/closed state only. |

### Available Button Commands

| Command | Description |
|---------|-------------|
| `sbs` | Step by Step (toggle) |
| `open` | Open |
| `close` | Close |
| `stop` | Stop |
| `partial_open_1` | Partial open position 1 |
| `partial_open_2` | Partial open position 2 |
| `partial_open_3` | Partial open position 3 |
| `lock` | Lock gate |
| `unlock` | Unlock gate |
| `light_toggle` | Toggle courtesy light |
| `open_and_lock` | Open then lock |
| `close_and_lock` | Close then lock |

### Configuration Options

```yaml
nice_bidiwifi:
  uart_rx_pin: 18          # T4 bus RX (default: 18)
  uart_tx_pin: 21          # T4 bus TX (default: 21)
  hub_address: 0x5090      # Hub address (default: 0x5090)
  led1_red_pin: 25         # LED1 red (default: 25)
  led1_green_pin: 2        # LED1 green (default: 2)
  led2_red_pin: 27         # LED2 red (default: 27)
  led2_green_pin: 26       # LED2 green (default: 26)
  led3_pin: 22             # LED3 (default: 22)
```

### Number ranges (force, speed, pause, …)

Opening/closing **force** (`0x4A` / `0x4B`) and **speed** (`0x42` / `0x43`) are exposed as **1–100** with unit **%**, which matches the usual Nice Bus-T4 / DMP convention on many controllers. **This repo does not embed model-specific tables** (e.g. CL201): your drive may accept the full range, clamp silently, or **NACK** (`0xFD`) if a value is illegal. For authoritative limits, use the **official Nice programming/installation guide for your board**.

If you need to try values outside the number entities, or to replay a capture from logs, use **`send_raw_cmd`** on the hub (see below).

### Debugging: raw hex on the bus

The hub exposes **`send_raw_cmd(std::string)`**, same idea as [makstech/esphome-BusT4](https://github.com/makstech/esphome-BusT4): pass a hex string (`"55.0C.00.FF..."` or `"550C00FF..."`; spaces, dots, colons, and newlines are ignored). An **odd** digit count or invalid characters are rejected (nothing is sent).

Use a **template `text`** entity in Home Assistant and call **`id(<your_hub_id>).send_raw_cmd(x)`** (the hub id from `nice_bidiwifi:`, not the cover id). Put **`text:` at the root of the YAML** (same indentation as `esphome:`, `number:`, `wifi:`). If `text:` is indented under the `number:` block, the file will fail to parse with an error near that line.

Example:

```yaml
text:
  - platform: template
    name: "Raw T4 hex"
    id: raw_t4_hex
    optimistic: true
    mode: text
    on_value:
      then:
        - lambda: |-
            if (!x.empty()) {
              id(my_gate).send_raw_cmd(x);
            }
```

## T4 Bus Protocol

The Nice T4 bus protocol operates at 19200 baud 8N1 with UART break signaling:

- **Break**: ~520us low pulse before each packet (generated via baud rate switching)
- **Packet**: `[0x00][0x55][size][to:2][from:2][msg_type][msg_size][crc1][message...][crc2][size]`
- **CRC**: XOR-based (CRC1 for header, CRC2 for message body)
- **Protocols**: DEP (direct commands) and DMP (register read/write)
- **Hub address**: 0x5090 (default for BiDi-WiFi hardware)

### Architecture

```
Home Assistant ←→ ESPHome (ESP32) ←→ T4 Bus ←→ Nice Control Unit (MC800)
                   WiFi/API            UART         Gate Motor
```

The component uses a hub+platform pattern (similar to `modbus_controller`):
- **Hub** (`nice_bidiwifi`) — T4 bus driver, device discovery, packet parsing
- **Platforms** — Cover, Button, Switch, Number, Sensor, Text Sensor, Binary Sensor

## Acknowledgments

This project was inspired by and references the work of:

- **[pruwait/Nice_BusT4](https://github.com/nicedaemon/Nice_BusT4)** — Original ESP8266 ESPHome component for T4 bus communication. Pioneered the ESPHome approach.
- **[gashtaan/nice-bidiwifi-firmware](https://github.com/gashtaan/nice-bidiwifi-firmware)** — ESP32 Arduino firmware for BiDi-WiFi. Provided key protocol insights including the 0x5090 hub address.
- **[makstech/esphome-BusT4](https://github.com/makstech/esphome-BusT4)** — ESP32 ESP-IDF ESPHome component. Comprehensive implementation with OXI support.

This implementation is a **clean-room rewrite** — no code was copied from the above projects (which use GPL v3). Protocol knowledge was derived from cross-referencing multiple sources and live hardware testing.

### Protocol References
- Nice DMBM Integration Protocol documentation
- MyNicePro APK reverse engineering (protocol layer analysis)
- Live packet analysis on MC800 hardware

## License

MIT License — see [LICENSE](LICENSE) for details.

## Disclaimer

This project is not affiliated with, endorsed by, or connected to Nice S.p.A. in any way. "Nice", "BiDi-WiFi", "MC800", and other product names are trademarks of Nice S.p.A. Use at your own risk — modifying gate automation firmware can affect safety features. Always ensure proper safety measures are in place.
