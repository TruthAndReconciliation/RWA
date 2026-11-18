#include "../MB001_Control/GamepadInput.h"
#include <assert.h>
#include <stdio.h>

int main() {
  BenchController c;
  GamepadInput p;
  GamepadSample s;
  p.connect(c);
  s.enable = true; p.report(c, s, 0); assert(c.state == DISARMED);
  s.enable = false; p.report(c, s, 1); assert(p.ready);
  s.enable = true; s.throttle = 500; p.report(c, s, 2);
  assert(c.state == DISARMED && !p.ready);
  s.throttle = 0; p.report(c, s, 3); assert(c.state == DISARMED);
  s.enable = false; p.report(c, s, 4);
  s.enable = true; p.report(c, s, 5); assert(c.state == ARMED);
  s.throttle = 1023; s.steering = -511; p.report(c, s, 6);
  assert(c.target.throttle == 100 && c.target.steering == -100);
  s.brake = 31; p.report(c, s, 7); assert(c.target.throttle == 0);
  s.brake = 1023; s.steering = 512; p.report(c, s, 8);
  assert(c.target.brake == 100 && c.target.throttle == 0 && c.target.steering == 100);
  p.tick(c, 507); assert(c.state == ARMED);
  p.report(c, s, 508); assert(c.state == FAULT && c.fault == GAMEPAD_TIMEOUT);
  assert(c.target.throttle == 0 && c.target.brake == 0 && c.target.steering == 0);
  c.clearFault(); p.inhibit(); p.report(c, s, 509); assert(c.state == DISARMED);
  s = GamepadSample(); p.report(c, s, 510);
  s.enable = true; p.report(c, s, 511); assert(c.state == ARMED);
  s.enable = false; p.report(c, s, 512); assert(c.state == DISARMED);
  s.enable = true; p.report(c, s, 513);
  s.stop = true; p.report(c, s, 514); assert(c.fault == USER_STOP);
  s.stop = false; p.report(c, s, 515); assert(c.state == FAULT);
  p.disconnect(c); p.connect(c); assert(c.state == FAULT);
  c.clearFault(); s = GamepadSample(); p.report(c, s, 516);
  s.enable = true; p.report(c, s, 517); p.disconnect(c);
  assert(c.fault == GAMEPAD_DISCONNECTED);
  c.clearFault(); p.connect(c);
  s = GamepadSample(); p.report(c, s, UINT32_MAX-100);
  s.enable = true; p.report(c, s, UINT32_MAX-99);
  p.tick(c, 399); assert(c.state == ARMED);
  p.tick(c, 400); assert(c.fault == GAMEPAD_TIMEOUT);
  c.clearFault(); p.connect(c); s = GamepadSample(); p.report(c, s, 500);
  p.tick(c, 1000); s.enable = true; p.report(c, s, 1001);
  assert(c.state == DISARMED); // stale neutral qualification cannot arm
  s.enable = false; p.report(c, s, 1002);
  s.enable = true; p.report(c, s, 1003); assert(c.state == ARMED);
  c.disarm(); p.inhibit(); p.report(c, s, 1004); assert(c.state == DISARMED);
  puts("PASS: Xbox gating, brake priority, mapping, stop, disconnect, stale reports and rollover");
}
