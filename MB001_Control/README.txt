MB001 CONTROL BOARD (future motor + IMU board) — Xbox BLE bench build 0.4.4-xbox-bench

USB firmware uploads only. No Arduino Cloud, Wi-Fi, OTA, onboard self-test commands,
or filesystem. Motor outputs remain hard-disabled; BNO055 remains unavailable.

CONTROLLER MAPPING
Intended controller: Xbox Series X|S model 1914, paired directly using BLE.
Physical pairing/input verification is pending; the available model 1708 did
not connect. A model 1914 is being sourced before testing resumes.
RT = throttle 0..100%; LT = brake 0..100%, with brake priority over throttle.
Left stick X = steering -100..100 normalized units, not physical degrees.
LB = hold to enable; release to reset all requested targets and disarm.
B = latched USER_STOP; reset stored targets. USB CLEAR_FAULT is required to recover.
Trigger dead zone: raw 0..30 of 1023. Steering dead zone: raw -50..50.
Outside dead zones, inputs scale to full travel. Start at 0/0/0.

PAIR AND CHECK (BOARD ONLY)
1. Keep the ESP32 powered by USB. Open Serial Monitor: 115200 baud, Newline.
2. Power on the Xbox controller; hold its top pairing button until the Xbox
   light flashes rapidly. Pair directly with the board, not through macOS.
3. Look for GAMEPAD connected. STATUS shows raw triggers/stick/buttons and
   connected/ready/fresh flags. TELEMETRY 1 enables reports up to four per second.
4. Release LB, both triggers and the stick. Then hold LB while inputs remain
   neutral. state should change to ARMED. Move RT/LT/stick and check requests.
5. Verify LT suppresses RT, releasing LB disarms, and B latches USER_STOP.
6. Release all controls; send CLEAR_FAULT; release LB neutral before re-enabling.
7. Verify controller power-off while armed clears requests and latches either
   GAMEPAD_DISCONNECTED or GAMEPAD_TIMEOUT. Reconnecting never clears a fault.
Controller firmware compatibility and actual pairing/input timing must be checked
on the user's unit. Bluepad32 documents Xbox firmware 5.15+ for BLE.

FRESHNESS AND OWNERSHIP
Only newly received Bluepad32 input reports renew the 500 ms input watchdog.
No reports for 500 ms while armed latches GAMEPAD_TIMEOUT. Late reports cannot
revive it. A 2-second controller watchdog remains as a secondary logic check.
A neutral report with LB released qualifies the next neutral LB press to arm;
qualification expires with stale data. Holding LB on connection cannot arm.
If the controller sends only changed inputs, an unmoving held input may time out;
verify this physically before extending this prototype. Do not renew freshness
from cached stick/button values merely to avoid a timeout.
Xbox owns movement in this build. USB ARM, HEARTBEAT, THROTTLE, BRAKE, STEER and
SET are unsupported and return ERR SYNTAX. USB cannot bypass LB gating.
USB DISARM requires LB release before re-enabling; USB STOP latches a fault.
A single Microsoft gamepad is accepted; other controllers are disconnected.
VID filtering is not authentication. Pairing keys persist across resets.
Discovery remains enabled for this bench version; production needs controller
identity binding and a deliberate pairing window before hardware is enabled.

USB DIAGNOSTICS
HELP, VERSION, PING, STATUS, HEALTH, DISARM, STOP, CLEAR_FAULT, TELEMETRY 0|1.
STATUS reports requested values, not measured/applied motor positions.
outputs_applied=0 and imu_valid=0 remain explicit. HEALTH reports memory,
flash size, reset reason and error counters. HEALTH also prints BLUETOOTH
scanning/ble_enabled flags plus candidate/validated state. These flags do not
prove reception over the air. No fabricated IMU measurements.
Faults remain latched until CLEAR_FAULT. The first cause is retained.
Serial lines are limited to 95 bytes and work to 64 bytes per loop; malformed
frames while armed latch SERIAL_FRAME_ERROR. Partial frames expire after 250 ms.
Send a newline to resynchronize, then CLEAR_FAULT. Responses can block; this
is not a hard real-time physical control implementation.

ARDUINO IDE SETUP
Install esp32_bluepad32 by Ricardo Quesada, version 4.1.0 (includes Bluepad32).
Additional Boards Manager URLs:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
Select ESP32S3 Dev Module from esp32_bluepad32, not the ordinary esp32 package.
Flash: 16MB. PSRAM: OPI. Flash Mode: QIO 80MHz. Upload speed: 115200.
USB CDC On Boot: Disabled for the USB-to-serial connector used here.
USB Mode: Hardware CDC and JTAG. Upload Mode: UART0 / Hardware CDC.
Partition Scheme menu: No OTA (2MB APP/2MB SPIFFS).
IMPORTANT: the included partitions.csv overrides that menu layout during build.
Actual layout: 20 KB NVS, 4 KB PHY, one factory app from 0x10000 to 0xFFFFFF.
There is no SPIFFS, OTA slot or core-dump partition in the actual uploaded table.
The menu's 2 MB compiler limit is conservative; current sketch fits well below it.
Keep partitions.csv and all .h/.cpp files alongside MB001_Control.ino.
Erase All Flash Before Sketch Upload: Disabled to preserve controller pairing.

