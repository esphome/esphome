#include "wifi_twt.h"

#ifdef USE_WIFI_TWT
#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esp_wifi.h"
#include "esp_wifi_he.h"
#include "esp_event.h"

namespace esphome::wifi_twt {

static const char *const TAG = "wifi_twt";

// Convert interval_ms to TWT wake_invl_mant / wake_invl_expn.
// Wire format (IEEE 802.11ax): interval_µs = mant × 2^expn.
// Start at expn=10 (2^10=1024µs=1TU, the minimum useful granularity), then shift up
// until mantissa fits in uint16_t.
static void compute_interval_params(uint32_t interval_ms, uint16_t &mant, uint8_t &expn) {
  expn = 10;
  uint32_t m = (uint32_t) (((uint64_t) interval_ms * 1000 + 512) >> 10);
  if (m < 1)
    m = 1;
  while (m > 65535) {
    m = (m + 1) >> 1;
    expn++;
  }
  mant = static_cast<uint16_t>(m);
}

// Convert duration_ms to TWT min_wake_dura.
// The 802.11ax on-air format uses 256 µs/unit always (unit=0); wake_duration_unit in the
// config struct is ignored by the ESP-IDF firmware when building the frame. Max = 255×256µs = 65ms.
static void compute_duration_params(uint32_t duration_ms, uint8_t &dura) {
  uint32_t d = (duration_ms * 1000 + 128) / 256;
  if (d < 1)
    d = 1;
  if (d > 255) {
    ESP_LOGW(TAG, "wake_duration %" PRIu32 " ms exceeds 802.11ax max (65 ms); capping at 65 ms", duration_ms);
    d = 255;
  }
  dura = static_cast<uint8_t>(d);
}

static void itwt_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  auto *self = static_cast<WiFiTWT *>(arg);

  if (event_id == WIFI_EVENT_ITWT_SETUP) {
    auto *setup = static_cast<wifi_event_sta_itwt_setup_t *>(event_data);
    if (setup->status == 1) {  // ESP-IDF 6.x iTWT success is 1, not ESP_OK(0)
      // Wire format: mant×2^expn µs (IEEE 802.11ax); duration always in 256µs units on-air
      uint64_t sp_ms = (uint64_t) setup->config.wake_invl_mant * (1u << setup->config.wake_invl_expn) / 1000;
      uint32_t wd_us = setup->config.min_wake_dura * 256u;
      ESP_LOGI(TAG,
               "iTWT established: flow_id=%u, interval=%" PRIu64 " ms, duration=%" PRIu32
               " ms (mant=%u, exp=%u, dura=%u)",
               setup->config.flow_id, sp_ms, wd_us / 1000, setup->config.wake_invl_mant, setup->config.wake_invl_expn,
               setup->config.min_wake_dura);
      self->twt_setup_success(setup->config.flow_id);
    } else {
      ESP_LOGW(TAG, "iTWT setup failed: status=%d, cmd=%d", setup->status, setup->config.setup_cmd);
    }

  } else if (event_id == WIFI_EVENT_ITWT_TEARDOWN) {
    auto *td = static_cast<wifi_event_sta_itwt_teardown_t *>(event_data);
    ESP_LOGI(TAG, "iTWT torn down: flow_id=%u", td->flow_id);
    self->twt_teardown_received(td->flow_id);

  } else if (event_id == WIFI_EVENT_TWT_WAKEUP) {
    self->twt_wakeup_received();

  } else if (event_id == WIFI_EVENT_ITWT_PROBE) {
    auto *probe = static_cast<wifi_event_sta_itwt_probe_t *>(event_data);
    if (probe->status == ITWT_PROBE_SUCCESS) {
      ESP_LOGD(TAG, "iTWT probe: TSF sync OK");
    } else {
      ESP_LOGW(TAG, "iTWT probe: TSF sync failed, status=%d", probe->status);
    }
  }
}

