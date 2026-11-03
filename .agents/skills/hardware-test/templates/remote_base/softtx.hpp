#pragma once
// Bit-bang a frame with interrupts left on, so a receiver on the same pin sees every edge.
// The ESP8266 remote_transmitter disables interrupts while it sends, which hides the edges.
#include "esphome/components/remote_base/remote_base.h"

namespace softtx {
inline void send(const esphome::remote_base::RemoteTransmitData &d, const char *name) {
  ESP_LOGI("rbt", "tx %s", name);
  for (int32_t v : d.get_data()) {
    digitalWrite(5, v > 0 ? HIGH : LOW);
    delayMicroseconds(v > 0 ? v : -v);
  }
  digitalWrite(5, LOW);
}
}  // namespace softtx
