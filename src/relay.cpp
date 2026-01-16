#include "relay.h"
#include "config.h"
#include "utils.h"

uint8_t relay_on[] = {0x0B, 0x05, 0x00, 0x00, 0xFF, 0x00};
uint8_t relay_off[] = {0x0B, 0x05, 0x00, 0x00, 0x00, 0x00};

bool relay_turn_off = false;
bool relay_turn_on = false;

static uint16_t crc16_modbus(const uint8_t *p, size_t n) {
  Serial.println("crc16_modbus");
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= *p++;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
  }
  return crc;
}

static void relayTimedPulse(uint8_t unitId, uint8_t channel) {
  static uint32_t start_time = 0;
  uint16_t crc;
  relay_on[0] = (uint8_t)(unitId & 0xFF);
  relay_on[3] = (uint8_t)(channel & 0xFF);
  relay_off[0] = (uint8_t)(unitId & 0xFF);
  relay_off[3] = (uint8_t)(channel & 0xFF);

  if (relay_turn_off) {
    Serial.println("Relay Stop id 0");
    if (!rs485_acquire(300)) {
      Serial.println("RS485 busy, skip relay pulse");
      return;
    }

    pre_transmission_main();
    crc = crc16_modbus(relay_on, sizeof(relay_on));
    Serial3.write(relay_on, sizeof(relay_on));
    Serial3.write(crc & 0xFF);
    Serial3.write((crc >> 8) & 0xFF);
    Serial3.flush();
    rs485_release();
    start_time = millis();
    relay_turn_off = false;
  }
  // delay(RELAY_PULSE_MS);
  if (time_guard_allow("print_time_wait_start_id_0", 1000)) {
    Serial.print(millis() - start_time);
    Serial.print(" >= ");
    Serial.println(RELAY_PULSE_MS);
    Serial.print("Time: ");
    Serial.println(millis() - start_time >= RELAY_PULSE_MS);
    Serial.print("Reley On: ");
    Serial.println(relay_turn_on);
  }
  if (millis() - start_time >= RELAY_PULSE_MS && relay_turn_on) {
    Serial.println("Relay Start id 0");

    if (!rs485_acquire(300)) {
      Serial.println("RS485 busy, skip relay pulse");
      return;
    }

    pre_transmission_main();
    crc = crc16_modbus(relay_off, sizeof(relay_off));
    Serial3.write(relay_off, sizeof(relay_off));
    Serial3.write(crc & 0xFF);
    Serial3.write((crc >> 8) & 0xFF);
    Serial3.flush();
    rs485_release();
    start_time = millis();
    relay_turn_on = false;
  }
}

static bool isInternetAlive(const IPAddress &testIp, uint16_t port,
                            uint16_t timeoutMs = 600) {
  Ethernet.maintain();
  EthernetClient c;
  uint32_t t0 = millis();
  bool ok = c.connect(testIp, port);
  uint32_t dt = millis() - t0;
  if (ok) {
    Serial.println("Іnternet Сonnection Successful!!");
    c.stop();
  }
  Serial.print("TCP check: ");
  Serial.print(ok);
  Serial.print(" in ");
  Serial.print(dt);
  Serial.println(" ms");
  return ok;
}

void ensureNetOrRebootPort0() {
  relayTimedPulse(UNIT_ID, CH);
  if (!time_guard_allow("relay", RELAY_SLEEP, true))
    return;
  if (isInternetAlive(NET_CHECK_IP, NET_CHECK_PORT))
    return;

  // relayTimedPulse(UNIT_ID, CH);
  relay_turn_off = true;
  relay_turn_on = true;
}
