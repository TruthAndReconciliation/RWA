#pragma once
#include <stdint.h>
// Board-only logic: no GPIO writes or physical motor control.
const uint32_t COMMAND_TIMEOUT_MS = 2000;
enum ControllerState { DISARMED, ARMED, FAULT };
enum FaultCode { NO_FAULT, USER_STOP, COMMAND_TIMEOUT, SERIAL_FRAME_ERROR, GAMEPAD_DISCONNECTED, GAMEPAD_TIMEOUT };
struct Targets { int throttle = 0; int brake = 0; int steering = 0; };
class BenchController {
 public:
  ControllerState state = DISARMED;
  FaultCode fault = NO_FAULT;
  Targets target;
  uint32_t lastCommandMs = 0;
  uint32_t faultCount = 0;
  bool arm(uint32_t now);
  void disarm();
  void trip(FaultCode reason);
  void clearFault();
  void tick(uint32_t now);
  const char* setTargets(int throttle, int brake, int steering, uint32_t now);
};
const char* stateName(ControllerState state);
const char* faultName(FaultCode fault);
