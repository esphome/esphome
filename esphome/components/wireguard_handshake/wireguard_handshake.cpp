#include "wireguard_handshake.h"

#include <ctime>
#include "esphome/core/log.h"
#include "esphome/core/component.h"

namespace esphome {
namespace wireguard_handshake {

static const char *const TAG = "wireguard_handshake.sensor";

void WireguardHandshake::update() {
  if (this->wireguard_ == nullptr) {
    ESP_LOGW(TAG, "wireguard component not available");
    this->has_state_ = false;
    return;
  }

  if (this->wireguard_->is_failed()) {
    ESP_LOGE(TAG, "wireguard component is failed");
    this->mark_failed();
    return;
  }

  time_t lhs = this->wireguard_->get_latest_handshake();

  if (lhs > 0) {
    publish_state((float) lhs);
  } else {
    this->has_state_ = false;
  }
}

void WireguardHandshake::dump_config() {
  LOG_SENSOR("", "WireGuard Latest Handshake", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace wireguard_handshake
}  // namespace esphome

// vim: tabstop=2 shiftwidth=2 expandtab
