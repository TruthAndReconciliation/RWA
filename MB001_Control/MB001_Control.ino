/*
  MB001 control board. ESP32-S3-N16R8. USB uploads only.
  Holding LB enables BENCH logic only. No motor outputs or IMU readings.
  Targets are requests, NOT measured/applied motor positions.
  Defaults: throttle 0%, brake 0% (released), steering 0 (-100..100 units).
  STOP/timeout reset stored requests; they are NOT a physical brake or
  emergency stop. The team must design hardware stopping behavior.
  Serial Monitor: 115200 baud, Newline or Both NL & CR.

  Commands arrive from two places with exactly the same rules:
    - USB Serial Monitor (Serial, UART0)
    - comms board link (Serial1, TX GPIO17 / RX GPIO18, 115200 8N1)
  Each reply goes back out the port the command came in on. The link cannot
  do anything USB cannot do, and neither can bypass the Xbox LB gate.
*/
#include "Controller.h"
#include "Protocol.h"
#include "PinAssignments.h"
#include "XboxBle.h"
#include <esp_system.h>

const char FIRMWARE_VERSION[] = "0.5.0-link-bench";
const uint32_t FRAME_TIMEOUT_MS = 250;
const uint32_t TELEMETRY_INTERVAL_MS = 250;
const uint32_t LINK_BAUD = 115200;
BenchController controller;
GamepadInput gamepad;
uint32_t rejectedCommands = 0;
uint32_t frameErrors = 0;

// One place commands can come from (USB or the comms link). Each port keeps
// its own half-received line and its own TELEMETRY on/off setting.
struct CommandPort {
  Stream* stream;                // Serial or Serial1
  char buffer[96] = {};          // current line being received
  size_t length = 0;             // characters in buffer so far
  bool discard = false;          // throwing away a bad line until the next '\n'
  uint32_t lastByteMs = 0;       // for the partial-line timeout
  bool telemetry = false;        // TELEMETRY 1 sent on this port?
  uint32_t lastTelemetryMs = 0;
  uint32_t commands = 0;         // complete lines received (shown in HEALTH)
  explicit CommandPort(Stream* s) : stream(s) {}
};
CommandPort usbPort(&Serial);
CommandPort linkPort(&Serial1);

