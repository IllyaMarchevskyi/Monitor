#pragma once
#include <Arduino.h>

void bdbgPeriodicRequest();
void bdbgFeedByte(uint8_t b);
void bdbgTryFinalizeFrame();
