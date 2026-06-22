#include "bme690.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome::bme690 {

bool BME690Component::check_result_(const char *label, int8_t rslt) {
  if (rslt == BME69X_OK) {
    return true;
  }

  ESP_LOGW(TAG, "%s failed: %d", label, rslt);
  return false;
}

bool BME690Component::check_bsec_status_(const char *label, bsec_library_return_t rslt) {
  if (rslt == BSEC_OK) {
    return true;
  }

  if (rslt > BSEC_OK) {
    ESP_LOGW(TAG, "%s warning: %d", label, static_cast<int>(rslt));
    return true;
  }

  ESP_LOGE(TAG, "%s failed: %d", label, static_cast<int>(rslt));
  return false;
}

BME69X_INTF_RET_TYPE BME690Component::read_i2c(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
  auto *self = static_cast<BME690Component *>(intf_ptr);
  if (self == nullptr) {
    return -1;
  }

  auto err = self->read_register(reg_addr, reg_data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read error %d on reg 0x%02X", static_cast<int>(err), reg_addr);
    return -1;
  }

  return BME69X_INTF_RET_SUCCESS;
}

BME69X_INTF_RET_TYPE BME690Component::write_i2c(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len,
                                                void *intf_ptr) {
  auto *self = static_cast<BME690Component *>(intf_ptr);
  if (self == nullptr) {
    return -1;
  }

  auto err = self->write_register(reg_addr, reg_data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write error %d on reg 0x%02X", static_cast<int>(err), reg_addr);
    return -1;
  }

  return BME69X_INTF_RET_SUCCESS;
}

void BME690Component::delay_usec(uint32_t period, void *) {
  // Driver requests microseconds; round up to the nearest millisecond.
  const uint32_t ms = (period + 999U) / 1000U;
  if (ms > 0) {
    delay(ms);
  }
}

void BME690Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BME690 sensor...");

  this->dev_.intf = BME69X_I2C_INTF;
  this->dev_.intf_ptr = this;
  this->dev_.read = BME690Component::read_i2c;
  this->dev_.write = BME690Component::write_i2c;
  this->dev_.delay_us = BME690Component::delay_usec;

  int8_t rslt = bme69x_init(&this->dev_);
  if (!this->check_result_("bme69x_init", rslt)) {
    this->mark_failed();
    return;
  }

  this->conf_.filter = BME69X_FILTER_OFF;
  this->conf_.odr = BME69X_ODR_NONE;
  this->conf_.os_hum = BME69X_OS_16X;
  this->conf_.os_pres = BME69X_OS_16X;
  this->conf_.os_temp = BME69X_OS_16X;
  rslt = bme69x_set_conf(&this->conf_, &this->dev_);
  if (!this->check_result_("bme69x_set_conf", rslt)) {
    this->mark_failed();
    return;
  }

  this->heatr_conf_.enable = BME69X_ENABLE;
  this->heatr_conf_.heatr_temp = 320;  // degC
  this->heatr_conf_.heatr_dur = 150;   // ms
  this->heatr_conf_.heatr_temp_prof = nullptr;
  this->heatr_conf_.heatr_dur_prof = nullptr;
  this->heatr_conf_.profile_len = 0;
  this->heatr_conf_.shared_heatr_dur = 0;
  rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &this->heatr_conf_, &this->dev_);
  if (!this->check_result_("bme69x_set_heatr_conf", rslt)) {
    this->mark_failed();
    return;
  }

  if (!this->configure_bsec_()) {
    ESP_LOGW(TAG, "BSEC configuration failed; running raw sensor only.");
  }
}

