#include "sensor_box.h"
#include "config.h"
#include "eth_manager.h"
#include "utils.h"
#include "serial.h"

ModbusMaster sensor_box;

static void read_TEMP_RH(float *mass);
static void pollAllSensorBoxes(bool &alive1, bool &alive2, bool &alive3,
                               bool &alive4);
static inline float floatFromWords(uint16_t high_word, uint16_t low_word);
static bool pingId(uint8_t id);
static bool readHalfFloats(uint8_t id, float out[3], uint16_t startAddr,
                           uint16_t regCount);
static bool readFloats(uint8_t id, float out[3], uint16_t startAddr,
                       uint16_t regCount);
static bool readPM(uint8_t id, uint16_t startAddr, float &f0, float &f1);

static uint16_t modbusRtuCrc16(const uint8_t *data, size_t len);
static size_t buildMbRtuRead03(uint8_t *out, uint8_t unit, uint16_t addr,
                               uint16_t qty);
static float decodeFloat32(const uint8_t *p, bool wordSwap = true,
                           bool byteSwap = false);
static bool sendRtuOverTcpRead03(float *mass, const IPAddress &ip,
                                 uint16_t port, uint8_t unit, uint16_t addr,
                                 uint16_t qty, uint16_t timeoutMs = 20);
static bool sendHexTCP(float *mass, const IPAddress &ip, uint16_t port,
                       const uint8_t *data, size_t len,
                       uint16_t timeoutMs = 20);

void poll_SensorBox_SensorZTS3008(bool &alive1, bool &alive2, bool &alive3,
                                  bool &alive4) {
  // if (!time_guard_allow("sensorbox", SernsorBoxTimeSleep, true))
  //   return;
  read_TEMP_RH(service_t);
  pollAllSensorBoxes(alive1, alive2, alive3, alive4);
}

static void read_TEMP_RH(float *mass) {
  if (!rs485_acquire(500))
    return;
  sensor_box.begin(11, Serial3);
  uint8_t res = sensor_box.readHoldingRegisters(0x0000, 2);
  if (res != sensor_box.ku8MBSuccess) {
    logLine("FAILED GET SERVICE_T DATA", true);
    rs485_release();
    return;
  }
  uint16_t rh_raw = sensor_box.getResponseBuffer(0);
  uint16_t t_raw_u = sensor_box.getResponseBuffer(1);
  int16_t t_raw_s = (int16_t)t_raw_u;
  mass[0] = t_raw_s / 10.0f;
  mass[1] = rh_raw / 10.0f;
  logLine("TEMP: ", false);
  logLine(t_raw_s, true);
  logLine(service_t[0], true);
  logLine("RH: ", false);
  logLine(rh_raw, true);
  logLine(service_t[1], true);

  rs485_release();
}

