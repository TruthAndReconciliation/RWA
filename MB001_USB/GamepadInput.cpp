#include "GamepadInput.h"

static int limit(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}
static int triggerPercent(int value) {
  value = limit(value, 0, 1023);
  // Three-percent dead zone, then continuous scaling to full travel.
  return value <= 30 ? 0 : (value - 30) * 100 / (1023 - 30);
}
static int steeringPercent(int value) {
  value = limit(value, -511, 512);
  if (value >= -50 && value <= 50) return 0;
  if (value < 0) return (value + 50) * 100 / (511 - 50);
  return (value - 50) * 100 / (512 - 50);
}
void GamepadInput::connect(BenchController& controller) {
  controller.disarm();
  connected = true; received = false; ready = false;
  raw = GamepadSample();
}
void GamepadInput::disconnect(BenchController& controller) {
  if (controller.state == ARMED) controller.trip(GAMEPAD_DISCONNECTED);
  else controller.disarm();
  connected = false; received = false; ready = false;
  raw = GamepadSample();
}
void GamepadInput::tick(BenchController& controller, uint32_t now) {
  if (received && uint32_t(now - lastReportMs) >= TIMEOUT_MS) {
    if (controller.state == ARMED) controller.trip(GAMEPAD_TIMEOUT);
    ready = false;
  }
  if (controller.state == FAULT) ready = false;
}
void GamepadInput::report(BenchController& controller, const GamepadSample& sample,
                          uint32_t now) {
  if (!connected) return;
  tick(controller, now); // A late report must not hide an expired deadline.
  received = true; lastReportMs = now; raw = sample;
  if (sample.stop) { controller.trip(USER_STOP); ready = false; return; }
  if (controller.state == FAULT) { ready = false; return; }
  bool neutral = sample.throttle >= 0 && sample.throttle <= 30 &&
                 sample.brake >= 0 && sample.brake <= 30 &&
                 sample.steering >= -50 && sample.steering <= 50;
  if (!sample.enable) {
    controller.disarm();
    ready = neutral;
    return;
  }
  if (controller.state != ARMED) {
    bool mayArm = ready && neutral;
    ready = false; // A failed attempt requires releasing LB again.
    if (!mayArm || !controller.arm(now)) return;
  }
  int brake = triggerPercent(sample.brake);
  // Brake priority starts above the raw dead zone, even if rounding yields 0%.
  int throttle = sample.brake > 30 ? 0 : triggerPercent(sample.throttle);
  controller.setTargets(throttle, brake, steeringPercent(sample.steering), now);
}
