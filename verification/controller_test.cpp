#include "../MB001_Control/Controller.h"
#include "../MB001_Control/Protocol.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
bool parse(const char* text, Command& c) {
  char buffer[160]; strcpy(buffer, text); return parseCommand(buffer, c);
}
int main() {
  BenchController c;
  assert(c.state == DISARMED && c.target.throttle == 0 && c.target.brake == 0);
  assert(c.setTargets(1,0,0,0));
  assert(c.arm(100));
  assert(!c.setTargets(50,0,-100,200));
  assert(c.setTargets(50,10,0,300));
  assert(c.target.throttle == 50 && c.target.steering == -100 && c.lastCommandMs == 200);
  assert(c.setTargets(101,0,0,300));
  c.tick(2199); assert(c.state == ARMED);
  c.tick(2200); assert(c.state == FAULT && c.fault == COMMAND_TIMEOUT);
  assert(c.target.throttle == 0 && c.target.brake == 0 && c.target.steering == 0);
  assert(c.setTargets(0,0,0,2201)); assert(!c.arm(2201));
  c.disarm(); assert(c.state == FAULT);
  c.trip(USER_STOP); assert(c.fault == COMMAND_TIMEOUT && c.faultCount == 1);
  c.clearFault(); assert(c.state == DISARMED && c.fault == NO_FAULT);
  assert(c.arm(UINT32_MAX-1000));
  c.tick(998); assert(c.state == ARMED);
  c.tick(999); assert(c.state == FAULT);
  c.clearFault(); assert(c.arm(0)); assert(!c.setTargets(0,0,0,1500));
  c.tick(3499); assert(c.state == ARMED);
  assert(c.setTargets(0,100,100,3500)); assert(c.state == FAULT);
  c.clearFault(); assert(c.arm(4000)); c.trip(USER_STOP);
  assert(c.fault == USER_STOP); c.disarm(); assert(c.state == FAULT);
  Command cmd;
  assert(parse("TELEMETRY 0",cmd) && cmd.kind==CMD_TELEMETRY && cmd.value==0);
  assert(parse("  TELEMETRY\t1  ",cmd) && cmd.kind==CMD_TELEMETRY && cmd.value==1);
  // Obsolete USB movement commands must remain unavailable.
  const char* invalid[]={"", "ARM", "HEARTBEAT", "SET 0 100 -100", "THROTTLE 50",
    "BRAKE 100", "STEER 0", "TELEMETRY", "TELEMETRY 0 1", "TELEMETRY 1x",
    "TELEMETRY 999999999999999999999999", "TELEMETRY 2147483648", "TELEMETRY -2147483649",
    "TELEMETRY nan", "TELEMETRY 1.5", "STOP x", "stop"};
  for (const char* s:invalid) assert(!parse(s,cmd));
  assert(parse("STOP",cmd) && cmd.kind==CMD_STOP);
  assert(parse("DISARM",cmd) && cmd.kind==CMD_DISARM);
  assert(parse("CLEAR_FAULT",cmd) && cmd.kind==CMD_CLEAR_FAULT);
  assert(parse("HELP",cmd) && cmd.kind==CMD_HELP);
  assert(parse("VERSION",cmd) && cmd.kind==CMD_VERSION);
  assert(parse("PING",cmd) && cmd.kind==CMD_PING);
  assert(parse("STATUS",cmd) && cmd.kind==CMD_STATUS);
  assert(parse("HEALTH",cmd) && cmd.kind==CMD_HEALTH);
  puts("PASS: state transitions, latched faults, atomic validation, timeout boundaries, rollover and parser rejection");
}
