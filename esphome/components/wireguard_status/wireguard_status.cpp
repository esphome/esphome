#include "wireguard_status.h"

#include "esphome/core/log.h"
#include "esphome/core/component.h"

namespace esphome {
namespace wireguard_status {

static const char *const TAG = "wireguard_status.binary_sensor";

void WireguardStatus::update() {
  if (this->wireguard_ == nullptr) {
    ESP_LOGW(TAG, "wireguard component not available");
    return;
  }

  if (this->wireguard_->is_failed()) {
    ESP_LOGE(TAG, "wireguard component is failed");
    this->mark_failed();
    return;
  }

  publish_state(this->wireguard_->is_peer_up());
}

void WireguardStatus::dump_config() {
  LOG_BINARY_SENSOR("", "WireGuard Status", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace wireguard_status
}  // namespace esphome

// vim: tabstop=2 shiftwidth=2 expandtab
