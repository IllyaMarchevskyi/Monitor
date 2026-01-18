#include "serial.h"
#include "config.h"
#include "eth_manager.h"

EthernetClient client;

void streamLogData() {
  client = serial_server.available();
  if (!client || !client.connected()) {
    return;
  }

  if (!client.connected() && client.available() == 0) {
    return;
  }
}


