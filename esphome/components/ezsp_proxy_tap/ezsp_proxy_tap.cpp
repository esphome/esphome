#include "ezsp_proxy_tap.h"

#ifdef USE_EZSP_PROXY_TAP

#include "esphome/core/log.h"

namespace esphome::ezsp_proxy_tap {

static const char *const TAG = "ezsp_proxy_tap";

void EzspProxyTap::setup() { this->parent_->set_tap(this); }

void EzspProxyTap::dump_config() { ESP_LOGCONFIG(TAG, "EZSP Proxy Tap:\n  Port: %s", this->parent_->get_name()); }

void EzspProxyTap::on_device_rx(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (this->acknowledger_.feed(data[i])) {
      uint8_t frame[ASH_ACK_FRAME_SIZE];
      ash_build_ack_frame(frame, this->acknowledger_.ack_num());
      this->parent_->write_from_tap(frame, sizeof(frame));
      ESP_LOGV(TAG, "Sent ACK %u", this->acknowledger_.ack_num());
    }
  }
}

void EzspProxyTap::on_protocol_enabled() {
  // Numbering from an earlier session is stale
  this->acknowledger_.reset();
}

void EzspProxyTap::on_device_disconnected() {
  // The frame numbering ended with the NCP's power; whatever boots next starts over
  this->acknowledger_.reset();
}

}  // namespace esphome::ezsp_proxy_tap

#endif  // USE_EZSP_PROXY_TAP
