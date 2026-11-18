/*
  MB001 board-only controller. ESP32-S3-N16R8. USB uploads only.
  Holding LB enables BENCH logic only. No motor outputs or IMU readings.
  Targets are requests, NOT measured/applied motor positions.
  Defaults: throttle 0%, brake 0% (released), steering 0 (-100..100 units).
  STOP/timeout reset stored requests; they are NOT a physical brake or
  emergency stop. The team must design hardware stopping behavior.
  Serial Monitor: 115200 baud, Newline or Both NL & CR.
*/
#include "Controller.h"
#include "Protocol.h"
#include "PinAssignments.h"
#include "XboxBle.h"
#include <esp_system.h>

const char FIRMWARE_VERSION[] = "0.4.4-xbox-bench";
const uint32_t FRAME_TIMEOUT_MS = 250;
const uint32_t TELEMETRY_INTERVAL_MS = 250;
BenchController controller;
GamepadInput gamepad;
char inputBuffer[96];
size_t inputLength = 0;
bool discardFrame = false;
bool telemetryEnabled = false;
uint32_t lastByteMs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t rejectedCommands = 0;
uint32_t frameErrors = 0;

void printStatus() {
  Serial.printf("STATUS version=%s mode=BENCH state=%s fault=%s uptime_ms=%lu "
                "requested_throttle=%d requested_brake=%d requested_steering=%d "
                "outputs_applied=0 imu_valid=0\n",
                FIRMWARE_VERSION, stateName(controller.state), faultName(controller.fault),
                (unsigned long)millis(), controller.target.throttle,
                controller.target.brake, controller.target.steering);
  Serial.printf("GAMEPAD connected=%d ready=%d fresh=%d raw_rt=%d raw_lt=%d raw_x=%d lb=%d b=%d\n",
                gamepad.connected, gamepad.ready,
                gamepad.received && uint32_t(millis()-gamepad.lastReportMs) < GamepadInput::TIMEOUT_MS,
                gamepad.raw.throttle, gamepad.raw.brake, gamepad.raw.steering,
                gamepad.raw.enable, gamepad.raw.stop);
}
void printHealth() {
  Serial.printf("HEALTH heap_free=%lu heap_min=%lu psram_total=%lu psram_free=%lu "
                "flash_bytes=%lu reset_reason=%d rejected=%lu frame_errors=%lu faults=%lu\n",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
                (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreePsram(),
                (unsigned long)ESP.getFlashChipSize(), (int)esp_reset_reason(),
                (unsigned long)rejectedCommands, (unsigned long)frameErrors,
                (unsigned long)controller.faultCount);
  Serial.println("HEALTH imu=NOT_IMPLEMENTED motors=NOT_IMPLEMENTED hardware_outputs=DISABLED");
  printXboxHealth();
  Serial.printf("PINS reserved_only=1 throttle_step=%d throttle_dir=%d throttle_en=%d "
                "brake_step=%d brake_dir=%d brake_en=%d "
                "steering_step=%d steering_dir=%d steering_en=%d imu_sda=%d imu_scl=%d\n",
                Pins::THROTTLE_STEP, Pins::THROTTLE_DIR, Pins::THROTTLE_ENABLE,
                Pins::BRAKE_STEP, Pins::BRAKE_DIR, Pins::BRAKE_ENABLE,
                Pins::STEERING_STEP, Pins::STEERING_DIR, Pins::STEERING_ENABLE,
                Pins::IMU_SDA, Pins::IMU_SCL);
}
void reject(const char* reason) {
  rejectedCommands++;
  Serial.print("ERR "); Serial.println(reason);
}
void processCommand(char* line) {
  Command command;
  if (!parseCommand(line, command)) { reject("SYNTAX"); return; }
  uint32_t now = millis();
  controller.tick(now);
  // USB supports only diagnostics and stop/recovery; Xbox owns movement.
  const char* error = nullptr;
  switch (command.kind) {
    case CMD_HELP:
      Serial.println("HELP VERSION PING STATUS HEALTH DISARM STOP CLEAR_FAULT TELEMETRY 0|1");
      Serial.println("XBOX: RT throttle, LT brake priority, left X steer, hold LB enable, B stop.");
      Serial.println("Neutral + LB released required before enabling. Motor outputs DISABLED.");
      return;
    case CMD_VERSION: Serial.println(FIRMWARE_VERSION); return;
    case CMD_PING: Serial.println("PONG"); return;
    case CMD_STATUS: printStatus(); return;
    case CMD_HEALTH: printHealth(); return;
    case CMD_DISARM: controller.disarm(); gamepad.inhibit(); break;
    case CMD_STOP: controller.trip(USER_STOP); gamepad.inhibit(); break;
    case CMD_CLEAR_FAULT:
      if (controller.state != FAULT) error = "NO_LATCHED_FAULT";
      else { controller.clearFault(); gamepad.inhibit(); }
      break;
    case CMD_TELEMETRY:
      if (command.value < 0 || command.value > 1) error = "TELEMETRY_EXPECTS_0_OR_1";
      else telemetryEnabled = command.value == 1;
      break;
    default: error = "UNKNOWN_COMMAND";
  }
  if (error) reject(error);
  else Serial.println("OK");
}
void rejectFrame() {
  if (discardFrame) return;
  discardFrame = true; inputLength = 0; frameErrors++;
  if (controller.state == ARMED) controller.trip(SERIAL_FRAME_ERROR);
  reject("SERIAL_FRAME_DISCARDED_SEND_NEWLINE");
}
void readSerial() {
  uint32_t now = millis();
  if (inputLength > 0 && uint32_t(now - lastByteMs) >= FRAME_TIMEOUT_MS) rejectFrame();
  // Bound work so a continuous stream cannot starve the controller timeout.
  for (int bytes = 0; bytes < 64 && Serial.available() > 0; bytes++) {
    int value = Serial.read(); lastByteMs = millis();
    if (value == '\r') continue;
    if (value == '\n') {
      if (!discardFrame && inputLength > 0) {
        inputBuffer[inputLength] = '\0'; processCommand(inputBuffer);
      }
      inputLength = 0; discardFrame = false;
    } else if (!discardFrame) {
      if ((value < 32 && value != '\t') || value > 126 ||
          inputLength >= sizeof(inputBuffer) - 1) rejectFrame();
      else inputBuffer[inputLength++] = char(value);
    }
  }
}
void setup() {
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);
  beginXbox(controller, gamepad);
  // No wait for Serial: the controller runs with the monitor closed, too.
}
void loop() {
  controller.tick(millis()); readSerial();
  uint32_t now = millis();
  pollXbox(now);
  now = millis(); controller.tick(now);
  // No motor or IMU driver is initialized or called in this bench build.
  if (telemetryEnabled && uint32_t(now - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    // Drop periodic reports when congested instead of accumulating a backlog.
    if (Serial.availableForWrite() >= 512) printStatus();
  }
  delay(1); // Let the Bluetooth/RTOS tasks run.
}