static void pollAllSensorBoxes(bool &alive1, bool &alive2, bool &alive3,
                               bool &alive4) {
  // 1) Ping primary IDs
  bool enable_alives = alive1 || alive2 || alive3 || alive4;
  for (uint8_t i = 0; i < PRIMARY_COUNT; ++i) {

    if (!enable_alives) {
      uint8_t id = PRIMARY_IDS[i];
      bool ok = (id == PRIMARY_IDS[1]) ? pingId_Ethernet(ip_4, port, time_sleep)
                                       : pingId(id);
      if (id == PRIMARY_IDS[0])
        alive1 = ok;
      else if (id == PRIMARY_IDS[1])
        alive2 = ok;
      else if (id == PRIMARY_IDS[2])
        alive3 = ok;
      else if (id == PRIMARY_IDS[3])
        alive4 = ok;
    }
  }

  logLine("ID", false);
  logLine(PRIMARY_IDS[0], false);
  logLine(": ", false);
  logLine(alive1, false);
  logLine(" | ", false);
  logLine("ID", false);
  logLine(PRIMARY_IDS[1], false);
  logLine(": ", false);
  logLine(alive2, false);
  logLine(" | ", false);
  logLine("ID", false);
  logLine(PRIMARY_IDS[2], false);
  logLine(": ", false);
  logLine(alive3, false);
  logLine(" | ", false);
  logLine("ID", false);
  logLine(PRIMARY_IDS[3], false);
  logLine(": ", false);
  logLine(alive4, true);

  // 2) Build list of additional IDs to poll
  uint8_t toPoll[7] = {0};
  uint8_t nPoll = 0;
  if (alive1)
    for (uint8_t i = 0; i < EXTRA_ONLY2_CNT; ++i)
      toPoll[nPoll++] = EXTRA_IF_ONLY2[i];
  if (alive2)
    for (uint8_t i = 0; i < EXTRA_ONLY4_CNT; ++i)
      toPoll[nPoll++] = EXTRA_IF_ONLY4[i];
  if (alive3)
    for (uint8_t i = 0; i < EXTRA_ONLY6_CNT; ++i)
      toPoll[nPoll++] = EXTRA_IF_ONLY6[i];
  if (alive4)
    for (uint8_t i = 0; i < EXTRA_ONLY7_CNT; ++i)
      toPoll[nPoll++] = EXTRA_IF_ONLY7[i];

  // 3) Poll and map results into sensors_dec
  for (uint8_t i = 0; i < nPoll; ++i) {
    uint8_t id = toPoll[i];
    float v[8] = {0};

    switch (id) {
    case 2:
      if (!readHalfFloats(id, v, GAS_START_ADDR2, GAS_REG_COUNT2)) {
        logLine("id: " + String(id) + " | Not Found CO, SO2, NO2", true);
        active_ids[i] = false;
        continue;
      }
      logLine("ID: ", false);
      logLine(id, true);
      logLine("CO ", false);
      logLine(v[1], true);
      logLine("SO2 ", false);
      logLine(v[3], true);
      logLine("NO2 ", false);
      logLine(v[5], true);
      sensors_dec[0] = v[1] / co_divider;      // CO
      sensors_dec[1] = v[3] / so2_no2_divider; // SO2
      sensors_dec[2] = v[5] / so2_no2_divider; // NO2
      // sensors_dec[4] = v[3]; // H2S
      break;
    case 3:
      if (!sendRtuOverTcpRead03(v, ip_3, port, /*id*/ id, /*addr*/ 0x0032,
                                /*qty*/ 4, time_sleep)) {
        logLine("id: " + String(id) + " | Not Found SO2, H2S", true);
        active_ids[i] = false;
        continue;
      }
      logLine("ID: ", false);
      logLine(id, true);
      logLine("SO2 ", false);
      logLine(v[0], true);
      logLine("H2S ", false);
      logLine(v[1], true);
      sensors_dec[1] = v[0]; // SO2
      sensors_dec[4] = v[1]; // H2S
      break;
    case 4:
      if (!sendRtuOverTcpRead03(v, ip_4, port, /*id*/ id, /*addr*/ 0x0032,
                                /*qty*/ 2, time_sleep)) {
        logLine("id: " + String(id) + " | Not Found CO", true);
        active_ids[i] = false;
        continue;
      }
      logLine("CO ", false);
      logLine(v[0], true);
      sensors_dec[0] = v[0]; // CO
      break;
    case 5:
      if (!readFloats(id, v, GAS_START_ADDR, GAS_REG_COUNT)) {
        logLine("id: " + String(id) + " | Not Found CO, SO2, NO2", true);
        active_ids[i] = false;
        continue;
      }
      logLine("ID: ", false);
      logLine(id, true);
      logLine("CO ", false);
      logLine(v[0], true);
      logLine("SO2 ", false);
      logLine(v[1], true);
      logLine("NO2 ", false);
      logLine(v[2], true);
      sensors_dec[0] = v[0]; // CO
      sensors_dec[1] = v[1]; // SO2
      sensors_dec[2] = v[2]; // NO2
      break;
    case 6:
      if (!readFloats(id, v, GAS_START_ADDR, GAS_REG_COUNT)) {
        logLine("id: " + String(id) + " | Not Found NO, H2S, O3", true);
        active_ids[i] = false;
        continue;
      }
      logLine("ID: ", false);
      logLine(id, true);
      logLine("NO ", false);
      logLine(v[0], true);
      logLine("H2S ", false);
      logLine(v[1], true);
      logLine("O3 ", false);
      logLine(v[2], true);
      sensors_dec[3] = v[0]; // NO
      sensors_dec[4] = v[1]; // H2S
      sensors_dec[5] = v[2]; // O3
      break;
    case 7:
      if (!readFloats(id, v, GAS_START_ADDR, GAS_REG_COUNT)) {
        logLine("id: " + String(id) + " | Not Found NH3, H2S, O3", true);
        active_ids[i] = false;
        continue;
      }
      logLine("ID: ", false);
      logLine(id, true);
      logLine("NH3 ", false);
      logLine(v[0], true);
      logLine("H2S ", false);
      logLine(v[1], true);
      logLine("O3 ", false);
      logLine(v[2], true);
      sensors_dec[6] = v[0]; // NH3
      sensors_dec[4] = v[1]; // H2S
      sensors_dec[5] = v[2]; // O3
      break;
    case 8: {
      float NO = 0, NO2 = 0, NH3 = 0;
      if (!sendRtuOverTcpRead03(v, ip_8, port, /*id*/ id, /*addr*/ 0x0032,
                                /*qty*/ 2, time_sleep)) {
        logLine("id: " + String(id) + " | Not Found NO, NO2, NH3", true);
        active_ids[i] = false;
        continue;
      }
      NO = v[0];
      sendRtuOverTcpRead03(v, ip_8, port, /*id*/ id, /*addr*/ 0x0034,
                           /*qty*/ 2, time_sleep);
      NO2 = v[0];
      sendRtuOverTcpRead03(v, ip_8, port, /*id*/ id, /*addr*/ 0x00b8,
                           /*qty*/ 2, time_sleep);
      NH3 = v[0];
      logLine("ID: ", false);
      logLine(id, true);
      logLine("NO ", false);
      logLine(NO, true);
      logLine("NO2 ", false);
      logLine(NO2, true);
      logLine("NH3 ", false);
      logLine(NH3, true);
      sensors_dec[3] = NO;   // NO
      sensors_dec[2] = NO2;  // NO2
      sensors_dec[6] = NH3; // NH3
      break;
    }
    case 9: {
      float pm25 = 0, pm10 = 0;
      if (!sendRtuOverTcpRead03(v, ip_9, port, id, 0x000, 2, time_sleep)) {
        logLine("id: " + String(id) + " | Not Found PM25, PM10", true);
        active_ids[i] = false;
        continue;
      }
      logLine("PM25 ", false);
      logLine(pm25 / pm_divider, true);
      logLine("PM10 ", false);
      logLine(pm10 / pm_divider, true);
      sensors_dec[7] = pm25 / pm_divider;
      sensors_dec[8] = pm10 / pm_divider;
      break;
    }
    case 10: {
      float pm25 = 0, pm10 = 0;
      if (!readPM(PM_ID, PM_START_ADDR, pm25, pm10)) {
        logLine("id: " + String(id) + " | Not Found PM25, PM10", true);
        active_ids[i] = false;
        continue;
      }
      logLine("PM25 ", false);
      logLine(pm25 / pm_divider, true);
      logLine("PM10 ", false);
      logLine(pm10 / pm_divider, true);
      sensors_dec[7] = pm25 / pm_divider;
      sensors_dec[8] = pm10 / pm_divider;
      break;
    }
    }
    active_ids[i] = true;
  }

  // float pm25 = 0, pm10 = 0;
  // if (readPM(PM_ID, PM_START_ADDR, pm25, pm10)) {

  //   Serial.print("PM25 ");
  //   Serial.println(pm25 / pm_divider);
  //   Serial.print("PM10 ");
  //   Serial.println(pm10 / pm_divider);
  //   sensors_dec[7] = pm25 / pm_divider;
  //   sensors_dec[8] = pm10 / pm_divider;
  // } else {
  //   Serial.println("Not Found PM25, PM10");
  // }
}

