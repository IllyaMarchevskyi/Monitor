#include "eth_manager.h"
#include "config.h"
#include "utils.h"

EthernetServer server(MODBUS_TCP_PORT);

static inline bool mac_valid(const uint8_t *m);
static String macToStringLocal(const uint8_t mac[6]);
static bool loadMacFromEeprom(uint8_t mac[6]);
static size_t buildSensorsJson(char *out, size_t maxLen);
static bool httpPostBody(EthernetClient &client, const char *hostHeader,
                         const char *path, const char *body, size_t bodyLen,
                         uint16_t timeoutMs = 2000);
static bool httpPostSensorsImpl(EthernetClient &client, const char *hostHeader,
                                const char *path, uint16_t timeoutMs = 2000);
static const char *hwName(uint8_t hs);
static const char *linkName(uint8_t ls);
static const char *sockName(uint8_t s);
static void printEthDiag(EthernetClient &c);

void initEthernet() {
  Ethernet.init(ETH_CS);
  uint8_t mac[6];
  Serial.println("Initialization Ethernet");
  if (!loadMacFromEeprom(mac)) {
    // Fallback to default MAC from Config.h when EEPROM contains 00.. or FF..
    Serial.println("Get Mac from EEPROM");
    memcpy(mac, MAC_ADDR, 6);
  }
  if (Ethernet.begin(mac, 1000) == 0) {
    Serial.println("Get Mac Def");
    Serial.println("DHCP failed. Using static IP.");
    Ethernet.begin(mac, STATIC_IP, GETWAY);
  }
  server.begin();
  Serial.print("Modbus server on ");
  Serial.println(Ethernet.localIP());
  Serial.println("0=NoHardware, 1=W5100, 2=W5200, 3=W5500");
  Serial.print("HW=");
  Serial.println((int)Ethernet.hardwareStatus());
}

// Public: POST to hostname
bool httpPostSensors(const char *host, uint16_t port, const char *path) {
  if (!time_guard_allow(path, SEND_DATA_TO_SERVER))
    return false;

  EthernetClient client;
  if (!client.connect(host, port)) {
    Serial.println(F("HTTP POST connect(host) failed"));
    return false;
  }
  return httpPostSensorsImpl(client, host, path);
}

// Public: POST to IP address
bool httpPostSensors(const IPAddress &ip, uint16_t port, const char *path) {
  if (!time_guard_allow(path, SEND_DATA_TO_SERVER))
    return false;

  EthernetClient client;
  if (!client.connect(ip, port)) {
    Serial.println(F("HTTP POST connect(IP) failed"));
    return false;
  }
  // Build Host header from IP (HTTP/1.1 requires Host)
  char hostBuf[24];
  snprintf(hostBuf, sizeof(hostBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return httpPostSensorsImpl(client, hostBuf, path);
}

bool pingId_Ethernet(const IPAddress &ip, uint16_t port,
                     uint16_t timeoutMs = 800) {
  EthernetClient client;
  Serial.print(F("Connect "));
  Serial.print(ip);
  Serial.print(F(":"));
  Serial.println(port);
#if defined(ETHERNET_H)
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println(F("ERROR: No Ethernet hardware"));
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println(F("ERROR: LinkOFF"));
  }
#endif
  uint32_t t0 = millis();
  bool ok = client.connect(ip, port);
  uint32_t dt = millis() - t0;
  if (!ok) {
    Serial.print(F("Connect FAILED in "));
    Serial.print(dt);
    Serial.println(F(" ms"));
    printEthDiag(client);
    client.stop();
    return false;
  }
  Serial.print(F("Connect OK in "));
  Serial.print(dt);
  Serial.println(F(" ms"));
  printEthDiag(client);
  client.stop();
  return true;
}

static inline bool mac_valid(const uint8_t *m) {
  bool allFF = true, all00 = true;
  for (int i = 0; i < 6; ++i) {
    if (m[i] != 0xFF)
      allFF = false;
    if (m[i] != 0x00)
      all00 = false;
  }
  if (allFF || all00)
    return false;
  if (m[0] & 0x01)
    return false; // must be unicast
  return true;
}

static String macToStringLocal(const uint8_t mac[6]) {
  auto hx = [](uint8_t v) {
    return (char)((v < 10) ? ('0' + v) : ('A' + (v - 10)));
  };
  String s;
  s.reserve(17);
  for (int i = 0; i < 6; ++i) {
    if (i)
      s += ':';
    s += hx(mac[i] >> 4);
    s += hx(mac[i] & 0x0F);
  }
  return s;
}

static bool loadMacFromEeprom(uint8_t mac[6]) {
  // Try to read 6-byte MAC from UNIQUE_ID_ADDR=0
  bool allFF = true, all00 = true;
  for (int i = 0; i < 6; ++i) {
    mac[i] = EEPROM.read(0 + i);
    if (mac[i] != 0xFF)
      allFF = false;
    if (mac[i] != 0x00)
      all00 = false;
  }
  auto valid = [](const uint8_t *m) {
    if (m[0] & 0x01)
      return false; // must be unicast
    return true;
  };
  if (!allFF && !all00 && valid(mac))
    return true;

  // Legacy 4-byte ID fallback at the same address range
  uint8_t id4[4];
  bool id4_ff = true;
  for (int i = 0; i < 4; ++i) {
    id4[i] = EEPROM.read(0 + i);
    if (id4[i] != 0xFF)
      id4_ff = false;
  }
  if (!id4_ff) {
    uint8_t s = id4[0] ^ id4[1] ^ id4[2] ^ id4[3];
    mac[0] = 0x02; // locally administered, unicast
    mac[1] = s;
    mac[2] = id4[0];
    mac[3] = id4[1];
    mac[4] = id4[2];
    mac[5] = id4[3];
    return true;
  }
  return false;
}

// ============================ HTTP POST helpers ============================

static size_t buildSensorsJson(char *out, size_t maxLen) {
  size_t values_number = labels_len;
  if (values_number > SEND_ARR_SIZE)
    values_number = SEND_ARR_SIZE;

  uint8_t eepromMac[6];
  for (int i = 0; i < 6; ++i)
    eepromMac[i] = EEPROM.read(0 + i);
  const uint8_t *idMac = mac_valid(eepromMac) ? eepromMac : MAC_ADDR;
  String idStr = macToStringLocal(idMac);
  char idBuf[24];
  strncpy(idBuf, idStr.c_str(), sizeof(idBuf));
  idBuf[sizeof(idBuf) - 1] = '\0';

  size_t pos = 0;
  auto emit = [&](const char *s) {
    size_t n = strlen(s);
    if (pos + n >= maxLen)
      n = (maxLen > pos) ? (maxLen - pos) : 0;
    if (n) {
      memcpy(out + pos, s, n);
      pos += n;
    }
  };

  emit("{");
  emit("\"");
  emit("id");
  emit("\":\"");
  emit(idBuf);
  emit("\"");

  bool first = false;
  for (size_t i = 0; i < values_number; ++i) {
    const LabelEntry &spec = labels[i];
    if (!spec.name || spec.useStd)
      continue;

    emit(",");
    emit("\"");
    emit(spec.name);
    emit("\":");
    char num[24];
    dtostrf(send_arr[i], 0, 3, num);
    char *p = num;
    while (*p == ' ')
      ++p;
    emit(p);
  }
  emit("}");
  if (pos < maxLen)
    out[pos] = '\0';
  return pos;
}

static bool httpPostBody(EthernetClient &client, const char *hostHeader,
                         const char *path, const char *body, size_t bodyLen,
                         uint16_t timeoutMs) {
  client.print("POST ");
  client.print(path);
  client.print(" HTTP/1.1\r\n");
  client.print("Host: ");
  client.print(hostHeader);
  client.print("\r\n");
  client.print("User-Agent: SensorBox/1.0\r\n");
  client.print("Content-Type: application/json\r\n");
  if (API_KEY[0] != '\0') {
    client.print("X-API-Key: ");
    client.print(API_KEY);
    client.print("\r\n");
  }
  client.print("Connection: close\r\n");
  client.print("Content-Length: ");
  client.print(bodyLen);
  client.print("\r\n\r\n");

  client.write(reinterpret_cast<const uint8_t *>(body), bodyLen);
  client.flush();

  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (client.available())
      break;
  }
  size_t got = 0;
  while (client.available() && got < 256) {
    (void)client.read();
    ++got;
  }
  client.stop();
  return (got > 0); // consider success if any response data arrived
}

