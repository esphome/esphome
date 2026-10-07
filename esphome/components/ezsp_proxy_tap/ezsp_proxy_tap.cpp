#include "ezsp_proxy_tap.h"

#ifdef USE_EZSP_PROXY_TAP

#include "esphome/core/log.h"

namespace esphome::ezsp_proxy_tap {

static const char *const TAG = "ezsp_proxy_tap";

void EzspProxyTap::setup() { this->parent_->set_tap(this); }

void EzspProxyTap::dump_config() { ESP_LOGCONFIG(TAG, "EZSP Proxy Tap:\n  Port: %s", this->parent_->get_name()); }

void EzspProxyTap::on_device_rx(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    // Observation only: this never gates forwarding, so it adds no latency and a frame it
    // cannot parse still reaches the client, which judges it for itself.
    this->acknowledger_.feed(data[i]);

    uint8_t ack_num;
    if (this->acknowledger_.take_pending_ack(ack_num)) {
      // The client suppresses its own ACKs, so this is the only acknowledgement the NCP
      // will see. Only ever sent for a frame that passed CRC and arrived in sequence.
      uint8_t frame[ASH_ACK_FRAME_MAX_SIZE];
      this->parent_->write_from_tap(frame, ash_build_ack_frame(frame, ack_num));
      ESP_LOGV(TAG, "Sent ACK for frame %u", ack_num);
    }
  }
}

void EzspProxyTap::on_protocol_enabled() {
  // The frame numbering we last followed may be from an earlier session. The client resets
  // the NCP as it starts, and its RSTACK sets the numbering again.
  this->acknowledger_.reset();
}

void EzspProxyTap::on_device_disconnected() {
  // The frame numbering ended with the NCP's power; whatever boots next starts over
  this->acknowledger_.reset();
}

}  // namespace esphome::ezsp_proxy_tap

#endif  // USE_EZSP_PROXY_TAP
