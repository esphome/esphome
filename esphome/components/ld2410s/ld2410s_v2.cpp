#include "ld2410s.h"

#include <cinttypes>
#include <cstring>

namespace esphome::ld2410s {

#ifdef LD2410S_V2

// PUBLIC

void LD2410S::dump_config() {
#ifdef USE_BUTTON
  ESP_LOGCONFIG(TAG, "Buttons:");
  LOG_BUTTON("  ", "Factory reset", this->factory_reset_button_);
  LOG_BUTTON("  ", "Start calibration", this->calibration_button_);
#endif

#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "Switches:");
  LOG_SWITCH("  ", "Minimal Output", this->minimal_output_switch_);
#endif

  ESP_LOGCONFIG(TAG,
                "Config Params Read: \n  max_dist:%d \n  min_dist:%d \n  delay:%d \n  status_freq:%d \n  dist_freq:%d "
                "\n  resp_speed:%d",
                this->max_dist_, this->min_dist_, this->delay_, this->status_freq_, this->dist_freq_,
                this->resp_speed_);
}

void LD2410S::init_() {
  ESP_LOGI(TAG, "init");
  // App.feed_wdt();

  this->init_done_ = false;

  this->minimal_output_ = true;

  this->read_all_();
}
void LD2410S::read_all_() {
  this->tx_schedule_.append(OUTPUT_MODE_SWITCH_CMD);
  this->tx_schedule_.append(CFG_FW_READ_CMD);
  this->tx_schedule_.append(CFG_PARAMS_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_TRIGGER_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_HOLD_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_SNR_READ_CMD);
}

// button
void LD2410S::calibration() { this->tx_schedule_.append(CALIBRATION_CMD); }
void LD2410S::factory_reset() {
  ESP_LOGI(TAG, "factory_reset");

  this->max_dist_ = 16;
  this->min_dist_ = 0;
  this->delay_ = 10;
  this->status_freq_ = 80;
  this->dist_freq_ = 80;

  this->minimal_output_ = true;

  this->resp_speed_ = 5;

  for (uint8_t i = 0; i < 16; i++) {
    this->thresholds_trigger_[i] = CFG_GATE_THRESHOLD_TRIGGER_WRITE_DATA[i];
    this->thresholds_hold_[i] = CFG_GATE_THRESHOLD_HOLD_WRITE_DATA[i];
    this->thresholds_snr_[i] = CFG_GATE_THRESHOLD_SNR_WRITE_DATA[i];
  }

  this->tx_schedule_.append(OUTPUT_MODE_SWITCH_CMD);

  this->tx_schedule_.append(CFG_PARAMS_WRITE_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_TRIGGER_WRITE_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_HOLD_WRITE_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_SNR_WRITE_CMD);

  this->tx_schedule_.append(CFG_PARAMS_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_TRIGGER_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_HOLD_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_SNR_READ_CMD);
}
// number
void LD2410S::set_delay(float delay) {
  this->delay_ = delay;
  this->tx_schedule_.append(CFG_PARAMS_WRITE_CMD, CFG_NO_DELAY_VALUE);
  this->no_delay_number_->publish_state(this->delay_);
}
void LD2410S::set_distance_reporting_freq(float distance_reporting_freq) {
  this->dist_freq_ = distance_reporting_freq * 10;
  this->tx_schedule_.append(CFG_PARAMS_WRITE_CMD, CFG_DISTANCE_FREQ_VALUE);
  this->distance_reporting_freq_number_->publish_state(static_cast<float>(this->dist_freq_) / 10);
}
void LD2410S::set_max_distance(float max_distance) {
  this->max_dist_ = static_cast<float>(max_distance) / 0.7f;
  this->tx_schedule_.append(CFG_PARAMS_WRITE_CMD, CFG_MAX_DETECTION_VALUE);
  this->max_distance_number_->publish_state(static_cast<float>(this->max_dist_) * 0.7);
}
void LD2410S::set_min_distance(float min_distance) {
  this->min_dist_ = static_cast<float>(min_distance) / 0.7f;
  this->tx_schedule_.append(CFG_PARAMS_WRITE_CMD, CFG_MIN_DETECTION_VALUE);
  this->min_distance_number_->publish_state(static_cast<float>(this->min_dist_) * 0.7);
}
void LD2410S::set_status_reporting_freq(float status_reporting_freq) {
  this->status_freq_ = status_reporting_freq * 10;
  this->tx_schedule_.append(CFG_PARAMS_WRITE_CMD, CFG_STATUS_FREQ_VALUE);
  this->status_reporting_freq_number_->publish_state(static_cast<float>(this->status_freq_) / 10);
}
void LD2410S::set_threshold_hold(float threshold_hold) {
  this->thresholds_hold_[this->thresholds_selected_gate_] = threshold_hold;
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_HOLD_WRITE_CMD, this->thresholds_selected_gate_);
  this->threshold_hold_number_->publish_state(this->thresholds_hold_[this->thresholds_selected_gate_]);
  this->publish_threshold_hold_();
}
void LD2410S::set_threshold_selected_gate(float threshold_selected_gate) {
  if (threshold_selected_gate < 0 || threshold_selected_gate >= 16)
    return;
  this->thresholds_selected_gate_ = static_cast<uint8_t>(threshold_selected_gate);
#ifdef USE_NUMBER
  this->threshold_selected_gate_number_->publish_state(this->thresholds_selected_gate_);
  this->threshold_trigger_number_->publish_state(this->thresholds_trigger_[this->thresholds_selected_gate_]);
  this->threshold_hold_number_->publish_state(this->thresholds_hold_[this->thresholds_selected_gate_]);
  this->threshold_snr_number_->publish_state(this->thresholds_snr_[this->thresholds_selected_gate_]);
#endif
}
void LD2410S::set_threshold_snr(float threshold_snr) {
  this->thresholds_snr_[this->thresholds_selected_gate_] = threshold_snr;
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_SNR_WRITE_CMD, this->thresholds_selected_gate_);
  this->threshold_snr_number_->publish_state(this->thresholds_snr_[this->thresholds_selected_gate_]);
  this->publish_threshold_snr_();
}
void LD2410S::set_threshold_trigger(float threshold_trigger) {
  this->thresholds_trigger_[this->thresholds_selected_gate_] = threshold_trigger;
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_TRIGGER_WRITE_CMD, this->thresholds_selected_gate_);
  this->threshold_trigger_number_->publish_state(this->thresholds_trigger_[this->thresholds_selected_gate_]);
  this->publish_threshold_trigger_();
}
// select
void LD2410S::set_response_speed_select(const std::string &response_speed_select) {
  this->resp_speed_ = response_speed_select == CFG_RESPONSE_SPEED_NORMAL ? 5 : 10;
  this->tx_schedule_.append(CFG_PARAMS_WRITE_CMD, CFG_RESPONSE_SPEED_VALUE);
#ifdef USE_SELECT
  this->response_speed_select_->publish_state(this->resp_speed_ == 5 ? CFG_RESPONSE_SPEED_NORMAL
                                                                     : CFG_RESPONSE_SPEED_FAST);
#endif
}
// switch
void LD2410S::set_minimal_output(bool state) {
  this->minimal_output_ = state;
  if (!state) {
    for (auto &energy_value : this->energy_values_) {
      energy_value = 0;
    }
  }
  this->tx_schedule_.append(OUTPUT_MODE_SWITCH_CMD);
}

