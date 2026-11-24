// Report notify counters once a burst has gone quiet for 2 s (esphome#22955). They are not in ESPHome: in the tree
// under test, define these uint32_t globals in esphome::bluetooth_connection and count in
// BluetoothConnection::on_notify_data: ndrop_rx every call, ndrop_nocli when there is no API connection, ndrop_drop
// when send_message() fails, ndrop_drop_run consecutive failures (reset on success) and ndrop_drop_maxrun its maximum.
#pragma once
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
namespace esphome::bluetooth_connection {
extern uint32_t ndrop_rx, ndrop_nocli, ndrop_drop, ndrop_drop_run, ndrop_drop_maxrun;
}
namespace ndrop {
using namespace esphome::bluetooth_connection;
static uint32_t last_rx, last_change, reported_rx;
inline void poll() {
  uint32_t now = esphome::millis();
  if (ndrop_rx != last_rx) {
    last_rx = ndrop_rx;
    last_change = now;
  } else if (last_rx != reported_rx && now - last_change > 2000) {
    reported_rx = last_rx;
    ESP_LOGI("ndrop", "NDROP rx=%u nocli=%u drop=%u maxrun=%u", (unsigned) ndrop_rx, (unsigned) ndrop_nocli,
             (unsigned) ndrop_drop, (unsigned) ndrop_drop_maxrun);
  }
}
}  // namespace ndrop
