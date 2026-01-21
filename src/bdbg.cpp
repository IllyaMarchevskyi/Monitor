#include "bdbg.h"
#include "config.h"
#include "utils.h"
#include "serial.h"

// Внутрішній стан модуля (не видно іншим файлам)
static uint8_t bdbg_raw[10] = {0};
static uint8_t bdbg_buf[20] = {0};
static uint8_t bdbg_idx = 0;
static uint32_t bdbg_last_req = 0;
static uint32_t bdbg_last_byte = 0;
static uint32_t bdbg_first_deadline = 0;
static bool bdbg_waiting = false;
static bool bdbg_has_data = false;

static void bdbgPeriodicRequest();
static void bdbgFeedByte(uint8_t b);
static void bdbgTryFinalizeFrame();
static void bdbg_print_hex(const uint8_t *p, size_t n);

void pollRadiation() {
  bdbgPeriodicRequest();
  while (Serial2.available()) {
    bdbgFeedByte(Serial2.read());
  }
  bdbgTryFinalizeFrame();
}

static void bdbgPeriodicRequest() {
  if (millis() - bdbg_last_req >= BDBG_TIME_SLEEP) {
    const uint8_t cmd[] = {0x55, 0xAA, 0x01};
    logLine("Start BDBG-09", true);

    bdbg_last_req = millis();
    bdbg_idx = 0;
    bdbg_has_data = false;
    bdbg_waiting = true;

    while (Serial2.available())
      (void)Serial2.read();

    digitalWrite(BDBG_DIR_PIN, HIGH);
    delayMicroseconds(300);

    Serial2.write(cmd, sizeof(cmd));
    Serial2.flush();

    delayMicroseconds(1200);
    digitalWrite(BDBG_DIR_PIN, LOW);

    logLine("[TX] ", false);
    bdbg_print_hex(cmd, sizeof(cmd));

    bdbg_first_deadline = millis() + BDBG_FIRST_BYTE_TIMEOUT_MS;
    bdbg_last_byte = 0;
  }
}

static void bdbgFeedByte(uint8_t b) {
  if (bdbg_idx < sizeof(bdbg_buf)) {
    bdbg_buf[bdbg_idx++] = b;
  }
  bdbg_has_data = true;
  bdbg_last_byte = millis();
}

static void bdbgTryFinalizeFrame() {
  if (!bdbg_waiting)
    return;

  uint32_t now = millis();
  if (bdbg_idx == 0 && now >= bdbg_first_deadline) {
    logLine("[BDBG] RX timeout (no first byte)", true);
    bdbg_waiting = false;
    return;
  }

  if (bdbg_idx > 0 && (now - bdbg_last_byte) >= BDBG_INTERBYTE_TIMEOUT_MS) {
    logLine("[RX] ", false);
    logLine(bdbg_idx, true);
    logLine(" B", true);
    bdbg_print_hex(bdbg_buf, bdbg_idx);

    if (bdbg_idx == 10) {
      for (uint8_t i = 0; i < 10; i++)
        bdbg_raw[i] = bdbg_buf[i];
      uint32_t raw = ((uint32_t)bdbg_raw[6] << 24) |
                     ((uint32_t)bdbg_raw[5] << 16) |
                     ((uint32_t)bdbg_raw[4] << 8) | ((uint32_t)bdbg_raw[3]);
      radiation_uSvh = raw / 100.0f;
    } else {
      logLine("BDBG: bad length=", false);
      logLine(bdbg_idx, true);
    }

    bdbg_idx = 0;
    bdbg_has_data = false;
    bdbg_waiting = false;
    logLine("Finish BDBG-09", true);
  }
}

static void bdbg_print_hex(const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (p[i] < 0x10)
      logLine('0', false);
    logLine(p[i], HEX, false);
    logLine(' ', false);
  }
  logLine();
}