void BME690Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BME690");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor);
  LOG_SENSOR("  ", "Gas resistance", this->gas_resistance_sensor);
  LOG_SENSOR("  ", "IAQ", this->iaq_sensor);
  LOG_SENSOR("  ", "IAQ accuracy", this->iaq_accuracy_sensor);
  LOG_SENSOR("  ", "Static IAQ", this->static_iaq_sensor);
  LOG_SENSOR("  ", "CO2 equivalent", this->co2_equivalent_sensor);
  LOG_SENSOR("  ", "Breath VOC equivalent", this->breath_voc_equivalent_sensor);
  LOG_SENSOR("  ", "Gas percentage", this->gas_percentage_sensor);
  LOG_SENSOR("  ", "Compensated temperature", this->comp_temperature_sensor);
  LOG_SENSOR("  ", "Compensated humidity", this->comp_humidity_sensor);
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "IAQ Accuracy", this->iaq_accuracy_text_sensor_);
#endif
  ESP_LOGCONFIG(TAG, "  Sample Rate: %s", this->sample_rate_ == SAMPLE_RATE_ULP ? "ULP" : "LP");
  ESP_LOGCONFIG(TAG, "  Temperature Offset: %.2f°C", this->ext_temp_offset_);
  ESP_LOGCONFIG(TAG, "  State Save Interval: %" PRIu32 "ms", this->state_save_interval_ms_);
  LOG_UPDATE_INTERVAL(this);
}

void BME690Component::update() {
  if (this->status_has_error()) {
    return;
  }

  const int64_t timestamp_ns = this->get_time_ns_();

  bsec_bme_settings_t sensor_settings = {};
  if (this->bsec_ready_) {
    if (this->next_call_ns_ != 0 && timestamp_ns < this->next_call_ns_) {
      return;
    }
    auto bsec_rslt = bsec_sensor_control(this->bsec_instance_.data(), timestamp_ns, &sensor_settings);
    if (!this->check_bsec_status_("bsec_sensor_control", bsec_rslt)) {
      this->status_set_warning();
    }
    ESP_LOGI(TAG, "BSEC control: next_call=%lld, trig=%u, op_mode=%u, temp_os=%u hum_os=%u pres_os=%u run_gas=%u",
             static_cast<long long>(sensor_settings.next_call), sensor_settings.trigger_measurement,
             sensor_settings.op_mode, sensor_settings.temperature_oversampling, sensor_settings.humidity_oversampling,
             sensor_settings.pressure_oversampling, sensor_settings.run_gas);
    this->next_call_ns_ = sensor_settings.next_call;
  } else {
    if (!this->bsec_fallback_warning_logged_) {
      if (this->bsec_setup_failed_step_ != nullptr) {
        ESP_LOGW(TAG, "BSEC setup failed at %s: %d; publishing raw sensor values only.",
                 this->bsec_setup_failed_step_, static_cast<int>(this->bsec_setup_failed_result_));
      } else {
        ESP_LOGW(TAG, "BSEC is not ready; publishing raw sensor values only.");
      }
      this->bsec_fallback_warning_logged_ = true;
    }
    sensor_settings.op_mode = BME69X_FORCED_MODE;
    sensor_settings.trigger_measurement = 1;
    sensor_settings.temperature_oversampling = BME69X_OS_16X;
    sensor_settings.humidity_oversampling = BME69X_OS_16X;
    sensor_settings.pressure_oversampling = BME69X_OS_16X;
    sensor_settings.heater_temperature = this->heatr_conf_.heatr_temp;
    sensor_settings.heater_duration = this->heatr_conf_.heatr_dur;
    sensor_settings.run_gas = 1;
    sensor_settings.process_data =
        BSEC_PROCESS_TEMPERATURE | BSEC_PROCESS_HUMIDITY | BSEC_PROCESS_PRESSURE | BSEC_PROCESS_GAS;
  }

  if (sensor_settings.trigger_measurement == 0) {
    return;
  }

  this->conf_.filter = BME69X_FILTER_OFF;
  this->conf_.odr = BME69X_ODR_NONE;
  this->conf_.os_hum = sensor_settings.humidity_oversampling;
  this->conf_.os_pres = sensor_settings.pressure_oversampling;
  this->conf_.os_temp = sensor_settings.temperature_oversampling;

  int8_t rslt = bme69x_set_conf(&this->conf_, &this->dev_);
  if (!this->check_result_("bme69x_set_conf", rslt)) {
    this->status_set_warning();
    return;
  }

  this->heatr_conf_.enable = sensor_settings.run_gas ? BME69X_ENABLE : BME69X_DISABLE;
  this->heatr_conf_.heatr_temp = sensor_settings.heater_temperature;
  this->heatr_conf_.heatr_dur = sensor_settings.heater_duration;
  this->heatr_conf_.shared_heatr_dur = 0;
  this->heatr_conf_.heatr_temp_prof = nullptr;
  this->heatr_conf_.heatr_dur_prof = nullptr;
  this->heatr_conf_.profile_len = 0;

  if (sensor_settings.heater_profile_len > 0) {
    this->heatr_conf_.profile_len = sensor_settings.heater_profile_len;
    this->heatr_conf_.heatr_temp_prof = const_cast<uint16_t *>(sensor_settings.heater_temperature_profile);
    this->heatr_conf_.heatr_dur_prof = const_cast<uint16_t *>(sensor_settings.heater_duration_profile);
  }

  rslt = bme69x_set_heatr_conf(sensor_settings.op_mode, &this->heatr_conf_, &this->dev_);
  if (!this->check_result_("bme69x_set_heatr_conf", rslt)) {
    this->status_set_warning();
    return;
  }

  rslt = bme69x_set_op_mode(sensor_settings.op_mode, &this->dev_);
  if (!this->check_result_("bme69x_set_op_mode", rslt)) {
    this->status_set_warning();
    return;
  }

  uint32_t meas_dur_us = bme69x_get_meas_dur(sensor_settings.op_mode, &this->conf_, &this->dev_);
  if (this->heatr_conf_.profile_len > 0) {
    // Use the total heating duration if a profile is requested.
    uint32_t total = 0;
    for (uint8_t idx = 0; idx < this->heatr_conf_.profile_len; idx++) {
      total += static_cast<uint32_t>(this->heatr_conf_.heatr_dur_prof[idx]) * 1000U;
    }
    meas_dur_us += total;
  } else {
    meas_dur_us += static_cast<uint32_t>(this->heatr_conf_.heatr_dur) * 1000U;
  }
  delay_usec(meas_dur_us, this);

  struct bme69x_data data = {};
  uint8_t n_fields = 0;
  rslt = bme69x_get_data(sensor_settings.op_mode, &data, &n_fields, &this->dev_);
  if (!this->check_result_("bme69x_get_data", rslt)) {
    this->status_set_warning();
    return;
  }

  if (n_fields == 0) {
    ESP_LOGV(TAG, "No new measurement available");
    return;
  }

  if (this->temperature_sensor != nullptr) {
    this->temperature_sensor->publish_state(data.temperature);
  }
  if (this->pressure_sensor != nullptr) {
    // Driver returns pressure in Pa; convert to hPa to match common ESPHome convention.
    this->pressure_sensor->publish_state(data.pressure / 100.0f);
  }
  if (this->humidity_sensor != nullptr) {
    this->humidity_sensor->publish_state(data.humidity);
  }
  if (this->gas_resistance_sensor != nullptr) {
    this->gas_resistance_sensor->publish_state(data.gas_resistance);
  }

  if (this->bsec_ready_) {
    this->push_inputs_to_bsec_(data, sensor_settings, timestamp_ns);
  }
}

