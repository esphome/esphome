#ifdef USE_ESP32

#include "esp32_touch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <cinttypes>

namespace esphome::esp32_touch {

template<size_t N> static const char *lookup_str(const char *const (&table)[N], size_t index) {
  return (index < N) ? table[index] : "UNKNOWN";
}

static const char *const TAG = "esp32_touch";

static constexpr uint32_t SETUP_MODE_LOG_INTERVAL_MS = 250;
static constexpr uint32_t INITIAL_STATE_DELAY_MS = 1500;
static constexpr uint32_t ONESHOT_SCAN_COUNT = 3;
static constexpr uint32_t ONESHOT_SCAN_TIMEOUT_MS = 2000;

// V1: called from esp_timer context (software filter)
// V2/V3: called from ISR context
// xQueueSendFromISR is safe from both contexts.

bool IRAM_ATTR ESP32TouchComponent::on_active_cb(touch_sensor_handle_t handle, const touch_active_event_data_t *event,
                                                 void *ctx) {
  auto *comp = static_cast<ESP32TouchComponent *>(ctx);
  TouchEvent te{event->chan_id, true};
  BaseType_t higher = pdFALSE;
  xQueueSendFromISR(comp->touch_queue_, &te, &higher);
  comp->enable_loop_soon_any_context();
  return higher == pdTRUE;
}

bool IRAM_ATTR ESP32TouchComponent::on_inactive_cb(touch_sensor_handle_t handle,
                                                   const touch_inactive_event_data_t *event, void *ctx) {
  auto *comp = static_cast<ESP32TouchComponent *>(ctx);
  TouchEvent te{event->chan_id, false};
  BaseType_t higher = pdFALSE;
  xQueueSendFromISR(comp->touch_queue_, &te, &higher);
  comp->enable_loop_soon_any_context();
  return higher == pdTRUE;
}

void ESP32TouchComponent::setup() {
  if (!this->create_touch_queue_()) {
    return;
  }

  // Create sample config - differs per hardware version
#ifdef USE_ESP32_VARIANT_ESP32
  touch_sensor_sample_config_t sample_cfg = TOUCH_SENSOR_V1_DEFAULT_SAMPLE_CONFIG(
      this->charge_duration_ms_, this->low_voltage_reference_, this->high_voltage_reference_);
#elif defined(USE_ESP32_VARIANT_ESP32P4)
  // div_num=8 (data scaling divisor), coarse_freq_tune=2, fine_freq_tune=2
  touch_sensor_sample_config_t sample_cfg = TOUCH_SENSOR_V3_DEFAULT_SAMPLE_CONFIG(8, 2, 2);
  sample_cfg.charge_times = this->charge_times_;
#else
  // ESP32-S2/S3 (V2)
  touch_sensor_sample_config_t sample_cfg = TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(
      this->charge_times_, this->low_voltage_reference_, this->high_voltage_reference_);
#endif

  // Create controller
  touch_sensor_config_t sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(1, &sample_cfg);
  sens_cfg.meas_interval_us = this->meas_interval_us_;
#ifndef USE_ESP32_VARIANT_ESP32
  sens_cfg.max_meas_time_us = 0;  // Disable measurement timeout (V2/V3 only)
#endif

  esp_err_t err = touch_sensor_new_controller(&sens_cfg, &this->sens_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create touch controller: %s", esp_err_to_name(err));
    this->cleanup_touch_queue_();
    this->mark_failed();
    return;
  }

  // Create channels for all children
  for (auto *child : this->children_) {
    touch_channel_config_t chan_cfg = {};
#ifdef USE_ESP32_VARIANT_ESP32
    chan_cfg.abs_active_thresh[0] = child->get_threshold();
    chan_cfg.charge_speed = TOUCH_CHARGE_SPEED_7;
    chan_cfg.init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT;
    chan_cfg.group = TOUCH_CHAN_TRIG_GROUP_BOTH;
#elif defined(USE_ESP32_VARIANT_ESP32P4)
    chan_cfg.active_thresh[0] = child->get_threshold();
#else
    // ESP32-S2/S3 (V2)
    chan_cfg.active_thresh[0] = child->get_threshold();
    chan_cfg.charge_speed = TOUCH_CHARGE_SPEED_7;
    chan_cfg.init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT;
#endif

    err = touch_sensor_new_channel(this->sens_handle_, child->get_channel_id(), &chan_cfg, &child->chan_handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create touch channel %d: %s", child->get_channel_id(), esp_err_to_name(err));
      this->cleanup_touch_queue_();
      this->mark_failed();
      return;
    }
  }

  // Configure filter
#ifdef USE_ESP32_VARIANT_ESP32
  // Software filter is REQUIRED for V1 on_active/on_inactive callbacks
  {
    touch_sensor_filter_config_t filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
    if (this->iir_filter_enabled_()) {
      filter_cfg.interval_ms = this->iir_filter_;
    }
    err = touch_sensor_config_filter(this->sens_handle_, &filter_cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to configure filter: %s", esp_err_to_name(err));
      this->cleanup_touch_queue_();
      this->mark_failed();
      return;
    }
  }
#else
  // V2/V3: Hardware benchmark filter
  {
    touch_sensor_filter_config_t filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
    if (this->filter_configured_) {
      filter_cfg.benchmark.filter_mode = this->filter_mode_;
      filter_cfg.benchmark.jitter_step = this->jitter_step_;
      filter_cfg.benchmark.denoise_lvl = this->noise_threshold_;
      filter_cfg.data.smooth_filter = this->smooth_level_;
      filter_cfg.data.debounce_cnt = this->debounce_count_;
    }
    err = touch_sensor_config_filter(this->sens_handle_, &filter_cfg);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to configure filter: %s", esp_err_to_name(err));
    }
  }
#endif

#if SOC_TOUCH_SUPPORT_DENOISE_CHAN
  if (this->denoise_configured_) {
    touch_denoise_chan_config_t denoise_cfg = {};
    denoise_cfg.charge_speed = TOUCH_CHARGE_SPEED_7;
    denoise_cfg.init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT;
    denoise_cfg.ref_cap = this->denoise_cap_level_;
    denoise_cfg.resolution = this->denoise_grade_;
    err = touch_sensor_config_denoise_channel(this->sens_handle_, &denoise_cfg);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to configure denoise: %s", esp_err_to_name(err));
    }
  }
