#include "wifi_twt.h"

#ifdef USE_WIFI_TWT

#include "esphome/core/log.h"
#include "esphome/core/wake.h"
#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::wifi_twt {

static const char *const TAG = "wifi_twt";

// Retry backoff after a rejected/timed-out TWT setup: doubles from 5s, capped at 60s, and
// never gives up — a request costs nothing while the device just runs at full power.
static constexpr uint32_t SETUP_RETRY_BASE_MS = 5000;
static constexpr uint32_t SETUP_RETRY_MAX_MS = 60000;

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
  if (ssid.empty()) {
    this->setup_pending_ = false;
    this->setup_retry_count_ = 0;
    this->cancel_timeout("twt_retry");
    if (this->active_flow_id_ != UINT8_MAX) {
      ESP_LOGD(TAG, "WiFi disconnected — resetting active TWT flow_id");
      this->active_flow_id_ = UINT8_MAX;
      this->reconfigure_pending_ = true;
      // AP-side teardown event will not arrive after disconnect; fire stop callback directly.
      this->defer([this]() { this->stop_callback_.call(); });
    }
  }
}

void WiFiTWT::twt_setup_failed() {
  this->setup_pending_ = false;
  this->defer([this]() {
    // Guard: skip if disabled, or a session was established some other way, before this
    // defer ran.
    if (this->disabled_ || this->active_flow_id_ != UINT8_MAX)
      return;
    // Stops growing once the delay hits its ceiling; avoids unbounded growth for no benefit.
    if (this->setup_retry_count_ < 5)
      this->setup_retry_count_++;
    uint32_t delay_ms = SETUP_RETRY_BASE_MS << (this->setup_retry_count_ - 1);
    if (delay_ms > SETUP_RETRY_MAX_MS)
      delay_ms = SETUP_RETRY_MAX_MS;
    ESP_LOGW(TAG, "iTWT setup rejected (attempt %u); retrying in %" PRIu32 " ms", this->setup_retry_count_, delay_ms);
    this->set_timeout("twt_retry", delay_ms, [this]() { this->start_twt(); });
  });
  wake_loop_threadsafe();
}

void WiFiTWT::twt_setup_success(uint8_t flow_id) {
  this->setup_pending_ = false;
  this->setup_retry_count_ = 0;
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
  if (this->wakeup_callback_.empty())
    return;
  this->defer([this]() { this->wakeup_callback_.call(); });
  wake_loop_threadsafe();
}

void WiFiTWT::disable_twt() {
  if (this->disabled_) {
    ESP_LOGW(TAG, "wifi_twt.disable called but TWT is already disabled");
    return;
  }
  this->was_active_before_disable_ = (this->active_flow_id_ != UINT8_MAX);
  this->disabled_ = true;
  this->reconfigure_pending_ = false;
  if (this->was_active_before_disable_) {
    ESP_LOGD(TAG, "TWT disabled — stopping active agreement");
    this->stop_twt();
  } else {
    ESP_LOGD(TAG, "TWT disabled — no active agreement");
  }
}

void WiFiTWT::enable_twt() {
  this->disabled_ = false;
  if (this->was_active_before_disable_) {
    this->was_active_before_disable_ = false;
    ESP_LOGD(TAG, "TWT enabled — resuming previous agreement");
    this->start_twt();
  } else {
    ESP_LOGD(TAG, "TWT enabled — no previous agreement to resume; call wifi_twt.start to negotiate");
  }
}

#ifdef USE_OTA_STATE_LISTENER
void WiFiTWT::on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *component) {
  if (state == ota::OTA_STARTED) {
    this->twt_active_before_ota_ = (this->active_flow_id_ != UINT8_MAX);
    if (this->twt_active_before_ota_)
      this->stop_twt();
  } else if (state == ota::OTA_COMPLETED) {
    // A completed OTA reboots into the new firmware momentarily (see esphome/ota), which
    // applies its own wifi_twt config from a clean boot — nothing to resume here.
    this->twt_active_before_ota_ = false;
  } else if (state == ota::OTA_ABORT) {
    // Execution continues on the current firmware, so resume regardless of auto_setup_ —
    // twt_active_before_ota_ already proves a session was active before the failed OTA.
    if (this->twt_active_before_ota_) {
      this->twt_active_before_ota_ = false;
      if (!this->disabled_)
        this->start_twt();
    }
  }
}
#endif

}  // namespace esphome::wifi_twt

#ifndef USE_ESP32
// Stub for platforms where no implementation exists; Python validation prevents this path.
void esphome::wifi_twt::WiFiTWT::setup() {
  ESP_LOGE("wifi_twt", "wifi_twt: no implementation for this platform");
  this->mark_failed();
}
#endif

#endif  // USE_WIFI_TWT