static inline float floatFromWords(uint16_t high_word, uint16_t low_word) {
  uint32_t raw =
      ((uint32_t)low_word << 16) | high_word; // device-specific word order
  float f;
  memcpy(&f, &raw, sizeof(float));
  return f;
}

static bool pingId(uint8_t id) {
  if (id == 4)
    return true; // handled via Ethernet ping externally
  if (!rs485_acquire(500))
    return false;
  sensor_box.begin(id, Serial3);
  uint8_t res = sensor_box.readHoldingRegisters(GAS_START_ADDR, 2);
  logLine("ID answer -> ", false);
  logLine(id, true);
  logLine("pingId res=0x", false);
  logLine(res, HEX, true);
  bool ok = (res == sensor_box.ku8MBSuccess);
  rs485_release();
  return ok;
}

static bool readHalfFloats(uint8_t id, float out[3], uint16_t startAddr,
                           uint16_t regCount) {
  if (!rs485_acquire(500))
    return false;
  sensor_box.begin(id, Serial3);
  uint8_t res = sensor_box.readHoldingRegisters(startAddr, regCount);
  if (res != sensor_box.ku8MBSuccess) {
    rs485_release();
    return false;
  }
  for (uint8_t i = 0; i < regCount; ++i) {
    out[i] = sensor_box.getResponseBuffer(i);
  }
  rs485_release();
  return true;
}

