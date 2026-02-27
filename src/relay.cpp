#include "relay.h"
#include "config.h"
#include "utils.h"
#include "serial.h"
#include <stdio.h>
#include <string.h>

uint8_t relay_on[] = {0x0B, 0x05, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00};
uint8_t relay_off[] = {0x0B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

bool relay_turn_off = false;
bool relay_turn_on = false;
static EthernetServer relay_http_server(RELAY_HTTP_PORT);
static EthernetClient relay_http_client;

static bool relayWriteChannel(uint8_t unitId, uint8_t channel, bool on);
static bool relayReadStatusByte(uint8_t unitId, uint8_t &status);
static bool readHttpRequestLine(EthernetClient &client, char *line, size_t maxLen,
                                uint16_t timeoutMs = 700);
static void drainHttpHeaders(EthernetClient &client, uint16_t timeoutMs = 200);
static void sendJson(EthernetClient &client, int statusCode,
                     const char *statusText, const char *body);
static void handleRelayHttpPath(EthernetClient &client, const char *path);

static uint16_t crc16_modbus(const uint8_t *p, size_t n) {
  logLine("crc16_modbus", true);
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= *p++;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
  }
  return crc;
}

bool sendModbus(uint8_t *req, int reqLen, uint8_t *resp, int respLen, int timeout = 500) {

  while (Serial3.available()) Serial3.read();

  Serial3.write(req, reqLen);
  Serial3.flush();
  post_transmission_main();

  unsigned long start = millis();
  int index = 0;

  while (millis() - start < timeout) {
    if (Serial3.available()) {
      uint8_t b = Serial3.read();
      if (resp != nullptr && respLen > 0) {
        resp[index++] = b;
        if (index >= respLen) return true;
      } else {
        (void)b;
      }
    }
  }
  return false;
}

static void relayTimedPulse(uint8_t unitId, uint8_t channel) {
  static uint32_t start_time = 0;
  uint16_t crc;
  relay_on[0] = (uint8_t)(unitId & 0xFF);
  relay_on[3] = (uint8_t)(channel & 0xFF);
  relay_off[0] = (uint8_t)(unitId & 0xFF);
  relay_off[3] = (uint8_t)(channel & 0xFF);
  uint8_t len_relay_on = sizeof(relay_on);
  uint8_t len_relay_off = sizeof(relay_off);

  if (relay_turn_off) {
    logLine("Relay Stop id 0", true);
    if (!rs485_acquire(300)) {
      logLine("RS485 busy, skip relay pulse", true);
      return;
    }

    pre_transmission_main();
    crc = crc16_modbus(relay_on, len_relay_on - 2);
    relay_on[len_relay_on - 2] = crc & 0xFF;
    relay_on[len_relay_on - 1] = crc >> 8;

    sendModbus(relay_on, len_relay_on, nullptr, 0);

    rs485_release();
    start_time = millis();
    relay_turn_off = false;
  }
  // delay(RELAY_PULSE_MS);
  if (time_guard_allow("print_time_wait_start_id_0", 1000)) {
    logLine(millis() - start_time, false);
    logLine(" >= ", false);
    logLine(RELAY_PULSE_MS, true);
    logLine("Time: ", false);
    logLine(millis() - start_time >= RELAY_PULSE_MS, true);
    logLine("Reley On: ", false);
    logLine(relay_turn_on, true);
  }

  if (millis() - start_time >= RELAY_PULSE_MS && relay_turn_on) {
    logLine("Relay Start id 0", true);

    if (!rs485_acquire(300)) {
      logLine("RS485 busy, skip relay pulse", true);
      return;
    }

    pre_transmission_main();
    crc = crc16_modbus(relay_off, len_relay_off - 2);
    relay_off[len_relay_off - 2] = crc & 0xFF;
    relay_off[len_relay_off - 1] = crc >> 8;

    sendModbus(relay_off, len_relay_off, nullptr, 0);

    rs485_release();
    start_time = millis();
    relay_turn_on = false;
  }
}

static void getrelayStatus() {
  uint8_t status = 0;
  if (relayReadStatusByte(UNIT_ID, status)) {
    logLine("Relay status byte: 0x", false);
    logLine(status, HEX, true);

    for (uint8_t i = 0; i < 8; ++i) {
      logLine("CH", false);
      logLine(i, false);
      logLine(": ", false);
      logLine(bitRead(status, i), true);
    }
    return;
  }
  logLine("Failed to get relay status", true);
}

