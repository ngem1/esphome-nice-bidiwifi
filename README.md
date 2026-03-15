# Nice BiDi-WiFi ESPHome Component

An [ESPHome](https://esphome.io/) external component that replaces the factory firmware on the **Nice BiDi-WiFi** module (ESP32-WROOM-32E), enabling local control of Nice gate automations via Home Assistant — no cloud required.

## Features

- **Full gate control** — Open, Close, Stop, Partial Open, Lock/Unlock, Light Toggle
- **Real-time position tracking** — Encoder-based position with percentage display
- **Configurable parameters** — Force, Pause Time via Home Assistant number entities
- **Feature switches** — Auto Close, Photo Close, Always Close, Pre-Flash, Key Lock
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

Other Nice controllers with IBT4N/BusT4 interface should work (Walky, Robus, Road 400, etc.) with device-specific adaptations built in:
- **Walky (WLA)** — 1-byte position values (auto-detected)
- **Robus (ROB)** — Position polling disabled during movement (auto-detected)
- **Road 400** — Alternate status codes 0x83/0x84 handled

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

### Flashing

1. **Remove** the BiDi-WiFi module from the gate control unit
2. **Connect** a USB-UART adapter (3.3V):
   - Adapter TX → Module RX (GPIO 3 / RXD0)
   - Adapter RX → Module TX (GPIO 1 / TXD0)
   - Adapter GND → Module GND
   - Adapter 3.3V → Module 3.3V
3. **Hold** the BOOT button (GPIO 0) while powering on to enter flash mode
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
