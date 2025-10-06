# ESP32 Door Lock Monitoring System

A door lock monitoring system based on ESP32-S3 and A3144 Hall sensor.

## Features

- ✅ Real-time door lock status monitoring with Hall sensor
- ✅ WiFi connection with auto-reconnect
- ✅ MQTT remote status reporting and control
- ✅ Buzzer status alerts (3 short beeps)
- ✅ LED status indication

## Hardware Requirements

- ESP32-S3 development board
- A3144 Hall effect sensor
- Active buzzer
- LED indicator
- Magnet

### Pin Connections

| Component | GPIO | Description |
|-----------|------|-------------|
| Hall Sensor | GPIO 5 | Digital input with pull-up |
| Buzzer | GPIO 12 | Digital output |
| LED | GPIO 15 | Digital output |

## Software Requirements

- ESP-IDF v5.5.1 or higher
- Python 3.7+

## Setup Instructions

### 1. Clone the Repository

```bash
git clone <your-repo-url>
cd door_locking_esp32native
```

### 2. Configure Private Information

Copy the example configuration file and modify it with your actual credentials:

```bash
cp main/config.h.example main/config.h
```

Then edit `main/config.h` and update the following values:

```c
// WiFi Configuration
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// MQTT Configuration
#define MQTT_SERVER   "your_mqtt_broker_ip"
#define MQTT_PORT     1883
#define MQTT_USERNAME "your_mqtt_username"
#define MQTT_PASSWORD "your_mqtt_password"
```

**⚠️ Important: `config.h` is added to `.gitignore` and will not be committed to the Git repository.**

### 3. Build the Project

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh  # Linux/Mac
# or
C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1  # Windows

# Build
idf.py build
```

### 4. Flash to Device

```bash
idf.py -p COM7 flash monitor  # Windows
# or
idf.py -p /dev/ttyUSB0 flash monitor  # Linux
```

## MQTT Topics

### Publish Topics (Device → Server)

- `esp32/lock/state` - Door lock status
  - `OPEN` - Door opened (magnet removed)
  - `CLOSED` - Door closed (magnet detected)

### Subscribe Topics (Server → Device)

- `esp32/lock/cmd` - Remote control commands
  - `BEEP` - Buzzer beeps 5 times
  - `STOP` - Stop buzzer

## System Behavior

### Door Closed (Magnet Near Sensor)
1. Hall sensor detects magnetic field
2. LED turns on
3. Buzzer beeps 3 times (200ms interval)
4. Publishes `CLOSED` status via MQTT

### Door Opened (Magnet Removed)
1. Hall sensor detects magnetic field disappears
2. LED turns off
3. Buzzer beeps 3 times (200ms interval)
4. Publishes `OPEN` status via MQTT

## Troubleshooting

### Compilation Errors

Make sure you have:
1. Correctly installed ESP-IDF v5.5.1
2. Created and configured `main/config.h` file
3. Run the ESP-IDF environment setup script

### WiFi Connection Issues

Check if the WiFi SSID and password in `main/config.h` are correct.

### MQTT Connection Issues

1. Confirm MQTT broker is running
2. Check if MQTT broker IP address is correct
3. Verify MQTT username and password
4. Ensure firewall allows port 1883

### System Keeps Restarting

This might be a stack overflow issue. Try increasing `HALL_TASK_STACK_SIZE` in `config.h`.

## Development Notes

### Project Structure

```
door_locking_esp32native/
├── main/
│   ├── main.c              # Main program entry
│   ├── config.h.example    # Configuration template
│   ├── config.h            # Actual config (not committed to Git)
│   ├── wifi_manager.c/h    # WiFi management
│   ├── hall_sensor.c/h     # Hall sensor driver
│   ├── buzzer.c/h          # Buzzer control
│   └── CMakeLists.txt      # Component configuration
├── CMakeLists.txt          # Project configuration
├── .gitignore              # Git ignore file
└── README.md               # This file
```

### Task Priorities

- Hall Sensor Task: Priority 6 (Highest)
- WiFi Task: Priority 5
- MQTT Task: Priority 4
- Buzzer Task: Priority 3

### Key Implementation Details

- **Direct ESP-IDF MQTT API**: Uses ESP-IDF's native MQTT client without wrapper layers
- **Interrupt-based Hall Sensor**: GPIO interrupt with software debouncing (100ms)
- **Non-blocking Buzzer**: State machine implementation for precise beep sequences
- **FreeRTOS Tasks**: Separate tasks for Hall sensor monitoring, WiFi management, and buzzer control

## Technical Specifications

- **ESP-IDF Version**: v5.5.1
- **Target**: ESP32-S3
- **RTOS**: FreeRTOS
- **Communication**: MQTT over TCP
- **WiFi**: 2.4GHz 802.11 b/g/n

## Known Issues

- None at the moment