static bool isInternetAlive(const IPAddress &testIp, uint16_t port,
                            uint16_t timeoutMs = 600) {
  Ethernet.maintain();
  EthernetClient c;
  uint32_t t0 = millis();
  bool ok = c.connect(testIp, port);
  uint32_t dt = millis() - t0;
  if (ok) {
    logLine("Іnternet Сonnection Successful!!", true);
    c.stop();
  }
  logLine("TCP check: ", false);
  logLine(ok, false);
  logLine(" in ", false);
  logLine(dt, false);
  logLine(" ms", true);
  return ok;
}

void ensureNetOrRebootPort0() {
  relayTimedPulse(UNIT_ID, CH);
  if (!time_guard_allow("relay", RELAY_TIME_SLEEP, true))
    return;
  if (isInternetAlive(GETWAY, NET_CHECK_PORT))
    return;

  // relayTimedPulse(UNIT_ID, CH);
  relay_turn_off = true;
  relay_turn_on = true;
  getrelayStatus();
}

void initRelayHttp() {
  relay_http_server.begin();
  logLine("Relay HTTP server on port ", false);
  logLine(RELAY_HTTP_PORT, true);
}

void relayHttpServiceOnce() {
  if (relay_http_client && !relay_http_client.connected()) {
    relay_http_client.stop();
  }

  if (!relay_http_client || !relay_http_client.connected()) {
    relay_http_client = relay_http_server.accept();
  }

  if (!relay_http_client || !relay_http_client.connected())
    return;

  if (!relay_http_client.available())
    return;

  char requestLine[128];
  if (!readHttpRequestLine(relay_http_client, requestLine, sizeof(requestLine))) {
    sendJson(relay_http_client, 408, "Request Timeout",
             "{\"ok\":false,\"error\":\"request timeout\"}");
    relay_http_client.stop();
    return;
  }

  drainHttpHeaders(relay_http_client);

  if (strncmp(requestLine, "GET ", 4) != 0) {
    sendJson(relay_http_client, 405, "Method Not Allowed",
             "{\"ok\":false,\"error\":\"only GET is supported\"}");
    relay_http_client.stop();
    return;
  }

  char *pathStart = requestLine + 4;
  char *pathEnd = strchr(pathStart, ' ');
  if (pathEnd == nullptr) {
    sendJson(relay_http_client, 400, "Bad Request",
             "{\"ok\":false,\"error\":\"invalid request line\"}");
    relay_http_client.stop();
    return;
  }
  *pathEnd = '\0';

  handleRelayHttpPath(relay_http_client, pathStart);
  relay_http_client.stop();
}

static bool relayWriteChannel(uint8_t unitId, uint8_t channel, bool on) {
  if (channel > 7) {
    return false;
  }

  if (!rs485_acquire(300)) {
    logLine("RS485 busy, skip relay write", true);
    return false;
  }

  uint8_t request[8];
  request[0] = unitId;
  request[1] = 0x05;
  request[2] = 0x00;
  request[3] = channel;
  request[4] = on ? 0xFF : 0x00;
  request[5] = 0x00;

  uint16_t crc = crc16_modbus(request, 6);
  request[6] = crc & 0xFF;
  request[7] = crc >> 8;

  uint8_t response[8];
  pre_transmission_main();
  bool ok = sendModbus(request, sizeof(request), response, sizeof(response));

  if (!ok) {
    rs485_release();
    logLine("Relay write failed: no response", true);
    return false;
  }

  uint16_t respCrc = (uint16_t)response[6] | ((uint16_t)response[7] << 8);
  uint16_t calcCrc = crc16_modbus(response, 6);
  bool valid = (respCrc == calcCrc) && (memcmp(response, request, 6) == 0);

  rs485_release();

  if (!valid) {
    logLine("Relay write failed: invalid echo", true);
    return false;
  }

  return true;
}

