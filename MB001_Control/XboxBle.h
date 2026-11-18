#pragma once
#include "GamepadInput.h"
#include <stddef.h>
void beginXbox(BenchController& controller, GamepadInput& input);
void pollXbox(uint32_t now);
// Fills out with the BLUETOOTH health line (no newline).
void formatXboxHealth(char* out, size_t size);
