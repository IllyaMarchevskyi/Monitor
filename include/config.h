#pragma once

#include <Arduino.h>
#include <Ethernet.h> // дає IPAddress та byte

// ---------- Helpers ----------
#ifndef ARRLEN
#define ARRLEN(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define SEC 1000UL
#define MIN (60UL * SEC)

// --------- Globals mass --------------
extern double send_arr[];
extern uint32_t sensors_dec[];
extern uint32_t radiation_uSvh;
extern uint32_t service_t[];

// ---------- Value Timers -------------
constexpr uint32_t SernsorBoxTimeSleep = 0;

// ---------- Pins ----------
constexpr uint8_t RS485_DIR_PIN = 5; // DE/RE for RS-485 (Sensor Box)
constexpr uint8_t BDBG_DIR_PIN = 4;  // TX enable for BDBG line (if used)
constexpr uint8_t ETH_CS = 10;       // Ethernet CS

// ---------- UART Speeds & Formats ----------
constexpr uint32_t SERIAL0_BAUD = 115200; // USB debug
constexpr uint32_t SERIAL2_BAUD = 19200;  // BDBG-09, 8N1
constexpr uint32_t SERIAL3_BAUD = 9600;   // Sensor Box

// ---------- BDBG-09 UART Timings ----------
constexpr uint32_t BDBG_FIRST_BYTE_TIMEOUT_MS = 500;
constexpr uint32_t BDBG_INTERBYTE_TIMEOUT_MS = 50;

// ---------- Sensor Box (Modbus RTU) ----------
// Primary/extra device IDs
extern const uint8_t PRIMARY_IDS[];
extern const uint8_t PRIMARY_COUNT;

extern const uint8_t EXTRA_IF_ONLY2[];
extern const uint8_t EXTRA_IF_ONLY4[];
extern const uint8_t EXTRA_IF_ONLY6[];
extern const uint8_t EXTRA_IF_ONLY7[];

extern const uint8_t EXTRA_ONLY2_CNT;
extern const uint8_t EXTRA_ONLY4_CNT;
extern const uint8_t EXTRA_ONLY6_CNT;
extern const uint8_t EXTRA_ONLY7_CNT;

// Remote bridge (ID 4)
extern const IPAddress ip_3;
extern const IPAddress ip_4;
extern const IPAddress ip_8;

constexpr int port = 5581;
constexpr uint8_t time_sleep = 20;

// PM sensor map
constexpr uint8_t PM_ID = 10;
constexpr uint8_t PM_START_ADDR = 1;
constexpr uint8_t PM_REG_COUNT = 2;
constexpr int pm_divider = 1000;

// Gas register map
constexpr uint16_t GAS_START_ADDR = 20;
constexpr uint16_t GAS_REG_COUNT = 6;

// Gas register map for 2 id
constexpr uint16_t GAS_START_ADDR2 = 0;
constexpr uint16_t GAS_REG_COUNT2 = 6;
constexpr int co_divider = 100;
constexpr int so2_no2_divider = 1000;
constexpr int divider = 100;

// ---------- Ethernet / Modbus TCP ----------
// Local TCP server
constexpr uint16_t MODBUS_TCP_PORT = 502;

extern const byte MAC_ADDR[];
extern const IPAddress STATIC_IP;
extern const IPAddress GETWAY;

extern uint8_t resp[256];

// Remote Server
extern const char SERVER_IP[]; // рядок як масив символів
constexpr uint16_t server_port = 4000;
extern const char API_KEY[];

// ---------- Relay ----------
extern const IPAddress NET_CHECK_IP; // Google DNS
constexpr uint16_t NET_CHECK_PORT = 53;

constexpr uint8_t UNIT_ID = 12; // ID пристрою
constexpr uint8_t CH = 0;       // Канал взаємодії від 0-3
constexpr uint16_t RELAY_HTTP_PORT = 8080;
constexpr uint32_t RELAY_PULSE_MS = 2 * MIN;

// ---------- Periods / Timings (ms) ----------
constexpr uint32_t BDBG_REQ_PERIOD_MS = 30 * SEC;
constexpr uint32_t DRAW_MONITORING = 1 * MIN;
constexpr uint32_t RELAY_SLEEP = 10 * MIN;
constexpr uint32_t SEND_DATA_TO_SERVER = 1 * MIN;

// ---------- Exported Data Array ----------
constexpr int SEND_ARR_SIZE = 30;
constexpr float DEFAULT_SEND_VAL = -1.0f;

// ---------- Display Parameters ----------
constexpr int col1_x = 0, col1_x_value = 80;
constexpr int col2_x = 170, col2_x_value = 240;
constexpr int col3_x = 330, col3_x_value = 410;
constexpr int y_step = 30;
constexpr int len_col = 7;

enum ChannelIndex : uint8_t {
  CH_CO,
  CH_SO2,
  CH_NO2,
  CH_NO,
  CH_H2S,
  CH_O3,
  CH_NH3,
  CH_PM2_5,
  CH_PM10,
  CH_R,
  CH_S_T,
  CH_S_RH,
  CH_TOTAL_COUNT
};
constexpr size_t CH_COUNT = CH_TOTAL_COUNT;

struct LabelEntry {
  const char *name;
  ChannelIndex channel;
  bool useStd;
};

extern const LabelEntry labels[];
extern const size_t labels_len;

// (опційно) якщо захочеш повернути INIT_SEND_ARR_0_13 — теж робимо extern тут
extern const float INIT_SEND_ARR_0_13[][6];
