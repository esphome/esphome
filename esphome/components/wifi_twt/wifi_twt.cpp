#include "wifi_twt.h"

#ifdef USE_WIFI_TWT

#include "esphome/core/log.h"
#include "esphome/core/wake.h"

namespace esphome::wifi_twt {

static const char *const TAG = "wifi_twt";

void WiFiTWT::dump_config() {
  ESP_LOGCONFIG(TAG, "WiFi TWT:");
  ESP_LOGCONFIG(TAG, "  Wake Interval: %" PRIu32 " ms", this->wake_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Wake Duration: %" PRIu32 " ms", this->wake_duration_ms_);
  static constexpr const char *const SETUP_CMD_NAMES[] = {"request", "suggest", "demand"};
  const char *cmd_str = this->setup_cmd_ < 3 ? SETUP_CMD_NAMES[this->setup_cmd_] : "unknown";
  ESP_LOGCONFIG(TAG, "  Setup Cmd: %s", cmd_str);
  ESP_LOGCONFIG(TAG, "  Flow Type: %s", this->flow_type_ == 0 ? "announced" : "unannounced");
  if (this->active_flow_id_ != UINT8_MAX) {
    ESP_LOGCONFIG(TAG, "  Active Flow ID: %u", this->active_flow_id_.load());
  } else {
    ESP_LOGCONFIG(TAG, "  Status: not negotiated");
  }
}

void WiFiTWT::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) {
  if (ssid.empty() && this->active_flow_id_ != UINT8_MAX) {
    ESP_LOGD(TAG, "WiFi disconnected — resetting active TWT flow_id");
    this->active_flow_id_ = UINT8_MAX;
    // AP-side teardown event will not arrive after disconnect; fire stop callback directly.
    this->defer([this]() { this->stop_callback_.call(); });
  }
}

void WiFiTWT::twt_setup_success(uint8_t flow_id) {
  bool was_active = (this->active_flow_id_ != UINT8_MAX);
  this->active_flow_id_ = flow_id;
  if (was_active) {
    ESP_LOGD(TAG, "iTWT renegotiated: new flow_id=%u", flow_id);
    return;
  }
  this->defer([this]() {
    // Guard: skip if teardown arrived before this defer ran.
    if (this->active_flow_id_ != UINT8_MAX)
      this->start_callback_.call();
  });
  wake_loop_threadsafe();
}

void WiFiTWT::twt_teardown_received(uint8_t flow_id) {
  if (flow_id != this->active_flow_id_) {
    ESP_LOGW(TAG, "teardown for unknown flow_id=%u (active=%u)", flow_id, (uint8_t) this->active_flow_id_);
    return;
  }
  this->active_flow_id_ = UINT8_MAX;
  this->defer([this]() {
    // Guard: skip if a new session was established before this defer ran.
    if (this->active_flow_id_ == UINT8_MAX)
      this->stop_callback_.call();
  });
  wake_loop_threadsafe();
}

void WiFiTWT::twt_wakeup_received() {
  this->defer([this]() { this->wakeup_callback_.call(); });
  wake_loop_threadsafe();
}

}  // namespace esphome::wifi_twt

#ifndef USE_ESP32
// Stub for platforms where no implementation exists; Python validation prevents this path.
void esphome::wifi_twt::WiFiTWT::setup() {
  ESP_LOGE("wifi_twt", "wifi_twt: no implementation for this platform");
  this->mark_failed();
}
#endif

#endif  // USE_WIFI_TWT
