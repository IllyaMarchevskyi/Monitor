#include "display.h"
#include "config.h"
#include "utils.h"

TFT_eSPI tft;

static const uint8_t *ids = nullptr;
static size_t ids_len = 0;
static bool no_repeate_active_ids[6] = {false, false, false,
                                        false, false, false};

static float prev_send_arr[31] = {0};
static String macToString(const uint8_t mac[6]);
static void drawFooter();
static void readEepromMac(uint8_t out[6]);
static bool macValid(const uint8_t b[6]);
static bool macValid(const uint8_t b[6]);
static void drawWorkIds(bool &alive1, bool &alive2, bool &alive3, bool &alive4);
static void drawData();
static void drawOnlyValue();

void initDisplay() {
  // bool t1 = true;
  // bool t2 = false;
  // bool t3 = false;
  // bool t4 = false;
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  drawData();
  drawFooter();
  // drawWorkIds(t1, t2, t3, t4);
}

void drawValue(bool &alive1, bool &alive2, bool &alive3, bool &alive4) {
  if (!time_guard_allow("draw/update", DRAW_TIME_SLEEP, true))
    return;

  drawOnlyValue();
  if (!ids) {
    drawWorkIds(alive1, alive2, alive3, alive4);
    return;
  }
}

static void drawData() {
  int x;
  int y;
  for (uint16_t i = 0; i < labels_len; i++) {
    x = (i < len_col) ? col1_x : (i < len_col * 2) ? col2_x : col3_x;
    y = (i < len_col)       ? i * y_step
        : (i < len_col * 2) ? (i - len_col) * y_step
                            : (i - len_col * 2) * y_step;
    Serial.print("X: ");
    Serial.print(x);
    Serial.print("Y: ");
    Serial.println(y);
    tft.fillRect(x, y, 150, y_step, TFT_BLACK);
    char label[16];
    sprintf(label, "%s: ", labels[i].name);
    tft.setCursor(x, y);
    tft.print(label);
    x = (i < len_col)       ? col1_x_value
        : (i < len_col * 2) ? col2_x_value
                            : col3_x_value;
    tft.setCursor(x, y);
    if (send_arr[i] == -1) {
      tft.print("#.##");
    } else {
      tft.print(send_arr[i], 2);
    }
    prev_send_arr[i] = send_arr[i];
  }
}

static void drawOnlyValue() {
  int x;
  int y;
  for (uint16_t i = 0; i < labels_len; i++) {
    if (send_arr[i] != prev_send_arr[i]) {
      x = (i < len_col)       ? col1_x_value
          : (i < len_col * 2) ? col2_x_value
                              : col3_x_value;
      y = (i < len_col)       ? i * y_step
          : (i < len_col * 2) ? (i - len_col) * y_step
                              : (i - len_col * 2) * y_step;
      tft.fillRect(x, y, 80, y_step, TFT_BLACK);
      tft.setCursor(x, y);
      if (send_arr[i] == -1) {
        {
          tft.print("#.##");
        }
      } else {
        {
          tft.print(send_arr[i], 2);
        }
      }
      prev_send_arr[i] = send_arr[i];
    }
  }
}

static void drawWorkIds(bool &alive1, bool &alive2, bool &alive3,
                        bool &alive4) {
  const int h = tft.height();
  const int w = tft.width();
  const int bar_h = 20;
  const int bar_w = 60;

  if (alive1) {
    ids = EXTRA_IF_ONLY2;
    ids_len = EXTRA_ONLY2_CNT;
  } else if (alive2) {
    ids = EXTRA_IF_ONLY4;
    ids_len = EXTRA_ONLY4_CNT;
  } else if (alive3) {
    ids = EXTRA_IF_ONLY6;
    ids_len = EXTRA_ONLY6_CNT;
  } else if (alive4) {
    ids = EXTRA_IF_ONLY7;
    ids_len = EXTRA_ONLY7_CNT;
  } else {
    ids = nullptr;
    ids_len = 0;
  }
  tft.fillRect(0, h - bar_h*2, w, bar_h, TFT_BLACK);

  tft.drawString("ID |", 2, h - bar_h * 2);
  if (ids == nullptr && ids_len == 0) {
    tft.drawString("NO ANSWER", 62, h - bar_h * 2);
  }

  for (size_t i = 0; i < ids_len; i++) {
    uint8_t id = ids[i];
    const char *st = active_ids[i] ? "1" : "0";
    tft.drawString(String(id) + ":" + st, 62 + bar_w * i, h - bar_h * 2);
    no_repeate_active_ids[i] = active_ids[i];
  }
}

