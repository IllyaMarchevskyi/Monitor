/*
  Environmental Station — Refactored Main
  Modules:
    - TFT (TFT_eSPI / ST7796)
    - BDBG-09 (Serial2 19200 8N1, cmd 55 AA 01 → 10 bytes)
    - Sensor Box (RS-485 Modbus RTU on Serial3 ): CO/SO2/NO2 as float, start 20,
  len 6
    - Modbus TCP (ENC28J60) exposes send_arr[] as float32 (2 regs/value)

  This main keeps logic readable; all constants live in EnvStationConfig.h
*/

#include "bdbg.h"
#include "config.h"
#include "display.h"
#include "eth_manager.h"
#include "modbus.h"
#include "relay.h"
#include "sensor_box.h"
#include "utils.h"

bool alive2 = false, alive4 = true, alive6 = false, alive7 = false;

static void initSerials();
static void pollMonitoringData();

void setup() {
  for (int i = 0; i < SEND_ARR_SIZE; i++)
    send_arr[i] = DEFAULT_SEND_VAL;

  initDisplay();
  Serial.begin(SERIAL0_BAUD);
  Serial.setTimeout(10);
  Serial.println("Setup Monitoring");

  initSerials();

  pinMode(RS485_DIR_PIN, OUTPUT);
  pinMode(BDBG_DIR_PIN, OUTPUT);
  digitalWrite(BDBG_DIR_PIN, LOW);

  sensor_box.preTransmission(pre_transmission_main);
  sensor_box.postTransmission(post_transmission_main);

  initEthernet();
  Serial.println("Finsh Initialization");
}
void loop() {
  uint32_t t1 = millis();

  // TIME_CALL("SensorBox and SensorZTS3008",
  //           poll_SensorBox_SensorZTS3008(alive2, alive4, alive6, alive7));

  // // BDBG-09
  // if (!alive4) {
  //   TIME_CALL("Radiation", pollRadiation());
  // }

  // TIME_CALL("Work with data", collectAndAverageEveryMinute());
  TIME_CALL("Monitoring Data", pollMonitoringData());
  TIME_CALL("Modbus connect", modbusTcpServiceOnce());
  TIME_CALL("Drawing value on arduino", drawValue(alive2, alive4, alive6, alive7));
  TIME_CALL("Ralay", ensureNetOrRebootPort0());
  TIME_CALL("Send to Server1",
            httpPostSensors(SERVER_IP, server_port, "/ingest"));
  uint32_t dt_ms = millis() - t1;
  if (dt_ms > 500) {
    Serial.print("Час: ");
    Serial.print(dt_ms);
    Serial.println(" ms");
  }
}

// =============================== Initialization ============================

static void initSerials() {
  Serial2.begin(SERIAL2_BAUD); // BDBG 8N1
  Serial2.setTimeout(10);

  Serial3.begin(SERIAL3_BAUD); // Sensor Box 8E1
  Serial3.setTimeout(10);
}

static void pollMonitoringData() {
  if (!time_guard_allow("monitoring", MONITOR_TIME_SLEEP))
    return;

  poll_SensorBox_SensorZTS3008(alive2, alive4, alive6, alive7);

  if (!alive4) {
    pollRadiation();
  }

  collectAndAverageEveryMinute();
}