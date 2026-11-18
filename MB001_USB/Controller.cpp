#include "Controller.h"
bool BenchController::arm(uint32_t now) {
  if (state != DISARMED || fault != NO_FAULT) return false;
  target = Targets(); lastCommandMs = now; state = ARMED;
  return true;
}
void BenchController::disarm() {
  target = Targets();
  if (state != FAULT) state = DISARMED; // Cannot bypass a latched fault.
}
void BenchController::trip(FaultCode reason) {
  target = Targets();
  if (state != FAULT) { fault = reason; faultCount++; }
  state = FAULT; // Preserve the first cause until explicitly cleared.
}
void BenchController::clearFault() {
  target = Targets(); fault = NO_FAULT; state = DISARMED;
}
void BenchController::tick(uint32_t now) {
  // Unsigned subtraction handles millis() rollover.
  if (state == ARMED && uint32_t(now - lastCommandMs) >= COMMAND_TIMEOUT_MS)
    trip(COMMAND_TIMEOUT);
}
const char* BenchController::setTargets(int throttle, int brake, int steering, uint32_t now) {
  tick(now);
  if (state != ARMED) return "NOT_ARMED";
  if (throttle < 0 || throttle > 100 || brake < 0 || brake > 100 ||
      steering < -100 || steering > 100) return "OUT_OF_RANGE";
  if (throttle > 0 && brake > 0) return "THROTTLE_BRAKE_CONFLICT";
  // Validate the entire request before modifying any field.
  target.throttle = throttle; target.brake = brake; target.steering = steering;
  lastCommandMs = now;
  return nullptr;
}
const char* stateName(ControllerState state) {
  switch (state) {
    case DISARMED: return "DISARMED";
    case ARMED: return "ARMED";
    default: return "FAULT";
  }
}
const char* faultName(FaultCode fault) {
  switch (fault) {
    case NO_FAULT: return "NONE";
    case USER_STOP: return "USER_STOP";
    case COMMAND_TIMEOUT: return "COMMAND_TIMEOUT";
    case GAMEPAD_DISCONNECTED: return "GAMEPAD_DISCONNECTED";
    case GAMEPAD_TIMEOUT: return "GAMEPAD_TIMEOUT";
    default: return "SERIAL_FRAME_ERROR";
  }
}
