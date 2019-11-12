#include "esp32_touch.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_touch {

static const char *TAG = "esp32_touch";

void ESP32TouchComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 Touch Hub...");
  touch_pad_init();

  if (this->iir_filter_enabled_()) {
    touch_pad_filter_start(this->iir_filter_);
  }

  touch_pad_set_meas_time(this->sleep_cycle_, this->meas_cycle_);
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);

  for (auto *child : this->children_) {
    // Disable interrupt threshold
    touch_pad_config(child->get_touch_pad(), 0);
  }
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

  if (this->iir_filter_enabled_()) {
    ESP_LOGCONFIG(TAG, "    IIR Filter: %ums", this->iir_filter_);
    touch_pad_filter_start(this->iir_filter_);
  } else {
    ESP_LOGCONFIG(TAG, "  IIR Filter DISABLED");
  }
  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED!");
  }

  for (auto *child : this->children_) {
    LOG_BINARY_SENSOR("  ", "Touch Pad", child);
    ESP_LOGCONFIG(TAG, "    Pad: T%d", child->get_touch_pad());
    ESP_LOGCONFIG(TAG, "    Threshold: %u", child->get_threshold());
    if (child->get_sample_size() > 0) {
      ESP_LOGCONFIG(TAG, "    Adaptive threshold:");
      ESP_LOGCONFIG(TAG, "      Offset: %u", child->get_offset());
      ESP_LOGCONFIG(TAG, "      Interval: %us", child->get_interval() / 1000);
      ESP_LOGCONFIG(TAG, "      Samples: %u", child->get_sample_size());
    }
  }
}

void ESP32TouchComponent::loop() {
  const uint32_t now = millis();
  bool should_print = this->setup_mode_ && now - this->setup_mode_last_log_print_ > 250;
  for (auto *child : this->children_) {
    uint16_t value;
    if (this->iir_filter_enabled_()) {
      touch_pad_read_filtered(child->get_touch_pad(), &value);
    } else {
      touch_pad_read(child->get_touch_pad(), &value);
    }

    child->value_ = value;
    child->publish_state(value < child->get_threshold());

    if (should_print) {
      ESP_LOGD(TAG, "Touch Pad '%s' (T%u): %u", child->get_name().c_str(), child->get_touch_pad(), value);
    }

    if (child->get_sample_size() > 0 && now - child->get_last_run() > child->get_interval()) {
      if (child->get_count() < child->get_sample_size()) {
        child->add_sample(value);
        child->increment_count();
      } else {
        value = child->find_most_freq();
        // Check underflow
        if (value < child->get_offset()) {
          child->set_threshold(0);
        } else {
          child->set_threshold(value - child->get_offset());
        }
        if (this->setup_mode_) {
          ESP_LOGD(TAG, "New threshold set: %u", child->get_threshold());
        }
        child->clear_samples();
        child->reset_count();
      }
      child->set_last_run(now);
    }
  }

  if (should_print) {
    // Avoid spamming logs
    this->setup_mode_last_log_print_ = now;
  }
}

void ESP32TouchComponent::on_shutdown() {
  if (this->iir_filter_enabled_()) {
    touch_pad_filter_stop();
    touch_pad_filter_delete();
  }
  touch_pad_deinit();
}

ESP32TouchBinarySensor::ESP32TouchBinarySensor(const std::string &name, touch_pad_t touch_pad, uint16_t threshold)
    : BinarySensor(name), touch_pad_(touch_pad), threshold_(threshold) {}
ESP32TouchBinarySensor::ESP32TouchBinarySensor(const std::string &name, touch_pad_t touch_pad, uint16_t offset,
                                               uint16_t interval, uint16_t sample_size)
    : BinarySensor(name),
      touch_pad_(touch_pad),
      offset_(offset),
      interval_(interval * 1000),
      sample_size_(sample_size) {}

uint16_t ESP32TouchBinarySensor::find_most_freq() {
  uint16_t max_count = 0, most_freq = 0;
  for (auto i : this->samples_) {
    if (max_count < i.second) {
      most_freq = i.first;
      max_count = i.second;
    }
  }
  return most_freq;
}

}  // namespace esp32_touch
}  // namespace esphome

#endif
