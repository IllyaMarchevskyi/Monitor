#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>

extern const uint16_t len_prev_send_arr;

void drawData();
void drawOnlyValue();
void initDisplay();