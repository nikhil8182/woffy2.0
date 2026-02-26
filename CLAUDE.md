# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Woffy 2.0 is a 4WD robot (ESP32 + 2x L293D motor drivers + 4x N20 motors) controlled over Wi-Fi and MQTT. The repo has three independent sub-projects:

1. **Firmware** (`woffy_mx1508/`) — Arduino/ESP32 sketch uploaded via Arduino IDE
2. **Web App** (`web-app/`) — Next.js 16 + React 19 + Tailwind CSS 4 (scaffold, work in progress)
3. **iOS App** (`mobile app/woffy mini/`) — SwiftUI app with MQTT and MiniMax AI chat

---

## Architecture

### Control Flow

Commands reach the robot through two parallel paths:
- **MQTT**: publish to `woffy/command` → `mqtt.onwords.in:1883` → ESP32 subscribes and executes
- **HTTP**: ESP32 hosts its own web UI at `192.168.4.1` (AP mode) or its STA IP; the UI POSTs to `/cmd?cmd=<command>`

Status is published back to `woffy/status`.

### Firmware Structure (`woffy_mx1508/`)

| File | Role |
|------|------|
| `woffy_mx1508.ino` | Entry point: Wi-Fi STA connect, MQTT setup, serial handler, main loop |
| `motors.h` | All motor logic: 8 GPIO PWM pins via `ledcAttach`/`ledcWrite` |
| `config.h` | Wi-Fi SSID/pass, MQTT broker, topics, credentials |
| `html.h` | Complete web UI stored in ESP32 flash as a `PROGMEM` raw string literal |

Motors are driven directly with PWM pairs (pin1=speed, pin2=0 for forward; reversed for backward). Default speed is PWM 180 (~70% / ~210 RPM). Speed range is 60–255.

### GPIO Pin Map (L293D)
- Front-Left: GPIO 25 / 26 — Front-Right: GPIO 27 / 14
- Rear-Left: GPIO 18 / 19 — Rear-Right: GPIO 21 / 22

### iOS App (`mobile app/woffy mini/`)

- `ContentView.swift` — Full SwiftUI app including `RoverState` (ObservableObject), MQTT client, network scanner, D-pad UI, and AI chat tab
- `MiniMaxChatService.swift` — MiniMax API integration (model: `MiniMax-M2.5`) for the AI assistant tab
- MQTT broker settings default to `mqtt.onwords.in:1883`, topic `woffy/command`

---

## Development Commands

### Firmware (Arduino IDE)
- Open `woffy_mx1508/woffy_mx1508.ino` in Arduino IDE
- Board: **ESP32 Dev Module**
- Serial monitor: **115200 baud**
- Upload: Use Arduino IDE's Upload button

### Serial Monitor (macOS, Python)
```bash
python3 sm.py      # auto-detects /dev/cu.usbserial-0001 or -0002
# or
python3 sm          # fixed to /dev/cu.usbserial-0001
```

### Web App
```bash
cd web-app
npm install
npm run dev        # development server
npm run build      # production build
npm run lint       # ESLint
```

---

## Serial / MQTT Commands

| Command | Action |
|---------|--------|
| `fwd` / `forward` / `w` | Forward |
| `bwd` / `backward` / `s` | Backward |
| `left` / `a` | Spin left |
| `right` / `d` | Spin right |
| `stop` / `x` | Stop |
| `speed:180` | Set speed (60–255) |
| `status` | Publish IP/WiFi/MQTT status |
| `demo` | 2s forward, pause, 5s forward |

---

## MQTT Broker
- Broker: `mqtt.onwords.in:1883`
- Command topic: `woffy/command`
- Status topic: `woffy/status`
- Credentials: in `config.h` and `ContentView.swift` (`RoverState`)

## LED Status (ESP32 onboard LED, GPIO 2, inverted)
- Solid ON: MQTT connected
- 500ms blink: MQTT stable, connected
- Fast blink: reconnecting
- 2 blinks on boot: Wi-Fi OK
- 5 fast blinks on boot: Wi-Fi failed
