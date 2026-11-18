# Smart Roller

Smart Roller is an ESP32-based controller for a motorized roller blind. I built the device and developed its firmware and browser interface. The ESP32 serves the control page directly over the local Wi-Fi network.

- Open or close the blind from a phone or computer on the same network.
- Set a position from 0% (closed) to 100% (open) and see the position reported by the controller.
- Schedule one-time or daily opening and closing.

## Hardware

| Component | Role |
| --- | --- |
| ESP32 development board (`esp32dev` in PlatformIO) | Hosts the web interface, runs the schedule, and controls the motor driver |
| TMC2208 stepper driver | Drives the motor using STEP and DIR signals |
| NEMA 17 stepper motor | Moves the roller blind |
| Roller blind and mechanical coupling | Transfer motor rotation to the blind |
| 12 V DC power supply | Powers the TMC2208 motor supply input |

### Control wiring

| ESP32 GPIO | Motor driver input | Function |
| --- | --- | --- |
| GPIO 18 | STEP | Step pulses |
| GPIO 19 | DIR | Direction |
| GPIO 27 | EN | Driver enable, active low |

### Motor and power wiring

| Connection | Destination |
| --- | --- |
| 12 V DC positive | TMC2208 module motor supply input, typically labeled VM or VMOT |
| 12 V DC negative | TMC2208 module GND, shared with ESP32 GND |
| Motor coil A pair | TMC2208 coil A outputs |
| Motor coil B pair | TMC2208 coil B outputs |

Check the pin labels on your driver module and the coil pinout for your specific NEMA 17 motor. See the [TMC2208 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/TMC2202_TMC2208_TMC2224_datasheet_rev1.14.pdf) for signal definitions.

## Build and flash

Install PlatformIO Core or use the PlatformIO terminal in VS Code. Run the commands from the repository root with the ESP32 connected by USB.

1. Set `ssid` and `password` in your local copy of [`src/main.cpp`](src/main.cpp). Keep the credentials out of commits.
2. For the first USB upload, change `upload_protocol = espota` to `upload_protocol = esptool` in [`platformio.ini`](platformio.ini) and remove or comment out the `upload_port` line containing the IP address.
3. Build and upload the firmware and the LittleFS image separately.

   ```sh
   pio run -e esp32dev
   pio run -e esp32dev -t upload
   pio run -e esp32dev -t uploadfs
   pio device monitor --baud 115200
   ```

4. Reset the ESP32 if needed and read its IP address from the `Got IP:` message. If it differs from the address in `ESP_IP` in [`data/js/index.js`](data/js/index.js), update that constant and run `pio run -e esp32dev -t uploadfs` again. Close the serial monitor before uploading over USB.
5. From a device on the same Wi-Fi network, open `http://<ESP32-IP>/` in a browser.

To upload updates over Wi-Fi, set `upload_protocol = espota` and set `upload_port` to the ESP32's current IP address in `platformio.ini`. The firmware announces the OTA hostname `smart-roller`.

See the [PlatformIO ESP32 documentation](https://docs.platformio.org/en/latest/platforms/espressif32.html#uploading-files-to-file-system) for LittleFS uploads.

## Hardware test

I tested the controller on a real roller blind. From a phone browser, I opened and closed the blind, set an intermediate position, and checked that a scheduled action moved the blind at the configured time.

## Current implementation

- Position is estimated from motor steps. The firmware assumes the blind is closed at startup and uses `MAX_STEPS = 16000` for full travel. Set the travel limit for your mechanism before operating the blind.
- The schedule is stored in ESP32 memory and is cleared on restart. Scheduled times use NTP with a fixed UTC+3 offset. Add opening and closing as separate events when setting up both times.
- The web interface and firmware updates are intended for a trusted local network. The current firmware does not require authentication for either.

## Repository layout

| Path | Contents |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | Wi-Fi, HTTP server, motor control, scheduling, and OTA updates |
| [`src/Event.h`](src/Event.h) | Schedule event data structure |
| [`data/`](data/) | HTML, CSS, and JavaScript served from LittleFS |
| [`platformio.ini`](platformio.ini) | Board, framework, filesystem, and upload configuration |

The firmware uses Arduino for ESP32 and AccelStepper. The [Qt/QML companion app](https://github.com/Vegas7787/smart-roller-app) is maintained in a separate repository.
