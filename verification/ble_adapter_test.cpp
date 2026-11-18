#include "stubs/Bluepad32.h"
#include "../MB001_USB/XboxBle.h"
#include <assert.h>
#include <stdio.h>
FakeBluepad BP32;
FakeSerial Serial;
uint32_t millis() { return 100; }
int main() {
  BenchController c;
  GamepadInput input;
  beginXbox(c,input);
  Controller pad;
  BP32.connect(&pad); // Device class is not known yet.
  assert(!pad.rejected && !input.connected);
  pollXbox(0); assert(!input.connected);
  pad.gamepad=true; pad.data=true; BP32.updated=true;
  pollXbox(100); assert(input.connected && input.received && input.ready);
  assert(c.state==DISARMED); // First report does not automatically arm.
  Controller second; BP32.connect(&second); assert(second.rejected);
  BP32.disconnect(&pad); assert(!input.connected);
  Controller wrongVendor; wrongVendor.properties.vendor_id=1;
  BP32.connect(&wrongVendor); assert(wrongVendor.rejected);
  Controller mouse; BP32.connect(&mouse); assert(!mouse.rejected);
  mouse.data=true; pollXbox(100); assert(mouse.rejected && !input.connected);
  BP32.disconnect(&mouse);
  puts("PASS: connection before first report, deferred class validation, single-controller and vendor filters");
}