bool BME690Component::configure_bsec_() {
  size_t instance_size = bsec_get_instance_size();
  this->bsec_instance_.assign(instance_size, 0);
  this->bsec_work_buffer_.assign(BSEC_MAX_WORKBUFFER_SIZE, 0);

  auto bsec_rslt = bsec_init(this->bsec_instance_.data());
  if (!this->check_bsec_status_("bsec_init", bsec_rslt)) {
    this->bsec_setup_failed_step_ = "bsec_init";
    this->bsec_setup_failed_result_ = bsec_rslt;
    return false;
  }
  this->log_bsec_version_();

  bsec_rslt =
      bsec_set_configuration(this->bsec_instance_.data(), this->get_bsec_configuration_(),
                             this->get_bsec_configuration_length_(), this->bsec_work_buffer_.data(),
                             this->bsec_work_buffer_.size());
  if (!this->check_bsec_status_("bsec_set_configuration", bsec_rslt)) {
    this->bsec_setup_failed_step_ = "bsec_set_configuration";
    this->bsec_setup_failed_result_ = bsec_rslt;
    return false;
  }

  this->pref_ = global_preferences->make_preference<std::array<uint8_t, BSEC_MAX_STATE_BLOB_SIZE + 4>>(
      this->state_preference_hash_);
  this->load_bsec_state_();

  bsec_sensor_configuration_t requested_virtual_sensors[14] = {};
  uint8_t n_requested = 0;
  const float sample_rate = this->get_sample_rate_();
  const bool bsec_3_3_or_newer = this->is_bsec_3_3_or_newer_();

  auto add_request = [&](uint8_t sensor_id) {
    requested_virtual_sensors[n_requested].sensor_id = sensor_id;
    requested_virtual_sensors[n_requested].sample_rate = sample_rate;
    n_requested++;
  };

  // Always request the raw feeds.
  add_request(BSEC_OUTPUT_RAW_PRESSURE);
  add_request(BSEC_OUTPUT_RAW_TEMPERATURE);
  add_request(BSEC_OUTPUT_RAW_HUMIDITY);
  add_request(BSEC_OUTPUT_RAW_GAS);

  // IAQ-related outputs.
  add_request(BSEC_OUTPUT_IAQ);
  add_request(BSEC_OUTPUT_STATIC_IAQ);
  add_request(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE);
  add_request(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY);
  add_request(BSEC_OUTPUT_CO2_EQUIVALENT);
  if (bsec_3_3_or_newer) {
    // BSEC 3.3 disables BREATH_VOC_EQUIVALENT for BME690. TVOC is LP-only and is currently not exposed
    // because it uses ppb rather than the Breath VOC sensor's ppm unit.
    if (this->sample_rate_ == SAMPLE_RATE_LP) {
      add_request(BSEC_OUTPUT_TVOC_EQUIVALENT);
    }
  } else {
    add_request(BSEC_OUTPUT_BREATH_VOC_EQUIVALENT);
  }
  add_request(BSEC_OUTPUT_GAS_PERCENTAGE);
  add_request(BSEC_OUTPUT_STABILIZATION_STATUS);
  add_request(BSEC_OUTPUT_RUN_IN_STATUS);

  bsec_sensor_configuration_t required_sensor_settings[BSEC_MAX_PHYSICAL_SENSOR] = {};
  uint8_t n_required = BSEC_MAX_PHYSICAL_SENSOR;

  bsec_rslt = bsec_update_subscription(this->bsec_instance_.data(), requested_virtual_sensors, n_requested,
                                       required_sensor_settings, &n_required);
  if (!this->check_bsec_status_("bsec_update_subscription", bsec_rslt)) {
    this->bsec_setup_failed_step_ = "bsec_update_subscription";
    this->bsec_setup_failed_result_ = bsec_rslt;
    return false;
  }

  this->bsec_ready_ = true;
  ESP_LOGI(TAG, "BSEC ready (sample rate %.3f Hz)", sample_rate);
  return true;
}