FILES
Controller.h/.cpp: BenchController state, targets, latched faults and watchdog.
GamepadInput.h/.cpp: pure C++ input mapping, dead zones and enabling rules.
XboxBle.h/.cpp: Bluepad32 adapter, callbacks and new-report polling.
Protocol.h/.cpp: USB parsing. MB001_Control.ino: scheduling and diagnostics.
PinAssignments.h: provisional STEP/DIR/ENABLE and IMU GPIO reservations.
partitions.csv: USB-only layout. No motor/IMU driver placeholders remain.
verification/: host-side state/mapping tests; no test commands on the device.

VALIDATION
Version 0.4.4 passed all three host test programs with AddressSanitizer and
UndefinedBehaviorSanitizer, and compiled for ESP32-S3 using Bluepad32 4.1.0.
Build: 565,209 bytes program storage and 66,820 bytes static RAM (276 and
24 bytes less, respectively, than 0.4.3).
Host C++ regression checks cover state transitions, fault latching, ranges,
parser rejection, throttle/brake priority, stick endpoints, neutral/LB gate,
disconnects, stale/late reports, USB disarm inhibition and timer rollover.
Adapter checks cover a connection before its first report, non-gamepad
rejection, vendor filtering and occupied slots. These use simulated inputs;
physical pairing and hardware behavior remain unverified.
The last USB-verified board firmware is 0.4.2: DISARMED, no fault, outputs disabled.
Source 0.4.4 is a cleanup release and has not been uploaded.

STOP AND HARDWARE INTEGRATION
STOP/timeout reset software requests to 0/0/0. Zero brake means released, NOT
physical stopping. No behavior here is a physical emergency stop. Before adding
outputs, the team must specify a physical stop policy and independent stop path.
Confirm driver models, wiring/logic voltage, enable polarity and motor direction;
then travel limits, steering homing/center, step counts per percent, acceleration
limits and feedback. Do not map percentages directly to GPIO duty cycles.
Hardware integration will require nonblocking drivers, a hardware-ready
interlock, and actual applied/feedback reporting. This build has no GPIO
initialization, step generation or motor driver calls.
For BNO055 integration: GPIO8 SDA/GPIO9 SCL are reserved; confirm breakout,
address, supply, axis mapping and mounting; add actual reads, calibration checks and stale-data faults.
Add polling when the IMU driver is implemented; no placeholder polling remains.
Decide whether valid IMU data is required to arm physical hardware.


References:
https://bluepad32.readthedocs.io/en/latest/plat_arduino/
https://bluepad32.readthedocs.io/en/latest/supported_gamepads/

GPIO RESERVATIONS
Assumption: each of the three stepper motor drivers accepts STEP, DIR and ENABLE.
These are ESP32 GPIO numbers (printed board labels), not header position numbers.
                 STEP    DIR    ENABLE
Throttle         GPIO4   GPIO5  GPIO6
Brake            GPIO7   GPIO10 GPIO11
Steering         GPIO12  GPIO13 GPIO14
BNO055 I2C: SDA GPIO8, SCL GPIO9 reserved, not initialized.
Comms board UART link: TX GPIO17, RX GPIO18 reserved, not initialized (see MB001_Comms/README.txt).
No clutch, limit-switch, homing or driver-fault pins assigned yet.

STEP pulses request increments; DIR selects direction; ENABLE controls the
individual driver. Pulse width, frequency, direction polarity, enable polarity,
acceleration and mechanical travel limits remain unconfigured pending hardware.
These signals go to motor DRIVER inputs, never to motor windings. ESP32 logic
is 3.3 V; Nathan must verify each driver's input voltage/current/interface before
wiring. Optoisolated/5 V inputs may require an interface, not direct GPIO wiring.
No assumptions are made about ground/terminal connections on an unknown driver.
Hardware must hold the drivers in the required inactive state during ESP32 boot
and reset; merely defining ENABLE pins does not establish that state. Brake
holding torque/physical stop behavior needs a separate hardware decision.

The assignments avoid USB GPIO19/20, UART GPIO43/44, strap GPIO0/3/45/46,
and module flash/PSRAM GPIO26..37. GPIO8/9 remain available for the IMU.
GPIO10..13 are also conventional SPI signal choices: do not later initialize
SPI with its default pins without remapping it away from these motor signals.
Clone board wiring must be checked against its silkscreen/schematic by the team.

HEALTH prints PINS reserved_only=1 with all assignments. Defining pins does
not initialize GPIO or generate any outputs.

CLEANUP IN 0.4.4
Removed obsolete USB movement parsing and the unused heartbeat method.
Removed empty hardware functions, unused IMU data fields and the no-op poll timer.
Kept diagnostics, watchdogs, stop/recovery, Xbox input rules and GPIO reservations.
No Cloud, OTA, Wi-Fi, motor outputs or onboard test commands were added.

Pin reference:
https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html
