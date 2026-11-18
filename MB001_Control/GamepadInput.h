#pragma once
#include "Controller.h"

// Bluepad32 normalized ranges. Values outside these ranges are clamped.
struct GamepadSample {
  int throttle = 0; // RT: 0..1023
  int brake = 0;    // LT: 0..1023
  int steering = 0; // Left stick X: -511..512
  bool enable = false; // LB, held to drive
  bool stop = false;   // B, latched until USB CLEAR_FAULT
};

class GamepadInput {
 public:
  static const uint32_t TIMEOUT_MS = 500;
  bool connected = false;
  bool received = false;
  bool ready = false;
  uint32_t lastReportMs = 0;
  GamepadSample raw;
  void connect(BenchController& controller);
  void disconnect(BenchController& controller);
  void tick(BenchController& controller, uint32_t now);
  void report(BenchController& controller, const GamepadSample& sample, uint32_t now);
  void inhibit() { ready = false; }
};
