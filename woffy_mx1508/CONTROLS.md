# Woffy 2.0 - Control Guide

## Connection Status
- **Web UI**: Shows Wi-Fi & MQTT status (green = connected, red = not connected)
- **Serial Monitor**: 115200 baud

---

## Control Methods

### 1. Web Browser
Open the IP address (check Serial Monitor) or connect to AP `Woffy2`.

### 2. MQTT Commands
| Command | Action |
|---------|--------|
| `fwd` or `forward` | Move forward |
| `bwd` or `backward` | Move backward |
| `left` | Turn left |
| `right` | Turn right |
| `stop` | Stop all motors |
| `speed:180` | Set speed (60-255) |
| `status` | Get connection status |

**Subscribe**: `woffy/command`
**Publish**: `woffy/status`

### 3. Serial Monitor
Same commands as MQTT at 115200 baud.

---

## Wi-Fi Setup
1. Connect to AP `Woffy2` (password: `woffy1234`)
2. Open `http://192.168.4.1/setup`
3. Scan and select your Wi-Fi network
4. Enter password and connect

---

## MQTT Broker
- **Server**: `mqtt.onwords.in`
- **Port**: `1883`
- **Username**: `Nikhil`
- **Password**: `Nikhil8182`

---

## Pin Layout (L293D)
- FL1=25, FL2=26 (Front Left)
- FR1=27, FR2=14 (Front Right)
- RL1=18, RL2=19 (Rear Left)
- RR1=21, RR2=22 (Rear Right)