#endif

#if SOC_TOUCH_SUPPORT_WATERPROOF
  if (this->waterproof_configured_) {
    touch_channel_handle_t guard_chan = nullptr;
    for (auto *child : this->children_) {
      if (child->get_channel_id() == this->waterproof_guard_ring_pad_) {
        guard_chan = child->chan_handle_;
        break;
      }
    }

    touch_channel_handle_t shield_chan = nullptr;
    touch_channel_config_t shield_cfg = {};
#ifdef USE_ESP32_VARIANT_ESP32P4
    shield_cfg.active_thresh[0] = 0;
    err = touch_sensor_new_channel(this->sens_handle_, SOC_TOUCH_MAX_CHAN_ID, &shield_cfg, &shield_chan);
#else
    shield_cfg.active_thresh[0] = 0;
    shield_cfg.charge_speed = TOUCH_CHARGE_SPEED_7;
    shield_cfg.init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT;
    err = touch_sensor_new_channel(this->sens_handle_, TOUCH_SHIELD_CHAN_ID, &shield_cfg, &shield_chan);
#endif
    if (err == ESP_OK) {
      touch_waterproof_config_t wp_cfg = {};
      wp_cfg.guard_chan = guard_chan;
      wp_cfg.shield_chan = shield_chan;
      wp_cfg.shield_drv = this->waterproof_shield_driver_;
      wp_cfg.flags.immersion_proof = 1;
      err = touch_sensor_config_waterproof(this->sens_handle_, &wp_cfg);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure waterproof: %s", esp_err_to_name(err));
      }
    } else {
      ESP_LOGW(TAG, "Failed to create shield channel: %s", esp_err_to_name(err));
    }
  }
