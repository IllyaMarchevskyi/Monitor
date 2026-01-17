#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>

extern const uint16_t len_prev_send_arr;

void initDisplay();
void drawOnlyValuesIds();
void drawValue(bool &alive1, bool &alive2, bool &alive3, bool &alive4);