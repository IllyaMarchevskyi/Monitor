#pragma once

#include <Arduino.h>
extern uint8_t relay_on[];
extern uint8_t relay_off[];

extern bool relay_turn_off;
extern bool relay_turn_on;

void ensureNetOrRebootPort0();