static bool readFloats(uint8_t id, float out[3], uint16_t startAddr,
                       uint16_t regCount) {
  if (!rs485_acquire(500))
    return false;
  sensor_box.begin(id, Serial3);
  uint8_t res = sensor_box.readHoldingRegisters(startAddr, regCount);
  if (res != sensor_box.ku8MBSuccess) {
    rs485_release();
    return false;
  }
  for (uint8_t i = 0; i < regCount / 2; ++i) {
    uint16_t hi = sensor_box.getResponseBuffer(i * 2 + 0);
    uint16_t lo = sensor_box.getResponseBuffer(i * 2 + 1);
    out[i] = floatFromWords(hi, lo);
  }
  rs485_release();
  return true;
}

static bool readPM(uint8_t id, uint16_t startAddr, float &f0, float &f1) {
  if (!rs485_acquire(500))
    return false;
  sensor_box.begin(id, Serial3);
  uint8_t res = sensor_box.readHoldingRegisters(startAddr, 2);
  if (res != sensor_box.ku8MBSuccess) {
    rs485_release();
    return false;
  }
  uint16_t hi0 = sensor_box.getResponseBuffer(0);
  uint16_t hi1 = sensor_box.getResponseBuffer(1);
  f0 = hi0;
  f1 = hi1;
  rs485_release();
  return true;
}

static uint16_t modbusRtuCrc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; ++i) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
  }
  return crc;
}

static size_t buildMbRtuRead03(uint8_t *out, uint8_t unit, uint16_t addr,
                               uint16_t qty) {
  out[0] = unit;
  out[1] = 0x03;
  out[2] = addr >> 8;
  out[3] = addr;
  out[4] = qty >> 8;
  out[5] = qty;

  uint16_t crc = modbusRtuCrc16(out, 6);
  out[6] = crc & 0xFF;
  out[7] = crc >> 8;
  return 8;
}

static float decodeFloat32(const uint8_t *p, bool wordSwap = true,
                           bool byteSwap) {
  uint8_t b[4] = {p[0], p[1], p[2], p[3]};
  if (wordSwap) {
    uint8_t t0 = b[0], t1 = b[1];
    b[0] = b[2];
    b[1] = b[3];
    b[2] = t0;
    b[3] = t1;
  }
  if (byteSwap) {
    uint8_t t = b[0];
    b[0] = b[1];
    b[1] = t;
    t = b[2];
    b[2] = b[3];
    b[3] = t;
  }
  uint32_t u = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
               ((uint32_t)b[2] << 8) | b[3];
  float f;
  memcpy(&f, &u, 4);
  return f;
}