void drawOnlyValuesIds() {
  if (!ids) return;
  const int h = tft.height();
  const int bar_h = 20;
  const int bar_w = 60;

  for (size_t i = 0; i < ids_len; i++) {
    uint8_t id = ids[i];
    const char *st = active_ids[i] ? "1" : "0";
    if (no_repeate_active_ids[i] == active_ids[i]) {
      continue;
    }

    if (id < 10) {
      tft.drawString(st, 82 + 2 + bar_w * i, h - bar_h * 2);
    } else {
      tft.drawString(st, 82 + 2 + 12 + bar_w * i, h - bar_h * 2);
    }
    no_repeate_active_ids[i] = active_ids[i];
  }
}

// static bool eepromMacInvalid() {
//   bool allFF = true, all00 = true;
//   for (int i = 0; i < 6; ++i) {
//     uint8_t b = EEPROM.read(0 + i);
//     if (b != 0xFF) {
//       {
//         allFF = false;
//       }
//     }
//     if (b != 0x00) {
//       {
//         all00 = false;
//       }
//     }
//   }
//   return (allFF || all00);
// }

static String macToString(const uint8_t mac[6]) {
  auto hx = [](uint8_t v) {
    return (char)((v < 10) ? ('0' + v) : ('A' + (v - 10)));
  };
  String s;
  s.reserve(17);
  for (int i = 0; i < 6; ++i) {
    if (i) {
      s += ':';
    }
    s += hx(mac[i] >> 4);
    s += hx(mac[i] & 0x0F);
  }
  return s;
}

static void drawFooter() {
  const int h = tft.height();
  const int w = tft.width();
  const int bar_h = 20; // enough for text size 2

  // Clear bottom bar
  tft.fillRect(0, h - bar_h, w, bar_h, TFT_BLACK);

  // Left bottom: label + HEX ID (as MAC-like)
  tft.setTextDatum(BL_DATUM);
  String label;
  String value;
  uint8_t mac[6];
  readEepromMac(mac);
  if (!macValid(mac)) {
    // EEPROM has 00.. or FF.. -> show default MAC and label MAC_DEF
    label = "MAC_DEF: ";
    value = macToString(MAC_ADDR);
  } else {
    label = "MAC: ";
    value = macToString(mac);
  }
  tft.drawString(label + value, 2, h - 1);

  // Right bottom: VERSION from Config.h
  tft.setTextDatum(BR_DATUM);
#ifdef VERSION
  tft.drawString(String("v") + VERSION, w - 2, h - 1);
#else
  tft.drawString("v?", w - 2, h - 1);
#endif
  // Reset datum to default for other drawings
  tft.setTextDatum(TL_DATUM);
}

static void readEepromMac(uint8_t out[6]) {
  for (int i = 0; i < 6; ++i)
    out[i] = EEPROM.read(0 + i);
}

static bool macValid(const uint8_t b[6]) {
  bool allFF = true, all00 = true;
  for (int i = 0; i < 6; ++i) {
    if (b[i] != 0xFF) {
      allFF = false;
    }
    if (b[i] != 0x00) {
      all00 = false;
    }
  }
  if (allFF || all00) {
    return false;
  }
  if (b[0] & 0x01) {
    return false;
  }
  return true;
}
