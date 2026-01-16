#pragma once
#include <ModbusMaster.h>

extern ModbusMaster sensor_box;

void poll_SensorBox_SensorZTS3008(bool &alive1, bool &alive2, bool &alive3,
                                  bool &alive4);