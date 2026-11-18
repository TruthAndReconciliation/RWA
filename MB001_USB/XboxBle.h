#pragma once
#include "GamepadInput.h"
void beginXbox(BenchController& controller, GamepadInput& input);
void pollXbox(uint32_t now);
void printXboxHealth();
