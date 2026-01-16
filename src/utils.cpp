#include "utils.h"
#include "config.h"

static void rebuildSendArrayFromLabels();
static void arrSumPeriodicUpdate();
static void sendArrPeriodicUpdate();

static uint16_t tmp_id_value = 0;
static uint16_t acc_count = 0;

static uint32_t acc_sum[CH_COUNT] = {0};
static uint32_t acc_sq_sum[CH_COUNT] = {0};
static uint32_t channel_avg[CH_COUNT] = {0};
static uint32_t channel_std[CH_COUNT] = {0};

static volatile bool g_rs485_busy = false;
static _TimeGuardEntry _tg_entries[8];

bool time_guard_allow(const char *key, uint32_t interval_ms,
                      bool wait_first /*=false*/) {
  uint32_t now = millis();
  int free_i = -1;
  for (int i = 0; i < (int)(sizeof(_tg_entries) / sizeof(_tg_entries[0]));
       ++i) {
    if (_tg_entries[i].key[0] == '\0') {
      if (free_i < 0)
        free_i = i;
      continue;
    }
    if (strcmp(_tg_entries[i].key, key) == 0) {
      if (now - _tg_entries[i].last_ms < interval_ms)
        return false;
      _tg_entries[i].last_ms = now;
      return true;
    }
  }
  // New key
  int use_i = (free_i >= 0) ? free_i : 0; // overwrite slot 0 if full
  strncpy(_tg_entries[use_i].key, key, sizeof(_tg_entries[use_i].key) - 1);
  _tg_entries[use_i].key[sizeof(_tg_entries[use_i].key) - 1] = '\0';
  _tg_entries[use_i].last_ms = now;
  return !wait_first;
}

bool rs485_acquire(uint16_t timeout_ms) {
  uint32_t t0 = millis();
  while (g_rs485_busy) {
    if (millis() - t0 > timeout_ms)
      return false;
    delay(1);
  }
  g_rs485_busy = true;
  return true;
}

void rs485_release() {
  g_rs485_busy = false;
  delayMicroseconds(4000);
}

size_t buildMbTcpRead03(uint8_t *out, uint16_t txId, uint8_t unit,
                        uint16_t addr, uint16_t qty) {
  out[0] = txId >> 8;
  out[1] = txId; // TxID
  out[2] = 0;
  out[3] = 0; // Protocol = 0
  out[4] = 0;
  out[5] = 6;    // Length = 6 (Unit+PDU)
  out[6] = unit; // Unit ID
  out[7] = 0x03; // Function
  out[8] = addr >> 8;
  out[9] = addr; // Start address
  out[10] = qty >> 8;
  out[11] = qty; // Quantity
  return 12;
}

