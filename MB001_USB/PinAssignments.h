#pragma once
#include <stdint.h>

// GPIO numbers match the board's printed numeric labels, NOT header positions.
// Provisional wiring for three STEP/DIR stepper drivers with separate enables.
// These constants reserve signals only. No pinMode/digitalWrite/pulses are used
// until the driver model, 3.3 V logic interface and enable polarity are confirmed.
namespace Pins {
  const uint8_t THROTTLE_STEP = 4;
  const uint8_t THROTTLE_DIR = 5;
  const uint8_t THROTTLE_ENABLE = 6;

  const uint8_t BRAKE_STEP = 7;
  const uint8_t BRAKE_DIR = 10;
  const uint8_t BRAKE_ENABLE = 11;

  const uint8_t STEERING_STEP = 12;
  const uint8_t STEERING_DIR = 13;
  const uint8_t STEERING_ENABLE = 14;

  // Reserved for Noah's BNO055 integration. I2C is not initialized yet.
  const uint8_t IMU_SDA = 8;
  const uint8_t IMU_SCL = 9;
}

// Leave 0/3/45/46 (boot straps), 19/20 (USB), 43/44 (USB-UART),
// 26..37 (module memory, including octal PSRAM), and board LED pins alone.
// No clutch output. Limit switches/homing/driver fault inputs are not assigned.
// Driver EN polarity and DIR-to-mechanical-direction mapping are intentionally
// unspecified: disabling a stepper can release holding torque on the brake.