#endif

  // Configure wakeup pads before enabling (must be done in INIT state)
  this->configure_wakeup_pads_();

  // Register callbacks
  touch_event_callbacks_t cbs = {};
  cbs.on_active = on_active_cb;
  cbs.on_inactive = on_inactive_cb;
  err = touch_sensor_register_callbacks(this->sens_handle_, &cbs, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register callbacks: %s", esp_err_to_name(err));
    this->cleanup_touch_queue_();
    this->mark_failed();
    return;
  }

  // Enable and start scanning
  err = touch_sensor_enable(this->sens_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable touch sensor: %s", esp_err_to_name(err));
    this->cleanup_touch_queue_();
    this->mark_failed();
    return;
  }

  // Do initial oneshot scans to populate baseline values
  for (uint32_t i = 0; i < ONESHOT_SCAN_COUNT; i++) {
    err = touch_sensor_trigger_oneshot_scanning(this->sens_handle_, ONESHOT_SCAN_TIMEOUT_MS);
    App.feed_wdt();  // 3 scans with 2s timeout might exceed WDT, so feed it here to be safe
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Oneshot scan %" PRIu32 " failed: %s", i, esp_err_to_name(err));
    }
  }

  err = touch_sensor_start_continuous_scanning(this->sens_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start continuous scanning: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
}

void ESP32TouchComponent::dump_config() {
#if !defined(USE_ESP32_VARIANT_ESP32P4)
  static constexpr const char *LV_STRS[] = {"0.5V", "0.6V", "0.7V", "0.8V"};
  static constexpr const char *HV_STRS[] = {"0.9V", "1.0V", "1.1V", "1.2V", "1.4V", "1.5V", "1.6V", "1.7V",
                                            "1.9V", "2.0V", "2.1V", "2.2V", "2.4V", "2.5V", "2.6V", "2.7V"};
  const char *lv_s = lookup_str(LV_STRS, this->low_voltage_reference_);
  const char *hv_s = lookup_str(HV_STRS, this->high_voltage_reference_);

  ESP_LOGCONFIG(TAG,
                "Config for ESP32 Touch Hub:\n"
                "  Measurement interval: %.1fus\n"
                "  Low Voltage Reference: %s\n"
                "  High Voltage Reference: %s",
                this->meas_interval_us_, lv_s, hv_s);
#else
  ESP_LOGCONFIG(TAG,
                "Config for ESP32 Touch Hub:\n"
                "  Measurement interval: %.1fus",
                this->meas_interval_us_);
#endif

#ifdef USE_ESP32_VARIANT_ESP32
  if (this->iir_filter_enabled_()) {
    ESP_LOGCONFIG(TAG, "  IIR Filter: %" PRIu32 "ms", this->iir_filter_);
  } else {
    ESP_LOGCONFIG(TAG, "  IIR Filter: 10ms (default)");
  }
#else
  if (this->filter_configured_) {
    // TOUCH_BM_IIR_FILTER_256 only exists on V2, shifting JITTER's position
    static constexpr const char *FILTER_STRS[] = {
      "IIR_4",
      "IIR_8",
      "IIR_16",
      "IIR_32",
      "IIR_64",
      "IIR_128",
#if SOC_TOUCH_SENSOR_VERSION == 2
      "IIR_256",
#endif
      "JITTER",
    };
    static constexpr const char *SMOOTH_STRS[] = {"OFF", "IIR_2", "IIR_4", "IIR_8"};
    const char *filter_s = lookup_str(FILTER_STRS, this->filter_mode_);
    const char *smooth_s = lookup_str(SMOOTH_STRS, this->smooth_level_);
    ESP_LOGCONFIG(TAG,
                  "  Filter mode: %s\n"
                  "  Debounce count: %" PRIu32 "\n"
                  "  Noise threshold coefficient: %" PRIu32 "\n"
                  "  Jitter filter step size: %" PRIu32 "\n"
                  "  Smooth level: %s",
                  filter_s, this->debounce_count_, this->noise_threshold_, this->jitter_step_, smooth_s);
  }

#if SOC_TOUCH_SUPPORT_DENOISE_CHAN
  if (this->denoise_configured_) {
    static constexpr const char *GRADE_STRS[] = {"BIT12", "BIT10", "BIT8", "BIT4"};
    static constexpr const char *CAP_STRS[] = {"5pF", "6.4pF", "7.8pF", "9.2pF", "10.6pF", "12pF", "13.4pF", "14.8pF"};
    const char *grade_s = lookup_str(GRADE_STRS, this->denoise_grade_);
    const char *cap_s = lookup_str(CAP_STRS, this->denoise_cap_level_);
    ESP_LOGCONFIG(TAG,
                  "  Denoise grade: %s\n"
                  "  Denoise capacitance level: %s",
                  grade_s, cap_s);
  }
#endif
#endif  // !USE_ESP32_VARIANT_ESP32

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED");
  }

  for (auto *child : this->children_) {
    LOG_BINARY_SENSOR("  ", "Touch Pad", child);
    ESP_LOGCONFIG(TAG,
                  "    Channel: %d\n"
                  "    Threshold: %" PRIu32 "\n"
                  "    Benchmark: %" PRIu32,
                  child->channel_id_, child->threshold_, child->benchmark_);
  }
}