static bool sendRtuOverTcpRead03(float *mass, const IPAddress &ip,
                                 uint16_t port, uint8_t unit, uint16_t addr,
                                 uint16_t qty, uint16_t timeoutMs) {
  uint8_t req[8] = {0};
  size_t len = buildMbRtuRead03(req, unit, addr, qty);

  EthernetClient client;
  logLine(F("Connect RTU/TCP "), false);
  logLine(ip, false);
  logLine(F(":"), false);
  logLine(port, true);
  if (!client.connect(ip, port)) {
    logLine(F("RTU/TCP connect failed"), true);
    client.stop();
    return false;
  }

  size_t sent = client.write(req, len);
  client.flush();
  logLine(F("Sent RTU "), false);
  logLine(sent, false);
  logLine(F(" bytes:"), true);
  printHex(req, len);
  if (sent != len) {
    client.stop();
    return false;
  }

  size_t got = 0;
  size_t expected = 0;
  uint32_t t0 = millis();
  extern uint8_t resp[256];
  while (millis() - t0 < timeoutMs) {
    while (client.available() && got < 256) {
      resp[got++] = client.read();
      if (got >= 3 && resp[1] == 0x03) {
        expected = (size_t)resp[2] + 5;
      } else if (got >= 2 && resp[1] == (0x03 | 0x80)) {
        expected = 5;
      }
    }
    if (expected > 0 && got >= expected)
      break;
    if (!client.connected() && client.available() == 0)
      break;
  }
  client.stop();

  logLine(F("Recv RTU "), false);
  logLine(got, false);
  logLine(F(" bytes:"), true);
  if (got)
    printHex(resp, got);

  if (got < 5 || resp[0] != unit)
    return false;
  if (resp[1] == (0x03 | 0x80)) {
    logLine(F("RTU exception code=0x"), false);
    logLine(resp[2], HEX, true);
    return false;
  }
  if (resp[1] != 0x03)
    return false;

  uint8_t byteCount = resp[2];
  expected = (size_t)byteCount + 5;
  if (got < expected || byteCount != qty * 2)
    return false;

  uint16_t gotCrc =
      (uint16_t)resp[expected - 2] | ((uint16_t)resp[expected - 1] << 8);
  uint16_t calcCrc = modbusRtuCrc16(resp, expected - 2);
  if (gotCrc != calcCrc) {
    logLine(F("RTU CRC mismatch"), true);
    return false;
  }

  uint8_t floatCount = byteCount / 4;
  for (uint8_t i = 0; i < floatCount; ++i) {
    mass[i] = decodeFloat32(resp + 3 + i * 4, true, false);
  }

  logLine(F("----------------------"), true);
  return true;
}

static bool sendHexTCP(float *mass, const IPAddress &ip, uint16_t port,
                       const uint8_t *data, size_t len, uint16_t timeoutMs) {
  EthernetClient client;
  if (!pingId_Ethernet(ip, port, timeoutMs))
    return false;
  client.connect(ip, port);

  size_t sent = client.write(data, len);
  client.flush();
  logLine(F("Sent "), false);
  logLine(sent, false);
  logLine(F(" bytes:"), true);
  printHex(data, len);

  size_t got = 0;
  uint32_t t0 = millis();
  extern uint8_t resp[256];
  while (millis() - t0 < timeoutMs) {
    while (client.available() && got < 256) {
      resp[got++] = client.read();
    }
    if (!client.connected() && client.available() == 0)
      break;
  }
  client.stop();
  const uint8_t *dataStart = resp + 9;
  for (int i = 0; i < resp[8] / 4; i++)
    mass[i] = decodeFloat32(dataStart + i * 4, true, false);

  logLine(F("Recv "), false);
  logLine(got, false);
  logLine(F(" bytes:"), true);
  if (got)
    printHex(resp, got);
  logLine(F("----------------------"), true);

  return true;
}
