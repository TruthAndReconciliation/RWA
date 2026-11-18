#include <Bluepad32.h>
#include <bt/uni_bt.h>
#include <bt/uni_bt_le.h>
#include "XboxBle.h"

static ControllerPtr activePad = nullptr;
static bool validatedPad = false;
static BenchController* bench = nullptr;
static GamepadInput* input = nullptr;

static void onConnected(ControllerPtr pad) {
  ControllerProperties properties = pad->getProperties();
  // Only accept one Microsoft gamepad. This is a model filter, not authentication.
  Serial.printf("GAMEPAD candidate model=%s vid=%04x pid=%04x\n",
                pad->getModelName().c_str(), properties.vendor_id, properties.product_id);
  if (activePad || properties.vendor_id != 0x045e) {
    Serial.println("GAMEPAD rejected: occupied slot or non-Microsoft device");
    pad->disconnect();
    return;
  }
  activePad = pad;
  validatedPad = false;
  // Device class lives in input data, which may not exist at connection time.
  // Reserve the slot now, but enable input only after a fresh gamepad report.
  Serial.println("GAMEPAD waiting for first input report");
}
static void onDisconnected(ControllerPtr pad) {
  if (pad != activePad) return;
  input->disconnect(*bench);
  activePad = nullptr;
  validatedPad = false;
  Serial.println("GAMEPAD disconnected");
}
void beginXbox(BenchController& controller, GamepadInput& gamepad) {
  bench = &controller; input = &gamepad;
  BP32.setup(&onConnected, &onDisconnected);
  BP32.enableVirtualDevice(false);
  // Bluepad32 4.1.0 starts scanning before Arduino setup(). The optional
  // configuration service is disabled by default. Avoid redundant queued
  // Bluetooth commands here; this release shares a callback registration.
  // Preserve pairing keys across boots; do not call forgetBluetoothKeys here.
}
void printXboxHealth() {
  Serial.printf("BLUETOOTH scanning=%d ble_enabled=%d candidate=%d validated=%d\n",
                uni_bt_enable_new_connections_is_enabled(), uni_bt_le_is_enabled(),
                activePad != nullptr, validatedPad);
}
void pollXbox(uint32_t now) {
  input->tick(*bench, now);
  bool updated = BP32.update();
  if (!activePad) return;
  if (!activePad->isConnected()) { onDisconnected(activePad); return; }
  if (!updated || !activePad->hasData()) return;
  if (!activePad->isGamepad()) {
    Serial.println("GAMEPAD rejected: input report is not a gamepad");
    input->disconnect(*bench);
    activePad->disconnect();
    return;
  }
  if (!validatedPad) {
    input->connect(*bench);
    validatedPad = true;
    Serial.println("GAMEPAD connected: first gamepad report received");
  }
  GamepadSample sample;
  sample.throttle = activePad->throttle();
  sample.brake = activePad->brake();
  sample.steering = activePad->axisX();
  sample.enable = activePad->l1();
  sample.stop = activePad->b();
  input->report(*bench, sample, millis());
}