void ESP32TouchComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // In setup mode, periodically log all pad values
  this->process_setup_mode_logging_(now);

  // Process queued touch events from callbacks
  TouchEvent event;
  while (xQueueReceive(this->touch_queue_, &event, 0) == pdTRUE) {
    for (auto *child : this->children_) {
      if (child->get_channel_id() != event.chan_id) {
        continue;
      }

      // Read current smooth value
      uint32_t value = 0;
      touch_channel_read_data(child->chan_handle_, TOUCH_CHAN_DATA_TYPE_SMOOTH, &value);
      child->value_ = value;

#ifndef USE_ESP32_VARIANT_ESP32
      // V2/V3: also read benchmark
      uint32_t benchmark = 0;
      touch_channel_read_data(child->chan_handle_, TOUCH_CHAN_DATA_TYPE_BENCHMARK, &benchmark);
      child->benchmark_ = benchmark;
#endif

      bool new_state = event.is_active;

      if (new_state != child->last_state_) {
        child->initial_state_published_ = true;
        child->last_state_ = new_state;
        child->publish_state(new_state);
#ifdef USE_ESP32_VARIANT_ESP32
        ESP_LOGV(TAG, "Touch Pad '%s' state: %s (value: %" PRIu32 ", threshold: %" PRIu32 ")",
                 child->get_name().c_str(), ONOFF(new_state), value, child->get_threshold());
#else
        if (new_state) {
          ESP_LOGV(TAG, "Touch Pad '%s' state: ON (value: %" PRIu32 ", benchmark: %" PRIu32 ", threshold: %" PRIu32 ")",
                   child->get_name().c_str(), value, benchmark, child->get_threshold());
        } else {
          ESP_LOGV(TAG, "Touch Pad '%s' state: OFF", child->get_name().c_str());
        }
#endif
      }
      break;
    }
  }

  // Publish initial OFF state for sensors that haven't received events yet
  bool all_initial_published = true;
  for (auto *child : this->children_) {
    this->publish_initial_state_if_needed_(child, now);
    if (!child->initial_state_published_) {
      all_initial_published = false;
    }
  }

  // Only disable loop once all initial states are published
  if (!this->setup_mode_ && all_initial_published) {
    this->disable_loop();
  }
}

void ESP32TouchComponent::on_shutdown() {
  if (this->sens_handle_ == nullptr)
    return;

  touch_sensor_stop_continuous_scanning(this->sens_handle_);
  touch_sensor_disable(this->sens_handle_);

  for (auto *child : this->children_) {
    if (child->chan_handle_ != nullptr) {
      touch_sensor_del_channel(child->chan_handle_);
      child->chan_handle_ = nullptr;
    }
  }

  touch_sensor_del_controller(this->sens_handle_);
  this->sens_handle_ = nullptr;

  this->cleanup_touch_queue_();
}

bool ESP32TouchComponent::create_touch_queue_() {
  size_t queue_size = this->children_.size() * 4;
  if (queue_size < 8)
    queue_size = 8;

  this->touch_queue_ = xQueueCreate(queue_size, sizeof(TouchEvent));

  if (this->touch_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create touch event queue of size %" PRIu32, (uint32_t) queue_size);
    this->mark_failed();
    return false;
  }
  return true;
}

