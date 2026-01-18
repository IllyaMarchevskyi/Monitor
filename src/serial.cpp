#include "serial.h"
#include "config.h"
#include "eth_manager.h"

EthernetClient client;

void streamLogData() {
  if (!client || !client.connected()) {
    client = serial_server.accept();
  }

  if (!client || !client.connected()) {
    return;
  }
}