float BME690Component::get_sample_rate_() const {
  return this->sample_rate_ == SAMPLE_RATE_ULP ? BSEC_SAMPLE_RATE_ULP : BSEC_SAMPLE_RATE_LP;
}

const uint8_t *BME690Component::get_bsec_configuration_() const {
  if (this->bsec_configuration_ != nullptr) {
    return this->bsec_configuration_;
  }
  return this->sample_rate_ == SAMPLE_RATE_ULP ? BSEC_CONFIG_IAQ_300S_4D : BSEC_CONFIG_IAQ_3S_4D;
}

uint32_t BME690Component::get_bsec_configuration_length_() const {
  if (this->bsec_configuration_length_ != 0) {
    return this->bsec_configuration_length_;
  }
  return this->sample_rate_ == SAMPLE_RATE_ULP ? sizeof(BSEC_CONFIG_IAQ_300S_4D) : sizeof(BSEC_CONFIG_IAQ_3S_4D);
}

bool BME690Component::push_inputs_to_bsec_(const struct bme69x_data &data, const bsec_bme_settings_t &settings,
                                           int64_t timestamp_ns) {
  bsec_input_t inputs[BSEC_MAX_PHYSICAL_SENSOR] = {};
  uint8_t n_inputs = 0;

  inputs[n_inputs++] = {timestamp_ns, this->ext_temp_offset_, 1, BSEC_INPUT_HEATSOURCE};

  if (bsec_has_input(settings.process_data, BSEC_INPUT_TEMPERATURE)) {
    inputs[n_inputs++] = {timestamp_ns, data.temperature, 1, BSEC_INPUT_TEMPERATURE};
  }
  if (bsec_has_input(settings.process_data, BSEC_INPUT_HUMIDITY)) {
    inputs[n_inputs++] = {timestamp_ns, data.humidity, 1, BSEC_INPUT_HUMIDITY};
  }
  if (bsec_has_input(settings.process_data, BSEC_INPUT_PRESSURE)) {
    inputs[n_inputs++] = {timestamp_ns, data.pressure, 1, BSEC_INPUT_PRESSURE};
  }
  if (bsec_has_input(settings.process_data, BSEC_INPUT_GASRESISTOR) && (data.status & BME69X_GASM_VALID_MSK)) {
    inputs[n_inputs++] = {timestamp_ns, data.gas_resistance, 1, BSEC_INPUT_GASRESISTOR};
  }
  if (bsec_has_input(settings.process_data, BSEC_INPUT_PROFILE_PART) && (data.status & BME69X_GASM_VALID_MSK)) {
    const float profile_part = (settings.op_mode == BME69X_FORCED_MODE) ? 0.0f : static_cast<float>(data.gas_index);
    inputs[n_inputs++] = {timestamp_ns, profile_part, 1, BSEC_INPUT_PROFILE_PART};
  }

  if (n_inputs == 0) {
    return false;
  }

  bsec_output_t outputs[BSEC_NUMBER_OUTPUTS] = {};
  uint8_t num_outputs = BSEC_NUMBER_OUTPUTS;
  auto bsec_rslt = bsec_do_steps(this->bsec_instance_.data(), inputs, n_inputs, outputs, &num_outputs);
  if (!this->check_bsec_status_("bsec_do_steps", bsec_rslt)) {
    this->status_set_warning();
    return false;
  }

  ESP_LOGI(TAG, "BSEC outputs: %u", num_outputs);
  this->handle_bsec_outputs_(outputs, num_outputs);
  this->save_bsec_state_();
  return true;
}

