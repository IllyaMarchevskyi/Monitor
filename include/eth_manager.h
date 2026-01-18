#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <Ethernet.h>
#include <string.h>
#include <utility/w5100.h>

extern EthernetServer modbus_server;
extern EthernetServer serial_server;

void initEthernet();

bool httpPostSensors(const char *host, uint16_t port, const char *path);
bool httpPostSensors(const IPAddress &ip, uint16_t port, const char *path);

bool pingId_Ethernet(const IPAddress &ip, uint16_t port, uint16_t timeoutMs);