static bool relayReadStatusByte(uint8_t unitId, uint8_t &status) {
  if (!rs485_acquire(300)) {
    logLine("RS485 busy, skip relay status read", true);
    return false;
  }

  uint8_t request[8];
  request[0] = unitId;
  request[1] = 0x01;
  request[2] = 0x00;
  request[3] = 0x00;
  request[4] = 0x00;
  request[5] = 0x08;

  uint16_t crc = crc16_modbus(request, 6);
  request[6] = crc & 0xFF;
  request[7] = crc >> 8;

  uint8_t response[6];
  pre_transmission_main();
  bool ok = sendModbus(request, sizeof(request), response, sizeof(response));

  if (!ok) {
    rs485_release();
    return false;
  }

  uint16_t respCrc = (uint16_t)response[4] | ((uint16_t)response[5] << 8);
  uint16_t calcCrc = crc16_modbus(response, 4);
  bool valid = (response[0] == unitId) && (response[1] == 0x01) &&
               (response[2] == 0x01) && (respCrc == calcCrc);

  if (valid) {
    status = response[3];
  }

  rs485_release();
  return valid;
}

static bool readHttpRequestLine(EthernetClient &client, char *line, size_t maxLen,
                                uint16_t timeoutMs) {
  size_t idx = 0;
  uint32_t t0 = millis();

  while (millis() - t0 < timeoutMs) {
    while (client.available()) {
      char ch = (char)client.read();
      if (ch == '\r') {
        continue;
      }
      if (ch == '\n') {
        line[idx] = '\0';
        return idx > 0;
      }
      if (idx + 1 < maxLen) {
        line[idx++] = ch;
      }
    }
  }

  line[0] = '\0';
  return false;
}

static void drainHttpHeaders(EthernetClient &client, uint16_t timeoutMs) {
  char prev = 0;
  char curr = 0;
  uint32_t t0 = millis();

  while (millis() - t0 < timeoutMs) {
    while (client.available()) {
      prev = curr;
      curr = (char)client.read();
      if (prev == '\n' && curr == '\n') {
        return;
      }
    }
  }
}

static void sendJson(EthernetClient &client, int statusCode,
                     const char *statusText, const char *body) {
  client.print("HTTP/1.1 ");
  client.print(statusCode);
  client.print(' ');
  client.print(statusText);
  client.print("\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Connection: close\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n");
  client.print("Content-Length: ");
  client.print(strlen(body));
  client.print("\r\n\r\n");
  client.print(body);
}

static void handleRelayHttpPath(EthernetClient &client, const char *path) {
  if (strcmp(path, "/relay/status") == 0) {
    uint8_t status = 0;
    if (!relayReadStatusByte(UNIT_ID, status)) {
      sendJson(client, 503, "Service Unavailable",
               "{\"ok\":false,\"error\":\"relay status read failed\"}");
      return;
    }

    char body[96];
    snprintf(body, sizeof(body),
             "{\"ok\":true,\"unit_id\":%u,\"status\":[%u,%u,%u,%u]}",
             UNIT_ID, (status >> 0) & 0x01, (status >> 1) & 0x01,
             (status >> 2) & 0x01, (status >> 3) & 0x01);
    sendJson(client, 200, "OK", body);
    return;
  }

  uint8_t relayId = 0;
  char action[8] = {0};
  int parsed = sscanf(path, "/relay/%hhu/%7s", &relayId, action);
  if (parsed == 2) {
    if (relayId < 1 || relayId > 4) {
      sendJson(client, 400, "Bad Request",
               "{\"ok\":false,\"error\":\"relay id must be 1..4\"}");
      return;
    }

    bool turnOn = (strcmp(action, "on") == 0);
    bool turnOff = (strcmp(action, "off") == 0);
    if (!turnOn && !turnOff) {
      sendJson(client, 400, "Bad Request",
               "{\"ok\":false,\"error\":\"action must be on or off\"}");
      return;
    }

    uint8_t relayChannel = relayId - 1; // external 1..4 -> internal 0..3
    if (!relayWriteChannel(UNIT_ID, relayChannel, turnOn)) {
      sendJson(client, 503, "Service Unavailable",
               "{\"ok\":false,\"error\":\"relay write failed\"}");
      return;
    }

    char body[96];
    snprintf(body, sizeof(body),
             "{\"ok\":true,\"unit_id\":%u,\"relay\":%u,\"state\":\"%s\"}",
             UNIT_ID, relayId, turnOn ? "on" : "off");
    sendJson(client, 200, "OK", body);
    return;
  }

  sendJson(client, 404, "Not Found",
           "{\"ok\":false,\"error\":\"use /relay/{1..4}/on, "
           "/relay/{1..4}/off, /relay/status\"}");
}
