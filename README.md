# RWA — MB001 ESP32-S3 Controllers

Arduino firmware for the two ESP32-S3-N16R8 boards in the MB001 motorcycle project.

| Folder | Board | Version | What it does |
|---|---|---|---|
| `MB001_Control/` | Control board | `0.5.0-link-bench` | Arming, faults, watchdogs, Xbox BLE input, USB diagnostics. Will drive the motors and read the IMU. |
| `MB001_Comms/` | Comms board | `0.1.0-comms-bench` | Wi-Fi bridge: relays text commands between the Python control script (UDP) and the control board (UART). |
| `verification/` | — | — | Host-side C++ tests for both boards. |

```
Python script  --Wi-Fi/UDP port 4210-->  Comms board  --UART GPIO17/18-->  Control board  -->  motors / IMU
```

The control board is the only board that can arm or move anything. The comms board only forwards commands.

Open each `.ino` in its own Arduino IDE window. Setup and operating notes:
- [Control board](MB001_Control/README.txt) (Bluepad32 board package, Xbox pairing)
- [Comms board](MB001_Comms/README.txt) (Wi-Fi setup, wiring, UDP protocol)

**Status:** bench builds only. Motor outputs and IMU readings are not implemented. The UART link between the boards carries diagnostics and stop/recovery commands only; Xbox still owns movement.

## Connecting the boards (UART)

The boards talk over a UART serial link: 115200 baud, 8N1, one text command per line. You need the control board on `0.5.0-link-bench` or later and the comms board on `0.1.0-comms-bench` or later.

### Wiring

Unplug both boards before wiring. You need 3 jumper wires, and **the two data wires cross over**:

```
   COMMS BOARD                 CONTROL BOARD
   GPIO17 (TX) ──────────────▶ GPIO18 (RX)
   GPIO18 (RX) ◀────────────── GPIO17 (TX)
   GND         ─────────────── GND
```

| Comms board pin | Control board pin | Purpose |
|---|---|---|
| GPIO17 (TX) | GPIO18 (RX) | Commands from the comms board to the control board |
| GPIO18 (RX) | GPIO17 (TX) | Replies from the control board to the comms board |
| GND | GND | Shared ground (**required**) |

- **Cross TX and RX.** Each board's TX (transmit) goes to the other board's RX (receive). Wiring 17 to 17 is the most common mistake.
- **The ground wire is required.** Without a shared ground the boards read random bytes.
- **Use the GPIO numbers printed on the board**, not header pin positions.
- **On the bench, power each board from its own USB cable.** Do not connect the 5V or 3V3 pins of the two boards together. Both boards use 3.3 V logic, so no level shifter is needed.
- **Keep the wires short** (under about 30 cm). On the bike, run the ground wire alongside the two signal wires.

### First bench test

1. **Upload the firmware.** On the comms board, copy `MB001_Comms/WifiSecrets.example.h` to `WifiSecrets.h` in the same folder first; the Wi-Fi details can be placeholders until step 7. (`WifiSecrets.h` is deliberately not in the repo, so your password never gets pushed. In the Arduino IDE you can make it with **⋯ → New Tab**.) Then upload `MB001_Comms` to it. Upload `MB001_Control` to the control board.
2. **Noise check before wiring.** Open the control board's Serial Monitor (115200 baud, Newline) and type `HEALTH` a few times over about 10 seconds. `frame_errors` should stay at 0, which shows the unconnected link pin isn't picking up noise.
3. **Wire the boards** as shown above, then plug in both USB cables. To watch both Serial Monitors at once, open each sketch in its own Arduino IDE window.
4. **Check the comms board's monitor.** Within about a second you should see `BRIDGE link=UP`. Then type:
   - `PING` → you should get `CTRL> PONG`
   - `STATUS` → you should get `CTRL> STATUS version=0.5.0-link-bench ...`
5. **Check the control board's monitor.** Type `HEALTH`. On the `LINK` line, `link_commands` should go up about once a second; those are the comms board's keepalive pings.
6. **Pull the link.** Unplug one data wire. About 2.5 seconds later the comms board should print `BRIDGE link=DOWN`. Plug it back in and check that `link=UP` returns.
7. **Wi-Fi.** Put your real network name and password in `MB001_Comms/WifiSecrets.h` (2.4 GHz only; a network that needs a username login won't work) and re-upload the comms board. Its monitor should print `WIFI connected ip=...`. From a laptop on the same network, run the Python test snippet in [`MB001_Comms/README.txt`](MB001_Comms/README.txt) using that IP. It should print a `BRIDGE ...` status line.
8. **Xbox unchanged.** On the control board, repeat the arming and stop checks from "PAIR AND CHECK" in [`MB001_Control/README.txt`](MB001_Control/README.txt): hold LB to arm, B latches `USER_STOP`, and `CLEAR_FAULT` recovers. This needs the Xbox model 1914 controller.

### Troubleshooting

| What you see | Likely cause |
|---|---|
| Link stays `DOWN` and `link_commands` stays at 0 | TX and RX are not crossed. Swap the two data wires. |
| `frame_errors` keeps climbing on the control board | Missing or loose ground wire. |
| `ERR SERIAL_FRAME_DISCARDED_SEND_NEWLINE` on the control board | Noise or a partial line on the link. Check the wiring, then send a blank line. |
| Control board latched `SERIAL_FRAME_ERROR` | Bad data arrived on the link while armed. Fix the wiring, then send `CLEAR_FAULT`. |

**Safety:** if bad data arrives on either input while the Xbox controller has the board armed, the control board latches a fault. A loose link cable fails safe, but don't unplug the link while armed unless you are testing that.

## Running the tests

```
cd verification
g++ -std=c++17 -fsanitize=address,undefined comms_test.cpp ../MB001_Comms/BridgeLogic.cpp -o comms_test && ./comms_test
g++ -std=c++17 -fsanitize=address,undefined controller_test.cpp ../MB001_Control/Controller.cpp ../MB001_Control/Protocol.cpp -o controller_test && ./controller_test
g++ -std=c++17 -fsanitize=address,undefined gamepad_test.cpp ../MB001_Control/GamepadInput.cpp ../MB001_Control/Controller.cpp -o gamepad_test && ./gamepad_test
g++ -std=c++17 -fsanitize=address,undefined -Istubs ble_adapter_test.cpp ../MB001_Control/XboxBle.cpp ../MB001_Control/GamepadInput.cpp ../MB001_Control/Controller.cpp -o ble_adapter_test && ./ble_adapter_test
```
