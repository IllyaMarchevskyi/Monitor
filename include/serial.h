#include <Ethernet.h>

extern EthernetClient client;

void streamLogData();

inline void logLine() {
  Serial.println();
  if (client && client.connected())
    client.println();
}

template <typename T> inline void logLine(const T &msg, bool ln = false) {
  if (ln)
    Serial.println(msg);
  else
    Serial.print(msg);

  if (client && client.connected()) {
    if (ln)
      client.println(msg);
    else
      client.print(msg);
  }
}

template <typename T>
inline void logLine(const T &msg, int base, bool ln = false) {
  if (ln)
    Serial.println(msg, base);
  else
    Serial.print(msg, base);

  if (client && client.connected()) {
    if (ln)
      client.println(msg, base);
    else
      client.print(msg, base);
  }
}
