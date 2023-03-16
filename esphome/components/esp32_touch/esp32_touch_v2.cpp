#ifdef USE_ESP32

#include "esp32_touch.h"

#if SOC_TOUCH_VERSION_2

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

void ESP32TouchComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 Touch Hub...");
  touch_pad_init();

  for (auto *child : this->children_) {
    // Disable interrupt threshold
    touch_pad_config(child->get_touch_pad());
  }

  touch_pad_set_meas_time(this->sleep_cycle_, this->meas_cycle_);
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);
  touch_pad_set_idle_channel_connect(TOUCH_PAD_IDLE_CH_CONNECT_DEFAULT);

  for (auto *child : this->children_) {
    touch_pad_set_cnt_mode(child->get_touch_pad(), TOUCH_PAD_SLOPE_DEFAULT, TOUCH_PAD_TIE_OPT_DEFAULT);
  }

  touch_pad_denoise_t denoise = {
      .grade = TOUCH_PAD_DENOISE_BIT4,
      .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
  };
  touch_pad_denoise_set_config(&denoise);
  touch_pad_denoise_enable();

  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  touch_pad_fsm_start();
}

void ESP32TouchComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Config for ESP32 Touch Hub:");
  ESP_LOGCONFIG(TAG, "  Meas cycle: %.2fms", this->meas_cycle_ / (8000000.0f / 1000.0f));
  ESP_LOGCONFIG(TAG, "  Sleep cycle: %.2fms", this->sleep_cycle_ / (150000.0f / 1000.0f));

  const char *lv_s;
  switch (this->low_voltage_reference_) {
    case TOUCH_LVOLT_0V5:
      lv_s = "0.5V";
      break;
    case TOUCH_LVOLT_0V6:
      lv_s = "0.6V";
      break;
    case TOUCH_LVOLT_0V7:
      lv_s = "0.7V";
      break;
    case TOUCH_LVOLT_0V8:
      lv_s = "0.8V";
      break;
    default:
      lv_s = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Low Voltage Reference: %s", lv_s);

  const char *hv_s;
  switch (this->high_voltage_reference_) {
    case TOUCH_HVOLT_2V4:
      hv_s = "2.4V";
      break;
    case TOUCH_HVOLT_2V5:
      hv_s = "2.5V";
      break;
    case TOUCH_HVOLT_2V6:
      hv_s = "2.6V";
      break;
    case TOUCH_HVOLT_2V7:
      hv_s = "2.7V";
      break;
    default:
      hv_s = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  High Voltage Reference: %s", hv_s);

  const char *atten_s;
  switch (this->voltage_attenuation_) {
    case TOUCH_HVOLT_ATTEN_1V5:
      atten_s = "1.5V";
      break;
    case TOUCH_HVOLT_ATTEN_1V:
      atten_s = "1V";
      break;
    case TOUCH_HVOLT_ATTEN_0V5:
      atten_s = "0.5V";
      break;
    case TOUCH_HVOLT_ATTEN_0V:
      atten_s = "0V";
      break;
    default:
      atten_s = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Voltage Attenuation: %s", atten_s);

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED!");
  }

  for (auto *child : this->children_) {
    LOG_BINARY_SENSOR("  ", "Touch Pad", child);
    ESP_LOGCONFIG(TAG, "    Pad: T%d", child->get_touch_pad());
    ESP_LOGCONFIG(TAG, "    Threshold: %u", child->get_threshold());
  }
}

void ESP32TouchComponent::loop() {
  const uint32_t now = millis();
  bool should_print = this->setup_mode_ && now - this->setup_mode_last_log_print_ > 250;
  for (auto *child : this->children_) {
    uint32_t value;

    touch_pad_read_raw_data(child->get_touch_pad(), &value);

    child->value_ = value;
    child->publish_state(value < child->get_threshold());

    if (should_print) {
      ESP_LOGD(TAG, "Touch Pad '%s' (T%u): %u", child->get_name().c_str(), child->get_touch_pad(), value);
    }

    App.feed_wdt();
  }

  if (should_print) {
    // Avoid spamming logs
    this->setup_mode_last_log_print_ = now;
  }
}

void ESP32TouchComponent::on_shutdown() {
  bool is_wakeup_source = false;

  for (auto *child : this->children_) {
    if (child->get_wakeup_threshold() != 0) {
      if (is_wakeup_source) {
        ESP_LOGW(TAG, "Only a single touch pad can be used to wakeup. Ignoring %u", child->get_touch_pad());
        continue;
      }
      is_wakeup_source = true;
      // Touch sensor FSM mode must be 'TOUCH_FSM_MODE_TIMER' to use it to wake-up.
      touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

      touch_pad_sleep_channel_enable(child->get_touch_pad(), true);
      touch_pad_sleep_set_threshold(child->get_touch_pad(), child->get_wakeup_threshold());
    }
  }

  if (!is_wakeup_source) {
    touch_pad_deinit();
  }
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // SOC_TOUCH_VERSION_2
#endif  // USE_ESP32
