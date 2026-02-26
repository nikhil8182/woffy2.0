# Woffy Mini Rover AI Pet Dog App - Design

**Date:** 2026-02-16

---

## Overview

A mobile app to control and interact with the Woffy Mini AI rover that behaves like a friendly pet dog.

---

## Features

### 1. Quick Actions (Home Screen)
- D-pad style controller: Forward, Back, Left, Right, Spin
- Large, colorful touch-friendly buttons
- Status bar: Battery %, Connection type, Signal strength
- Dog animation feedback on button press

### 2. Live Camera
- Full-screen video feed from rover
- Overlay: Rover name, timestamp, GPS coordinates
- Screenshot & Record buttons
- Night vision toggle

### 3. AI Chat
- Message bubble interface (user vs rover)
- Voice input (hold to talk)
- Text input field
- Quick phrase buttons: "Good boy!", "Sit", "Who are you?", "Take a photo"
- Friendly dog personality responses

### 4. Settings
- Connection mode: WiFi Direct / Bluetooth / Cloud
- Rover name customization
- Movement speed control
- AI personality options
- System info display

---

## Connection

- **WiFi Direct:** Local UDP/TCP for low-latency
- **Bluetooth:** BLE for close-range
- **Cloud:** MQTT for remote access
- Auto-detection of best connection

---

## Navigation

Tab-based with 4 tabs:
1. Home (Quick Actions)
2. Camera
3. Chat
4. Settings
