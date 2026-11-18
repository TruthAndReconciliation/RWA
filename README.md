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

## Running the tests

```
cd verification
g++ -std=c++17 -fsanitize=address,undefined comms_test.cpp ../MB001_Comms/BridgeLogic.cpp -o comms_test && ./comms_test
g++ -std=c++17 -fsanitize=address,undefined controller_test.cpp ../MB001_Control/Controller.cpp ../MB001_Control/Protocol.cpp -o controller_test && ./controller_test
g++ -std=c++17 -fsanitize=address,undefined gamepad_test.cpp ../MB001_Control/GamepadInput.cpp ../MB001_Control/Controller.cpp -o gamepad_test && ./gamepad_test
g++ -std=c++17 -fsanitize=address,undefined -Istubs ble_adapter_test.cpp ../MB001_Control/XboxBle.cpp ../MB001_Control/GamepadInput.cpp ../MB001_Control/Controller.cpp -o ble_adapter_test && ./ble_adapter_test
```