// PROTECTED
void LD2410S::read_all_thresholds_() {
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_TRIGGER_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_HOLD_READ_CMD);
  this->tx_schedule_.append(CFG_GATE_THRESHOLD_SNR_READ_CMD);
}

void LD2410S::parse_data_energy_values_read_(uint8_t *data) {
  uint16_t read_position = 0;

  for (uint32_t &energy_value : this->energy_values_) {
    uint32_t val = 0;
    read_seq_data(data, read_position, &val);

    uint32_t db = 0;
    if (val > 0) {
      db = 10 * log10(val);
    }
    if (db > energy_value) {
      energy_value = db;
    }
  }
  this->publish_energy_values_();
}

void LD2410S::parse_ack_config_start_(const uint8_t *data) {
  uint16_t read_position = 0;
  uint16_t protocol_version = 0;
  uint16_t buffer_size = 0;
  read_seq_data(data, read_position, &protocol_version);  // does not exist in both documents
  read_seq_data(data, read_position, &buffer_size);

  ESP_LOGD(TAG, "CONFIG MODE ENABLED, protocol_version:%d  buffer_size:%d", protocol_version, buffer_size);
}
void LD2410S::parse_ack_config_end_(const uint8_t *data) { ESP_LOGD(TAG, "CONFIG MODE DISABLED"); }
void LD2410S::parse_ack_minimal_output_(uint8_t *data) {
  if (this->minimal_output_) {
    ESP_LOGI(TAG, "Minimal Output Mode switched ON");
  } else {
    ESP_LOGI(TAG, "Minimal Output Mode switched OFF");
  }
#ifdef USE_SWITCH
  this->minimal_output_switch_->publish_state(this->minimal_output_);
#endif
}
void LD2410S::parse_ack_config_read_(uint8_t *data) {
  uint16_t read_position = 0;
  read_seq_data(data, read_position, &this->max_dist_);
  read_seq_data(data, read_position, &this->min_dist_);
  read_seq_data(data, read_position, &this->delay_);
  read_seq_data(data, read_position, &this->status_freq_);
  read_seq_data(data, read_position, &this->dist_freq_);
  read_seq_data(data, read_position, &this->resp_speed_);

#ifdef USE_NUMBER
  this->max_distance_number_->publish_state(static_cast<float>(this->max_dist_) * 0.7);
  this->min_distance_number_->publish_state(static_cast<float>(this->min_dist_) * 0.7);
  this->no_delay_number_->publish_state(this->delay_);
  this->status_reporting_freq_number_->publish_state(static_cast<float>(this->status_freq_) / 10);
  this->distance_reporting_freq_number_->publish_state(static_cast<float>(this->dist_freq_) / 10);
#endif
#ifdef USE_SELECT
  this->response_speed_select_->publish_state(this->resp_speed_ == 5 ? CFG_RESPONSE_SPEED_NORMAL
                                                                     : CFG_RESPONSE_SPEED_FAST);
#endif
}
void LD2410S::parse_ack_fw_read_(const uint8_t *data) {
  uint16_t read_position = 0;
  uint32_t equipment_type = 0;
  uint16_t major_v = 0;
  uint16_t minor_v = 0;
  uint16_t patch_v = 0;
  read_seq_data(data, read_position, &equipment_type);  // does not exist in both documents
  read_seq_data(data, read_position, &major_v);
  read_seq_data(data, read_position, &minor_v);
  read_seq_data(data, read_position, &patch_v);
  char version[20];
  snprintf(version, sizeof(version), "v%u.%u.%u", major_v, minor_v, patch_v);

  this->publish_fw_version_(version);
}
void LD2410S::parse_ack_threshold_trigger_read_(uint8_t *data) {
  uint16_t read_position = 0;
  read_seq_data(data, read_position, &this->thresholds_trigger_, 16, 4);

#ifdef USE_NUMBER
  this->threshold_trigger_number_->publish_state(this->thresholds_trigger_[this->thresholds_selected_gate_]);
#endif

  this->publish_threshold_trigger_();
}
void LD2410S::parse_ack_threshold_hold_read_(uint8_t *data) {
  uint16_t read_position = 0;
  read_seq_data(data, read_position, &this->thresholds_hold_, 16, 4);
#ifdef USE_NUMBER
  this->threshold_hold_number_->publish_state(this->thresholds_hold_[this->thresholds_selected_gate_]);
#endif

  this->publish_threshold_hold_();
}
void LD2410S::parse_ack_threshold_snr_read_(uint8_t *data) {
  uint16_t read_position = 0;
  read_seq_data(data, read_position, &this->thresholds_snr_, 16, 4);
#ifdef USE_NUMBER
  this->threshold_snr_number_->publish_state(this->thresholds_snr_[this->thresholds_selected_gate_]);
#endif

  this->publish_threshold_snr_();
}