void printStatus(Print& out) {
  out.printf("STATUS version=%s mode=BENCH state=%s fault=%s uptime_ms=%lu "
             "requested_throttle=%d requested_brake=%d requested_steering=%d "
             "outputs_applied=0 imu_valid=0\n",
             FIRMWARE_VERSION, stateName(controller.state), faultName(controller.fault),
             (unsigned long)millis(), controller.target.throttle,
             controller.target.brake, controller.target.steering);
  out.printf("GAMEPAD connected=%d ready=%d fresh=%d raw_rt=%d raw_lt=%d raw_x=%d lb=%d b=%d\n",
             gamepad.connected, gamepad.ready,
             gamepad.received && uint32_t(millis()-gamepad.lastReportMs) < GamepadInput::TIMEOUT_MS,
             gamepad.raw.throttle, gamepad.raw.brake, gamepad.raw.steering,
             gamepad.raw.enable, gamepad.raw.stop);
}
void printHealth(Print& out) {
  out.printf("HEALTH heap_free=%lu heap_min=%lu psram_total=%lu psram_free=%lu "
             "flash_bytes=%lu reset_reason=%d rejected=%lu frame_errors=%lu faults=%lu\n",
             (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
             (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreePsram(),
             (unsigned long)ESP.getFlashChipSize(), (int)esp_reset_reason(),
             (unsigned long)rejectedCommands, (unsigned long)frameErrors,
             (unsigned long)controller.faultCount);
  out.println("HEALTH imu=NOT_IMPLEMENTED motors=NOT_IMPLEMENTED hardware_outputs=DISABLED");
  char bluetooth[128];
  formatXboxHealth(bluetooth, sizeof(bluetooth));
  out.println(bluetooth);
  out.printf("LINK tx=%d rx=%d baud=%lu usb_commands=%lu link_commands=%lu\n",
             Pins::LINK_TX, Pins::LINK_RX, (unsigned long)LINK_BAUD,
             (unsigned long)usbPort.commands, (unsigned long)linkPort.commands);
  out.printf("PINS reserved_only=1 throttle_step=%d throttle_dir=%d throttle_en=%d "
             "brake_step=%d brake_dir=%d brake_en=%d "
             "steering_step=%d steering_dir=%d steering_en=%d imu_sda=%d imu_scl=%d\n",
             Pins::THROTTLE_STEP, Pins::THROTTLE_DIR, Pins::THROTTLE_ENABLE,
             Pins::BRAKE_STEP, Pins::BRAKE_DIR, Pins::BRAKE_ENABLE,
             Pins::STEERING_STEP, Pins::STEERING_DIR, Pins::STEERING_ENABLE,
             Pins::IMU_SDA, Pins::IMU_SCL);
}
void reject(Print& out, const char* reason) {
  rejectedCommands++;
  out.print("ERR "); out.println(reason);
}
void processCommand(CommandPort& port, char* line) {
  Print& out = *port.stream; // replies go back where the command came from
  Command command;
  if (!parseCommand(line, command)) { reject(out, "SYNTAX"); return; }
  uint32_t now = millis();
  controller.tick(now);
  // USB/link support only diagnostics and stop/recovery; Xbox owns movement.
  const char* error = nullptr;
  switch (command.kind) {
    case CMD_HELP:
      out.println("HELP VERSION PING STATUS HEALTH DISARM STOP CLEAR_FAULT TELEMETRY 0|1");
      out.println("XBOX: RT throttle, LT brake priority, left X steer, hold LB enable, B stop.");
      out.println("Neutral + LB released required before enabling. Motor outputs DISABLED.");
      return;
    case CMD_VERSION: out.println(FIRMWARE_VERSION); return;
    case CMD_PING: out.println("PONG"); return;
    case CMD_STATUS: printStatus(out); return;
    case CMD_HEALTH: printHealth(out); return;
    case CMD_DISARM: controller.disarm(); gamepad.inhibit(); break;
    case CMD_STOP: controller.trip(USER_STOP); gamepad.inhibit(); break;
    case CMD_CLEAR_FAULT:
      if (controller.state != FAULT) error = "NO_LATCHED_FAULT";
      else { controller.clearFault(); gamepad.inhibit(); }
      break;
    case CMD_TELEMETRY:
      if (command.value < 0 || command.value > 1) error = "TELEMETRY_EXPECTS_0_OR_1";
      else port.telemetry = command.value == 1; // only for this port
      break;
    default: error = "UNKNOWN_COMMAND";
  }
  if (error) reject(out, error);
  else out.println("OK");
}
void rejectFrame(CommandPort& port) {
  if (port.discard) return;
  port.discard = true; port.length = 0; frameErrors++;
  // Garbage on either input while armed latches a fault (fail safe).
  if (controller.state == ARMED) controller.trip(SERIAL_FRAME_ERROR);
  reject(*port.stream, "SERIAL_FRAME_DISCARDED_SEND_NEWLINE");
}
void readPort(CommandPort& port) {
  uint32_t now = millis();
  if (port.length > 0 && uint32_t(now - port.lastByteMs) >= FRAME_TIMEOUT_MS) rejectFrame(port);
  // Bound work so a continuous stream cannot starve the controller timeout.
  for (int bytes = 0; bytes < 64 && port.stream->available() > 0; bytes++) {
    int value = port.stream->read(); port.lastByteMs = millis();
    if (value == '\r') continue;
    if (value == '\n') {
      if (!port.discard && port.length > 0) {
        port.buffer[port.length] = '\0';
        port.commands++;
        processCommand(port, port.buffer);
      }
      port.length = 0; port.discard = false;
    } else if (!port.discard) {
      if ((value < 32 && value != '\t') || value > 126 ||
          port.length >= sizeof(port.buffer) - 1) rejectFrame(port);
      else port.buffer[port.length++] = char(value);
    }
  }
}
void sendTelemetry(CommandPort& port, uint32_t now) {
  if (!port.telemetry || uint32_t(now - port.lastTelemetryMs) < TELEMETRY_INTERVAL_MS) return;
  port.lastTelemetryMs = now;
  // Drop periodic reports when congested instead of accumulating a backlog.
  if (port.stream->availableForWrite() >= 512) printStatus(*port.stream);
}
void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  // UART link to the comms board. Buffer sizes must be set before begin().
  Serial1.setRxBufferSize(512);
  Serial1.setTxBufferSize(1024);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, Pins::LINK_RX, Pins::LINK_TX);
  beginXbox(controller, gamepad);
  // No wait for Serial: the controller runs with the monitor closed, too.
}
void loop() {
  controller.tick(millis());
  readPort(usbPort);
  readPort(linkPort);
  uint32_t now = millis();
  pollXbox(now);
  now = millis(); controller.tick(now);
  // No motor or IMU driver is initialized or called in this bench build.
  sendTelemetry(usbPort, now);
  sendTelemetry(linkPort, now);
  delay(1); // Let the Bluetooth/RTOS tasks run.
}
