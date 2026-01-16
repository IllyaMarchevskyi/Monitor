#include "modbus.h"
#include "config.h"
#include "eth_manager.h"

static void modbusTcpHandleRequest(EthernetClient &client,
                                   const uint8_t *request, size_t n);


void modbusTcpServiceOnce() {
  EthernetClient client = server.available();
  if (!client || !client.connected())
    return;

  if (!client.connected() && client.available() == 0)
    client.stop();

  if (client.available() >= 12) {
    uint8_t req[260];
    size_t n = client.read(req, sizeof(req));
    modbusTcpHandleRequest(client, req, n);
    client.flush();
  }
}

static void modbusTcpHandleRequest(EthernetClient &client,
                                   const uint8_t *request, size_t n) {
  if (n < 12)
    return;

  uint16_t trans_id = (request[0] << 8) | request[1];
  uint16_t len = (request[4] << 8) | request[5];
  uint8_t unit_id = request[6];

  uint8_t func = request[7];
  uint16_t addr = (request[8] << 8) | request[9];
  uint16_t count = (request[10] << 8) | request[11];

  uint16_t total_regs = SEND_ARR_SIZE * 2; // 2 regs per float32

  if (func == 3 && (addr + count) <= total_regs) {
    uint16_t byte_count = count * 2;
    uint16_t pdu_len = 1 + 1 + byte_count; // func + byte_count + data

    uint8_t resp[260];
    // MBAP
    resp[0] = (trans_id >> 8) & 0xFF;
    resp[1] = (trans_id) & 0xFF;
    resp[2] = 0;
    resp[3] = 0; // protocol=0
    resp[4] = (pdu_len + 1) >> 8;
    resp[5] = (pdu_len + 1) & 0xFF; // unit + PDU
    resp[6] = unit_id;

    // PDU
    resp[7] = func;
    resp[8] = byte_count;

    for (uint16_t i = 0; i < count; i++) {
      uint16_t reg_index = addr + i;
      uint16_t fidx = reg_index / 2; // value index
      uint8_t part = reg_index % 2;  // 0=AB, 1=CD

      float f = (float)send_arr[fidx];
      uint32_t fbits;
      memcpy(&fbits, &f, sizeof(float));

      if (part == 0) {                           // high word
        resp[9 + i * 2] = (fbits) & 0xFF;        // C
        resp[10 + i * 2] = (fbits >> 8) & 0xFF;  // D
      } else {                                   // low word
        resp[9 + i * 2] = (fbits >> 16) & 0xFF;  // A
        resp[10 + i * 2] = (fbits >> 24) & 0xFF; // B
      }
    }

    client.write(resp, 9 + byte_count);
  } else {
    uint8_t err[] = {(uint8_t)(trans_id >> 8),
                     (uint8_t)trans_id,
                     0,
                     0,
                     0,
                     3,
                     unit_id,
                     (uint8_t)(func | 0x80),
                     0x02};
    client.write(err, sizeof(err));
  }
}
