#include "zwave_proxy_tap.h"

#ifdef USE_ZWAVE_PROXY_TAP

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::zwave_proxy_tap {

ESPHOME_LOG_TAG(TAG, "zwave_proxy_tap");

void ZWaveProxyTap::setup() { this->parent_->set_tap(this); }

void ZWaveProxyTap::dump_config() { ESP_LOGCONFIG(TAG, "Z-Wave Proxy Tap:\n  Port: %s", this->parent_->get_name()); }

void ZWaveProxyTap::on_device_rx(const uint8_t *data, size_t len) {
  const uint32_t now = App.get_loop_component_start_time();
  for (size_t i = 0; i < len; i++) {
    if (this->scanner_.feed(data[i], now)) {
      this->parent_->write_from_tap(&ZWAVE_ACK_BYTE, 1);
      ESP_LOGV(TAG, "Sent ACK");
    }
  }
}

void ZWaveProxyTap::on_protocol_enabled() { this->scanner_.reset(); }

void ZWaveProxyTap::on_device_disconnected() { this->scanner_.reset(); }

}  // namespace esphome::zwave_proxy_tap

#endif  // USE_ZWAVE_PROXY_TAP