void LD2410S::publish_distance_(uint16_t distance, bool force_publish) {
#ifdef USE_SENSOR
  if (this->distance_sensor_ != nullptr) {
    if (this->distance_sensor_->state != distance || force_publish) {
      this->distance_sensor_->publish_state(distance);
    }
  }
#endif
}
void LD2410S::publish_presence_(bool presence, bool force_publish) {
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    if (this->presence_binary_sensor_->state != presence || force_publish) {
      this->presence_binary_sensor_->publish_state(presence);
    }
  }
#endif
}
void LD2410S::publish_calibration_progress_(uint16_t calibration_progress, bool force_publish) {
#ifdef USE_SENSOR
  if (this->calibration_progress_sensor_ != nullptr) {
    if (calibration_progress == 100) {
      if (this->calibration_progress_sensor_->state != 0 || force_publish) {
        this->calibration_progress_sensor_->publish_state(0);
      }
    } else {
      if (this->calibration_progress_sensor_->state != calibration_progress || force_publish) {
        this->calibration_progress_sensor_->publish_state(calibration_progress);
      }
    }
  }
#endif
}
void LD2410S::publish_calibration_running_(bool running, bool force_publish) {
#ifdef USE_BINARY_SENSOR
  if (this->calibration_running_binary_sensor_ != nullptr) {
    if (this->calibration_running_binary_sensor_->state != running || force_publish) {
      this->calibration_running_binary_sensor_->publish_state(running);
    }
  }
#endif
}
void LD2410S::publish_fw_version_(const std::string &version, bool force_publish) {
#ifdef USE_TEXT_SENSOR
  if (this->fw_version_text_sensor_ != nullptr) {
    if (this->fw_version_text_sensor_->state != version || force_publish) {
      this->fw_version_text_sensor_->publish_state(version);
    }
  }
#endif
}
void LD2410S::publish_threshold_trigger_(bool force_publish) {
  std::string vals = format_int(this->thresholds_trigger_, 16, 2);

#ifdef USE_TEXT_SENSOR
  if (this->threshold_trigger_text_sensor_ != nullptr) {
    if (this->threshold_trigger_text_sensor_->state != vals || force_publish) {
      this->threshold_trigger_text_sensor_->publish_state(vals);
    }
  }
#endif
}
void LD2410S::publish_threshold_hold_(bool force_publish) {
  std::string vals = format_int(this->thresholds_hold_, 16, 2);

#ifdef USE_TEXT_SENSOR
  if (this->threshold_hold_text_sensor_ != nullptr) {
    if (this->threshold_hold_text_sensor_->state != vals || force_publish) {
      this->threshold_hold_text_sensor_->publish_state(vals);
    }
  }
#endif
}
void LD2410S::publish_threshold_snr_(bool force_publish) {
  std::string vals = format_int(this->thresholds_snr_, 16, 2);

#ifdef USE_TEXT_SENSOR
  if (this->threshold_snr_text_sensor_ != nullptr) {
    if (this->threshold_snr_text_sensor_->state != vals || force_publish) {
      this->threshold_snr_text_sensor_->publish_state(vals);
    }
  }
#endif
}
void LD2410S::publish_energy_values_(bool force_publish) {
  this->energy_values_str_ = format_int(this->energy_values_, 16, 2);

#ifdef USE_TEXT_SENSOR
  if (this->energy_values_text_sensor_ != nullptr) {
    if (this->energy_values_text_sensor_->state != this->energy_values_str_ || force_publish) {
      this->energy_values_text_sensor_->publish_state(this->energy_values_str_);
    }
  }
#endif
}

std::string LD2410S::format_int(uint32_t *in, uint8_t len, uint8_t min_w) {
  if (len == 0)
    return "";

  std::string result;
  int sum = 0;
  for (uint8_t i = 0; i < len; ++i) {
    sum += in[i];

    if (i > 0)
      result += ',';

    char num[12];
    snprintf(num, sizeof(num), "%" PRIu32, in[i]);
    size_t num_len = strlen(num);

    if (num_len < min_w)
      result += std::string(min_w - num_len, '0');

    result += num;
  }

  if (sum == 0) {
    result = "";
  }

  return result;
}

#endif

}  // namespace esphome::ld2410s
