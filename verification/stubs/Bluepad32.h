#pragma once
#include <stdint.h>
#include <string>
#include <functional>
struct ControllerProperties { uint16_t vendor_id = 0x045e; uint16_t product_id = 0x0b13; };
class Controller {
 public:
  bool connected = true, data = false, gamepad = false, rejected = false;
  ControllerProperties properties;
  ControllerProperties getProperties() { return properties; }
  std::string getModelName() { return "test"; }
  bool isGamepad() { return gamepad; }
  bool isConnected() { return connected; }
  bool hasData() { return data; }
  void disconnect() { rejected = true; connected = false; }
  int throttle() { return 0; } int brake() { return 0; } int axisX() { return 0; }
  bool l1() { return false; } bool b() { return false; }
};
using ControllerPtr = Controller*;
struct FakeBluepad {
  std::function<void(ControllerPtr)> connect, disconnect;
  bool updated = false;
  void setup(std::function<void(ControllerPtr)> a, std::function<void(ControllerPtr)> b) { connect=a; disconnect=b; }
  void enableVirtualDevice(bool) {}
  bool update() { return updated; }
};
struct FakeSerial { template<class... T> void printf(const char*, T...) {} void println(const char*) {} };
extern FakeBluepad BP32;
extern FakeSerial Serial;
uint32_t millis();