void BME690Component::handle_bsec_outputs_(const bsec_output_t *outputs, uint8_t num_outputs) {
  for (uint8_t idx = 0; idx < num_outputs; idx++) {
    const auto &out = outputs[idx];
    switch (out.sensor_id) {
      case BSEC_OUTPUT_IAQ:
        if (this->iaq_sensor != nullptr) {
          this->iaq_sensor->publish_state(out.signal);
        }
        if (this->iaq_accuracy_sensor != nullptr) {
          this->iaq_accuracy_sensor->publish_state(out.accuracy);
        }
#ifdef USE_TEXT_SENSOR
        if (this->iaq_accuracy_text_sensor_ != nullptr && out.accuracy < 4) {
          this->iaq_accuracy_text_sensor_->publish_state(IAQ_ACCURACY_STATES[out.accuracy]);
        }
#endif
        this->last_iaq_accuracy_ = out.accuracy;
        if (out.accuracy >= 2) {
          this->state_dirty_ = true;
        }
        break;
      case BSEC_OUTPUT_STATIC_IAQ:
        if (this->static_iaq_sensor != nullptr) {
          this->static_iaq_sensor->publish_state(out.signal);
        }
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        if (this->co2_equivalent_sensor != nullptr) {
          this->co2_equivalent_sensor->publish_state(out.signal);
        }
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        if (this->breath_voc_equivalent_sensor != nullptr) {
          this->breath_voc_equivalent_sensor->publish_state(out.signal);
        }
        break;
      case BSEC_OUTPUT_GAS_PERCENTAGE:
        if (this->gas_percentage_sensor != nullptr) {
          this->gas_percentage_sensor->publish_state(out.signal);
        }
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        if (this->comp_temperature_sensor != nullptr) {
          this->comp_temperature_sensor->publish_state(out.signal);
        }
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        if (this->comp_humidity_sensor != nullptr) {
          this->comp_humidity_sensor->publish_state(out.signal);
        }
        break;
      default:
        break;
    }
  }
}

