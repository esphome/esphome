#include "zigbee_proxy.h"

#ifdef USE_ZIGBEE_PROXY

#include "esphome/core/log.h"

namespace esphome::zigbee_proxy {

static const char *const TAG = "zigbee_proxy";

void ZigbeeProxy::setup() { this->parent_->set_tap(this); }

void ZigbeeProxy::dump_config() { ESP_LOGCONFIG(TAG, "Zigbee Proxy:\n  Port: %s", this->parent_->get_name()); }

void ZigbeeProxy::on_device_rx(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    // Observation only: the detector never gates forwarding, so it adds no latency and a
    // frame it cannot parse still reaches the client, which judges it for itself.
    this->detector_.from_ncp(data[i]);

    uint8_t ack_num;
    if (this->detector_.take_pending_ack(ack_num)) {
      // The client suppresses its own ACKs, so this is the only acknowledgement the NCP
      // will see. Only ever sent for a frame that passed CRC and arrived in sequence.
      uint8_t frame[ASH_ACK_FRAME_MAX_SIZE];
      this->parent_->write_from_tap(frame, ash_build_ack_frame(frame, ack_num));
      ESP_LOGV(TAG, "Sent ACK for frame %u", ack_num);
    }
  }

  const bool armed = this->detector_.armed();
  if (armed != this->was_armed_) {
    this->was_armed_ = armed;
    ESP_LOGD(TAG, "ASH session %s",
             armed ? LOG_STR_LITERAL("detected, acknowledging frames")
                   : LOG_STR_LITERAL("lost, no longer acknowledging frames"));
  }
}

void ZigbeeProxy::on_client_tx(const uint8_t *data, size_t len) {
  // Scanning this direction only matters while waiting for the version command that
  // completes the handshake. Outside that window it is skipped entirely -- which is what
  // makes a firmware upload, all of which flows this way, essentially free.
  if (!this->detector_.needs_host_scan()) {
    return;
  }
  for (size_t i = 0; i < len; i++) {
    this->detector_.from_host(data[i]);
  }
}

void ZigbeeProxy::on_protocol_disabled() {
  // A client turning protocol handling off is usually about to reflash the radio, so the
  // handshake we saw says nothing about what will be on the wire next. Forget it: a real
  // ASH session announces itself again with an RSTACK.
  this->detector_.reset();
}

}  // namespace esphome::zigbee_proxy

#endif  // USE_ZIGBEE_PROXY
