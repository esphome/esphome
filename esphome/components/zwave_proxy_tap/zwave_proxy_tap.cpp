#include "zwave_proxy_tap.h"

#ifdef USE_ZWAVE_PROXY_TAP

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::zwave_proxy_tap {

static const char *const TAG = "zwave_proxy_tap";

void ZWaveProxyTap::setup() { this->parent_->set_tap(this); }

void ZWaveProxyTap::dump_config() { ESP_LOGCONFIG(TAG, "Z-Wave Proxy Tap:\n  Port: %s", this->parent_->get_name()); }

void ZWaveProxyTap::on_device_rx(const uint8_t *data, size_t len) {
  this->detector_.begin_batch(App.get_loop_component_start_time());
  for (size_t i = 0; i < len; i++) {
    // Observation only: the detector never gates forwarding, so it adds no latency and a
    // frame it cannot parse still reaches the client, which judges it for itself.
    this->detector_.from_device(data[i]);

    if (this->detector_.take_pending_ack()) {
      // The client suppresses its own ACKs, so this is the only acknowledgement the
      // controller will see. Only ever sent for a frame that passed its checksum.
      this->parent_->write_from_tap(&ZWAVE_ACK_BYTE, 1);
      ESP_LOGV(TAG, "Sent ACK");
    }
  }

  const bool armed = this->detector_.armed();
  if (armed != this->was_armed_) {
    this->was_armed_ = armed;
    ESP_LOGD(TAG, "Serial API session %s",
             armed ? LOG_STR_LITERAL("detected, acknowledging frames")
                   : LOG_STR_LITERAL("lost, no longer acknowledging frames"));
  }
}

void ZWaveProxyTap::on_client_tx(const uint8_t *data, size_t len) {
  // Scanning this direction only matters until an exchange completes. Once armed it is
  // skipped entirely -- which is what makes a firmware upload, all of which flows this
  // way, essentially free.
  if (!this->detector_.needs_host_scan()) {
    return;
  }
  this->detector_.begin_batch(App.get_loop_component_start_time());
  for (size_t i = 0; i < len; i++) {
    this->detector_.from_host(data[i]);
  }
}

void ZWaveProxyTap::on_protocol_disabled() {
  // A client turning protocol handling off is usually about to reflash the controller, so
  // the exchange we saw says nothing about what will be on the wire next. Forget it: a
  // real session proves itself again with another exchange.
  this->detector_.reset();
}

void ZWaveProxyTap::on_device_disconnected() {
  // The controller lost power, so the exchange we saw belongs to a session that no longer
  // exists. Whatever appears next has to prove itself again.
  if (this->was_armed_) {
    ESP_LOGD(TAG, "Device removed, no longer acknowledging frames");
    this->was_armed_ = false;
  }
  this->detector_.reset();
}

}  // namespace esphome::zwave_proxy_tap

#endif  // USE_ZWAVE_PROXY_TAP