void BME690Component::log_bsec_version_() {
  bsec_version_t ver{};
  auto rslt = bsec_get_version(this->bsec_instance_.data(), &ver);
  if (rslt == BSEC_OK) {
    this->bsec_version_ = ver;
    this->bsec_version_known_ = true;
    ESP_LOGI(TAG, "BSEC version %u.%u.%u.%u", ver.major, ver.minor, ver.major_bugfix, ver.minor_bugfix);
  } else {
    this->bsec_version_known_ = false;
    ESP_LOGW(TAG, "bsec_get_version failed: %d", static_cast<int>(rslt));
  }
}

bool BME690Component::is_bsec_3_3_or_newer_() const {
  if (!this->bsec_version_known_) {
    return false;
  }
  return this->bsec_version_.major > 3 || (this->bsec_version_.major == 3 && this->bsec_version_.minor >= 3);
}

bool BME690Component::load_bsec_state_() {
  std::array<uint8_t, BSEC_MAX_STATE_BLOB_SIZE + 4> buf{};
  if (!this->pref_.load(&buf)) {
    return false;
  }

  uint32_t len = (static_cast<uint32_t>(buf[0]) << 24) | (static_cast<uint32_t>(buf[1]) << 16) |
                 (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
  if (len == 0 || len > BSEC_MAX_STATE_BLOB_SIZE) {
    return false;
  }

  auto rslt = bsec_set_state(this->bsec_instance_.data(), buf.data() + 4, len, this->bsec_work_buffer_.data(),
                             this->bsec_work_buffer_.size());
  if (!this->check_bsec_status_("bsec_set_state", rslt)) {
    return false;
  }

  ESP_LOGI(TAG, "Restored BSEC state (%" PRIu32 " bytes)", len);
  return true;
}

void BME690Component::save_bsec_state_() {
  if (!this->bsec_ready_) {
    return;
  }

  const uint32_t now = millis();
  if (!this->state_dirty_ || (now - this->last_state_save_ms_ < this->state_save_interval_ms_)) {
    return;
  }

  std::array<uint8_t, BSEC_MAX_STATE_BLOB_SIZE> state_buf{};
  uint32_t state_len = BSEC_MAX_STATE_BLOB_SIZE;
  uint8_t state_set_id = 0;
  uint32_t work_buf_len = this->bsec_work_buffer_.size();

  auto rslt = bsec_get_state(this->bsec_instance_.data(), state_set_id, state_buf.data(), state_len,
                             this->bsec_work_buffer_.data(), work_buf_len, &state_len);
  if (!this->check_bsec_status_("bsec_get_state", rslt)) {
    return;
  }

  std::array<uint8_t, BSEC_MAX_STATE_BLOB_SIZE + 4> buf{};
  buf[0] = static_cast<uint8_t>((state_len >> 24) & 0xFF);
  buf[1] = static_cast<uint8_t>((state_len >> 16) & 0xFF);
  buf[2] = static_cast<uint8_t>((state_len >> 8) & 0xFF);
  buf[3] = static_cast<uint8_t>(state_len & 0xFF);
  std::copy(state_buf.begin(), state_buf.begin() + state_len, buf.begin() + 4);

  if (this->pref_.save(&buf)) {
    this->last_state_save_ms_ = now;
    this->state_dirty_ = false;
    ESP_LOGI(TAG, "Saved BSEC state (%" PRIu32 " bytes)", state_len);
  } else {
    ESP_LOGW(TAG, "Failed to save BSEC state");
  }
}

int64_t BME690Component::get_time_ns_() {
  uint32_t time_ms = millis();
  if (this->last_time_ms_ > time_ms) {
    this->millis_overflow_counter_++;
  }
  this->last_time_ms_ = time_ms;

  return (time_ms + (static_cast<int64_t>(this->millis_overflow_counter_) << 32)) * INT64_C(1000000);
}

}  // namespace esphome::bme690