void ESP32TouchComponent::cleanup_touch_queue_() {
  if (this->touch_queue_) {
    vQueueDelete(this->touch_queue_);
    this->touch_queue_ = nullptr;
  }
}

void ESP32TouchComponent::configure_wakeup_pads_() {
#if SOC_TOUCH_SUPPORT_SLEEP_WAKEUP
  bool has_wakeup = false;
  for (auto *child : this->children_) {
    if (child->get_wakeup_threshold() != 0) {
      has_wakeup = true;
      break;
    }
  }

  if (!has_wakeup)
    return;

#ifdef USE_ESP32_VARIANT_ESP32
  // V1: Simple sleep config - threshold is set via channel config's abs_active_thresh
  touch_sleep_config_t sleep_cfg = TOUCH_SENSOR_DEFAULT_DSLP_CONFIG();
  sleep_cfg.deep_slp_sens_cfg = nullptr;
  esp_err_t err = touch_sensor_config_sleep_wakeup(this->sens_handle_, &sleep_cfg);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to configure touch sleep wakeup: %s", esp_err_to_name(err));
  }
#else
  // V2/V3: Need to specify a deep sleep channel and threshold
  touch_channel_handle_t wakeup_chan = nullptr;
  uint32_t wakeup_thresh = 0;
  for (auto *child : this->children_) {
    if (child->get_wakeup_threshold() != 0) {
      wakeup_chan = child->chan_handle_;
      wakeup_thresh = child->get_wakeup_threshold();
      break;  // Only one deep sleep wakeup channel is supported
    }
  }

  if (wakeup_chan != nullptr) {
    touch_sleep_config_t sleep_cfg = TOUCH_SENSOR_DEFAULT_DSLP_CONFIG();
    sleep_cfg.deep_slp_chan = wakeup_chan;
    sleep_cfg.deep_slp_thresh[0] = wakeup_thresh;
    sleep_cfg.deep_slp_sens_cfg = nullptr;
    esp_err_t err = touch_sensor_config_sleep_wakeup(this->sens_handle_, &sleep_cfg);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to configure touch sleep wakeup: %s", esp_err_to_name(err));
    }
  }
#endif
#endif  // SOC_TOUCH_SUPPORT_SLEEP_WAKEUP
}

void ESP32TouchComponent::process_setup_mode_logging_(uint32_t now) {
  if (this->setup_mode_ && now - this->setup_mode_last_log_print_ > SETUP_MODE_LOG_INTERVAL_MS) {
    for (auto *child : this->children_) {
      if (child->chan_handle_ == nullptr)
        continue;

      uint32_t smooth_value = 0;
      touch_channel_read_data(child->chan_handle_, TOUCH_CHAN_DATA_TYPE_SMOOTH, &smooth_value);
      child->value_ = smooth_value;

#ifdef USE_ESP32_VARIANT_ESP32
      ESP_LOGD(TAG, "Touch Pad '%s' (Ch%d): %" PRIu32, child->get_name().c_str(), child->channel_id_, smooth_value);
#else
      uint32_t benchmark = 0;
      touch_channel_read_data(child->chan_handle_, TOUCH_CHAN_DATA_TYPE_BENCHMARK, &benchmark);
      child->benchmark_ = benchmark;
      int32_t difference = static_cast<int32_t>(smooth_value) - static_cast<int32_t>(benchmark);
      ESP_LOGD(TAG,
               "Touch Pad '%s' (Ch%d): value=%" PRIu32 ", benchmark=%" PRIu32 ", difference=%" PRId32
               " (set threshold < %" PRId32 " to detect touch)",
               child->get_name().c_str(), child->channel_id_, smooth_value, benchmark, difference, difference);
#endif
    }
    this->setup_mode_last_log_print_ = now;
  }
}

void ESP32TouchComponent::publish_initial_state_if_needed_(ESP32TouchBinarySensor *child, uint32_t now) {
  if (!child->initial_state_published_) {
    if (now > INITIAL_STATE_DELAY_MS) {
      child->publish_initial_state(false);
      child->initial_state_published_ = true;
      ESP_LOGV(TAG, "Touch Pad '%s' state: OFF (initial)", child->get_name().c_str());
    }
  }
}

}  // namespace esphome::esp32_touch

#endif  // USE_ESP32
