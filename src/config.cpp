#include "config.h"

double send_arr[SEND_ARR_SIZE] = {0};
float sensors_dec[9] = {0};  // [CO, SO2, NO2, NO, H2S, O3, NH3, PM_2.5, PM_10]
float radiation_uSvh = 0.0f; // radiation
float service_t[2] = {0};    // temperature, dampness

const size_t sensors_dec_cnt = ARRLEN(sensors_dec);
const size_t service_t_cnt = ARRLEN(service_t);
// ---------- Sensor Box (Modbus RTU) ----------
bool active_ids[6] = {false, false, false, false, false, false};

const uint8_t PRIMARY_IDS[] = {2, 4, 6, 7};
const uint8_t PRIMARY_COUNT = ARRLEN(PRIMARY_IDS);

const uint8_t EXTRA_IF_ONLY2[] = {2, 5};
const uint8_t EXTRA_IF_ONLY4[] = {4, 3, 8};
const uint8_t EXTRA_IF_ONLY6[] = {6, 5, 10};
const uint8_t EXTRA_IF_ONLY7[] = {7, 5, 10};

const uint8_t EXTRA_ONLY2_CNT = ARRLEN(EXTRA_IF_ONLY2);
const uint8_t EXTRA_ONLY4_CNT = ARRLEN(EXTRA_IF_ONLY4);
const uint8_t EXTRA_ONLY6_CNT = ARRLEN(EXTRA_IF_ONLY6);
const uint8_t EXTRA_ONLY7_CNT = ARRLEN(EXTRA_IF_ONLY7);

const IPAddress ip_3(192, 168, 88, 3);
const IPAddress ip_4(192, 168, 88, 4);
const IPAddress ip_8(192, 168, 88, 8);

const byte MAC_ADDR[] = {0x02, 0x11, 0x22, 0x00, 0x00, 0x01};
const IPAddress STATIC_IP(192, 168, 88, 2);
const IPAddress GETWAY(192, 168, 88, 1);

uint8_t resp[256];

// Allow overriding via PlatformIO build flags.
const char SERVER_IP[] = SERVER_IP_VALUE;
const char API_KEY[] = API_KEY_VALUE;

const IPAddress NET_CHECK_IP(8, 8, 8, 8);

const LabelEntry labels[] = {
    {"CO", CH_CO, false},       {"SO2", CH_SO2, false},
    {"NO2", CH_NO2, false},     {"NO", CH_NO, false},
    {"H2S", CH_H2S, false},     {"O3", CH_O3, false},
    {"NH3", CH_NH3, false},     {"S_CO", CH_CO, true},
    {"S_SO2", CH_SO2, true},    {"S_NO2", CH_NO2, true},
    {"S_NO", CH_NO, true},      {"S_H2S", CH_H2S, true},
    {"S_O3", CH_O3, true},      {"S_NH3", CH_NH3, true},
    {"PM2.5", CH_PM2_5, false}, {"PM10", CH_PM10, false},
    {"R", CH_R, false},         {"S_T", CH_S_T, false},
    {"S_RH", CH_S_RH, false},
};

const size_t labels_len = ARRLEN(labels);

// const float INIT_SEND_ARR_0_13[][6] = {
//     {-0.7, -0.69, -0.71, -0.68, -0.72, -0.67},
//     {0.1, 0.11, 0.09, 0.12, 0.08, 0.13},
//     {0.01, 0.02, 0, 0.03, -0.01, 0.04},
//     {0.2, 0.21, 0.19, 0.22, 0.18, 0.23},
//     {22.6, 23.05, 22.15, 22.83, 22.37, 22.8},
//     {56, 57.12, 54.88, 56.56, 55.44, 56.2},
//     {4.76, 5, 4.52, 4.88, 4.64, 4.79},
//     {5.6, 5.88, 5.32, 5.74, 5.46, 5.63},
//     {28, 28.56, 27.44, 28.28, 27.72, 28.2},
//     {3, 3.15, 2.85, 3.07, 2.92, 3.03},
//     {540, 545.4, 534.6, 542.7, 537.3, 541},
//     {1010.22, 1020.32, 1000.12, 1015.27, 1005.17, 1011.22},
//     {9, 9.45, 8.55, 9.22, 8.78, 9.03},
//     {1.1, 1.16, 1.04, 1.13, 1.07, 1.13},
//     {0.5, 0.51, 0.49, 0.52, 0.48, 0.53},
//     {1, 1.05, 0.95, 1.02, 0.97, 1.03},
//     {0, 0.01, -0.01, 0.02, -0.02, 0.03},
//     {18, 18.36, 17.64, 18.18, 17.82, 18.2},
//     {19, 19.38, 18.62, 19.19, 18.81, 19.2},
//     {24.5, 24.99, 24.01, 24.75, 24.25, 24.7},
//     {24.5, 24.99, 24.01, 24.75, 24.25, 24.7}};