void WiFiTWT::setup() {
  wifi::global_wifi_component->add_ip_state_listener(this);
  wifi::global_wifi_component->add_connect_state_listener(this);

  wifi_twt_config_t twt_cfg = {};
  twt_cfg.post_wakeup_event = true;
  if (esp_wifi_sta_twt_config(&twt_cfg) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure TWT post-wakeup event");
    this->mark_failed();
    return;
  }

  auto register_handler = [&](int32_t event_id, esp_event_handler_instance_t *handle, const char *name) -> bool {
    esp_err_t err = esp_event_handler_instance_register(WIFI_EVENT, event_id, &itwt_event_handler, this, handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register %s handler: %s", name, esp_err_to_name(err));
      return false;
    }
    return true;
  };

  auto unregister_handler = [](esp_event_handler_instance_t handle, int32_t event_id) {
    if (handle != nullptr)
      esp_event_handler_instance_unregister(WIFI_EVENT, event_id, handle);
  };

  if (!register_handler(WIFI_EVENT_ITWT_SETUP, &this->itwt_setup_handle_, "ITWT_SETUP") ||
      !register_handler(WIFI_EVENT_ITWT_TEARDOWN, &this->itwt_teardown_handle_, "ITWT_TEARDOWN") ||
      !register_handler(WIFI_EVENT_TWT_WAKEUP, &this->twt_wakeup_handle_, "TWT_WAKEUP") ||
      !register_handler(WIFI_EVENT_ITWT_PROBE, &this->itwt_probe_handle_, "ITWT_PROBE")) {
    unregister_handler(this->itwt_setup_handle_, WIFI_EVENT_ITWT_SETUP);
    unregister_handler(this->itwt_teardown_handle_, WIFI_EVENT_ITWT_TEARDOWN);
    unregister_handler(this->twt_wakeup_handle_, WIFI_EVENT_TWT_WAKEUP);
    unregister_handler(this->itwt_probe_handle_, WIFI_EVENT_ITWT_PROBE);
    this->mark_failed();
  }
}

void WiFiTWT::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                          const network::IPAddress &dns2) {
  if (!ips[0].is_set())
    return;
  if (this->auto_setup_)
    this->start_twt();
}

void WiFiTWT::start_twt() {
  uint8_t flow_id = this->active_flow_id_;
  if (flow_id != UINT8_MAX) {
    ESP_LOGD(TAG, "iTWT already active on flow_id=%u, skipping setup", flow_id);
    return;
  }
  wifi_phy_mode_t phymode;
  if (esp_wifi_sta_get_negotiated_phymode(&phymode) != ESP_OK) {
    ESP_LOGW(TAG, "Could not query negotiated PHY mode");
    return;
  }
  if (phymode != WIFI_PHY_MODE_HE20) {
    ESP_LOGW(TAG, "Connected AP does not support WiFi 6 (HE20) — TWT unavailable");
    return;
  }

  uint16_t mant;
  uint8_t expn, dura;
  compute_interval_params(this->wake_interval_ms_, mant, expn);
  compute_duration_params(this->wake_duration_ms_, dura);

  uint64_t sp_us = (uint64_t) mant * (1u << expn);
  uint32_t actual_interval_ms = (uint32_t) (sp_us / 1000);
  uint32_t actual_duration_us = (uint32_t) dura * 256u;

  ESP_LOGD(TAG, "iTWT setup: interval=%" PRIu32 " ms (mant=%u, exp=%u), duration=%" PRIu32 " ms (dura=%u)",
           actual_interval_ms, mant, expn, actual_duration_us / 1000, dura);

  // ESP-IDF requires SP (interval) >= WD (wake duration) + 10ms guard band.
  if (sp_us < actual_duration_us + 10000) {
    ESP_LOGW(TAG,
             "ESP-IDF TWT validation will reject: SP=%" PRIu32 " µs"
             " < WD=%" PRIu32 " µs + 10 ms guard. Increase wake_interval or reduce wake_duration.",
             (uint32_t) sp_us, actual_duration_us);
  }

  wifi_itwt_setup_config_t cfg = {};
  cfg.setup_cmd = static_cast<wifi_twt_setup_cmds_t>(this->setup_cmd_);
  cfg.flow_id = 0;
  cfg.twt_id = 0;
  cfg.flow_type = this->flow_type_;
  cfg.min_wake_dura = dura;
  cfg.wake_duration_unit = 0;  // 802.11ax on-air format always uses 256µs units
  cfg.wake_invl_expn = expn;
  cfg.wake_invl_mant = mant;
  cfg.trigger = 0;
  cfg.timeout_time_ms = 5000;

  esp_err_t err = esp_wifi_sta_itwt_setup(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "iTWT setup request failed: %s", esp_err_to_name(err));
  } else {
    ESP_LOGD(TAG, "iTWT setup request sent");
  }
}

void WiFiTWT::stop_twt() {
  uint8_t flow_id = this->active_flow_id_;
  if (flow_id == UINT8_MAX) {
    ESP_LOGW(TAG, "stop_twt() called but no active TWT agreement");
    return;
  }
  esp_err_t err = esp_wifi_sta_itwt_teardown(flow_id);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "iTWT teardown failed: %s", esp_err_to_name(err));
  }
}

}  // namespace esphome::wifi_twt

#endif  // USE_ESP32
#endif  // USE_WIFI_TWT