void printHex(const uint8_t *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (b[i] < 0x10)
      Serial.print('0');
    Serial.print(b[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

// ================================ RS-485 DIR ================================
void pre_transmission_main() { digitalWrite(RS485_DIR_PIN, HIGH); }
void post_transmission_main() { digitalWrite(RS485_DIR_PIN, LOW); }

void collectAndAverageEveryMinute() {
  if (!time_guard_allow("sec-tick", 1000))
    return;

  arrSumPeriodicUpdate();
  acc_count++;

  if (acc_count >= SAMPLES_PER_MIN) {
    for (size_t index = 0; index < CH_COUNT; ++index) {
      float sampleCount = (acc_count == 0) ? 1.0f : (float)acc_count;
      float avg = acc_sum[index] / sampleCount;
      channel_avg[index] = avg; // середнє за хвилину

      float variance = 0.0f;
      if (sampleCount > 1.0f) {
        variance = (acc_sq_sum[index] - sampleCount * avg * avg) /
                   (sampleCount - 1.0f);
        if (variance < 0.0f)
          variance = 0.0f;
      }
      channel_std[index] = sqrtf(variance);

      acc_sum[index] = 0;
      acc_sq_sum[index] = 0; // готуємося до наступної хвилини
    }
    acc_count = 0;

    rebuildSendArrayFromLabels();
    Serial.println(F("--- 1-min averages ready ---"));
    sendArrPeriodicUpdate();
  }
  tmp_id_value++;
}

static void rebuildSendArrayFromLabels() {
  size_t used = labels_len;
  if (used > SEND_ARR_SIZE)
    used = SEND_ARR_SIZE;
  for (size_t i = 0; i < used; ++i) {
    float value = DEFAULT_SEND_VAL;
    ChannelIndex channel = labels[i].channel;
    size_t idx = static_cast<size_t>(channel);
    if (idx < CH_COUNT) {
      value = labels[i].useStd ? channel_std[idx] : channel_avg[idx];
    }
    send_arr[i] = value;
  }
  for (size_t i = used; i < SEND_ARR_SIZE; ++i) {
    send_arr[i] = DEFAULT_SEND_VAL;
  }
}

// uint32_t time_guard_remaining(const char *key, uint32_t interval_ms) {
//   uint32_t now = millis();
//   for (int i = 0; i < (int)(sizeof(_tg_entries) / sizeof(_tg_entries[0]));
//        ++i) {
//     if (_tg_entries[i].key[0] == '\0')
//       continue;
//     if (strcmp(_tg_entries[i].key, key) == 0) {
//       uint32_t elapsed = now - _tg_entries[i].last_ms;
//       if (elapsed >= interval_ms)
//         return 0;
//       return interval_ms - elapsed;
//     }
//   }
//   return 0;
// }

static void arrSumPeriodicUpdate() {

  acc_sum[0] += sensors_dec[0]; // CO
  acc_sq_sum[0] += sensors_dec[0] * sensors_dec[0];

  acc_sum[1] += sensors_dec[1]; // SO2
  acc_sq_sum[1] += sensors_dec[1] * sensors_dec[1];

  acc_sum[2] += sensors_dec[2]; // NO2
  acc_sq_sum[2] += sensors_dec[2] * sensors_dec[2];

  acc_sum[3] += sensors_dec[3]; // NO
  acc_sq_sum[3] += sensors_dec[3] * sensors_dec[3];

  acc_sum[4] += sensors_dec[4]; // H2S
  acc_sq_sum[4] += sensors_dec[4] * sensors_dec[4];

  acc_sum[5] += sensors_dec[5]; // O3
  acc_sq_sum[5] += sensors_dec[5] * sensors_dec[5];

  acc_sum[6] += sensors_dec[6]; // NH3
  acc_sq_sum[6] += sensors_dec[6] * sensors_dec[6];

  acc_sum[7] += sensors_dec[7]; // PM_2.5
  acc_sq_sum[7] += sensors_dec[7] * sensors_dec[7];

  acc_sum[8] += sensors_dec[8]; // PM_10
  acc_sq_sum[8] += sensors_dec[8] * sensors_dec[8];

  // Radiation and service (no meteo)
  acc_sum[9] += radiation_uSvh;
  acc_sq_sum[9] += radiation_uSvh * radiation_uSvh;

  acc_sum[10] += service_t[0];
  acc_sq_sum[10] += service_t[0] * service_t[0];

  acc_sum[11] += service_t[1];
  acc_sq_sum[11] += service_t[1] * service_t[1];

  //   for (size_t i = 0; i < CH_COUNT; ++i) {
  //   float value = INIT_SEND_ARR_0_13[i][tmp_id_value%6];
  //   acc_sum[i] += value;  // double -> float ок
  //   acc_sq_sum[i] += value * value;
  // }

  // for(int id=0; id<labels_len; id++)
  // {
  //   Serial.print(labels[id]);
  //   Serial.print(": ");
  //   Serial.print("acc_sum");
  //   Serial.print('[');
  //   Serial.print(id);
  //   Serial.print("]=");
  //   Serial.print(acc_sum[id]);
  //   Serial.println(';');
  // }
}

// ============================== send_arr Maintenance ========================
static void sendArrPeriodicUpdate() {
  for (uint16_t id = 0; id < labels_len; id++) {
    Serial.print(labels[id].name);
    Serial.print(": ");
    Serial.print("send_arr");
    Serial.print('[');
    Serial.print(id);
    Serial.print("]=");
    Serial.print(send_arr[id]);
    Serial.println(';');
  }
}
