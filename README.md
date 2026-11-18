# RWA — MB001 ESP32-S3 Controller

ESP32-S3-N16R8 Arduino firmware for the MB001 motorcycle project.

Current source version: `0.4.4-xbox-bench`.

- `MB001_USB/`: Arduino sketch, controller logic, Xbox BLE input, USB diagnostics, GPIO reservations, and partition table.
- `verification/`: host-side C++ regression tests and Bluetooth test stubs.

Open `MB001_USB/MB001_USB.ino` in Arduino IDE. See [setup and operating instructions](MB001_USB/README.txt) for the Bluepad32 board package and configuration.

This is a board-only bench build: motor outputs and IMU readings are not implemented. The planned second ESP32-S3 and inter-board I2C integration are not included in this version.