static bool httpPostSensorsImpl(EthernetClient &client, const char *hostHeader,
                                const char *path, uint16_t timeoutMs) {
  char body[768];
  size_t bodyLen = buildSensorsJson(body, sizeof(body) - 1);
  return httpPostBody(client, hostHeader, path, body, bodyLen, timeoutMs);
}

static const char *hwName(uint8_t hs) {
  switch (hs) {
  case EthernetNoHardware:
    return "NoHardware";
  case EthernetW5100:
    return "W5100";
  case EthernetW5200:
    return "W5200";
  case EthernetW5500:
    return "W5500";
  default:
    return "UnknownHW";
  }
}
static const char *linkName(uint8_t ls) {
  switch (ls) {
  case LinkON:
    return "LinkON";
  case LinkOFF:
    return "LinkOFF";
  default:
    return "LinkUnknown";
  }
}
static const char *sockName(uint8_t s) {
  switch (s) {
  case 0x00:
    return "CLOSED";
  case 0x13:
    return "INIT";
  case 0x14:
    return "LISTEN";
  case 0x15:
    return "SYNSENT";
  case 0x16:
    return "SYNRECV";
  case 0x17:
    return "ESTABLISHED";
  case 0x18:
    return "FIN_WAIT";
  case 0x1A:
    return "CLOSING";
  case 0x1B:
    return "TIME_WAIT";
  case 0x1C:
    return "CLOSE_WAIT";
  case 0x1D:
    return "LAST_ACK";
  default:
    return "OTHER";
  }
}
static void printEthDiag(EthernetClient &c) {
#if defined(ETHERNET_H)
  Serial.print(F("HW="));
  Serial.print(hwName(Ethernet.hardwareStatus()));
  Serial.print(F(" LINK="));
  Serial.print(linkName(Ethernet.linkStatus()));
#endif
  Serial.print(F(" Sock=0x"));
  Serial.print(c.status(), HEX);
  Serial.print(F(" ("));
  Serial.print(sockName(c.status()));
  Serial.println(F(")"));

  Serial.print(F("Local="));
  Serial.print(Ethernet.localIP());
  Serial.print(F(" GW="));
  Serial.print(Ethernet.gatewayIP());
  Serial.print(F(" DNS="));
  Serial.print(Ethernet.dnsServerIP());
  Serial.print(F(" MASK="));
  Serial.println(Ethernet.subnetMask());
}