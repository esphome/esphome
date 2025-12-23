#include "stcc4.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stcc4 {

static const char *const TAG = "stcc4";

void STCC4Component::start_continuous_measurement_() {
  // Takes the sensor out of idle mode to begin taking measurements
  if (!this->write_command((uint16_t) SensorCommand::START_CONTINUOUS_MEASUREMENT)) {
    ESP_LOGW(TAG, "Sensor startup failed");
    this->status_set_error();
    return;
  }

  delay_microseconds_safe(1000 * 1000 * 1);

  // As per recommended procedure, attempt to read measurements until we recieve a good one (uint16_t)
  // SensorCommand::READ_MEASUREMENT, buffer, 4, (uint8_t) 150)
  uint16_t buffer[4];
  bool good_data = false;
  for (int i = 0; i < 5 && !good_data; i++) {
    if (this->get_register((uint16_t) SensorCommand::READ_MEASUREMENT, buffer, 4, (uint8_t) 150)) {
      good_data = true;
      break;
    }
    delay_microseconds_safe(1000 * 1000);
  }

  if (!good_data) {
    ESP_LOGE(TAG, "Failed to get valid measurement after retries");
    this->status_set_error();
    return;
  }
}

void STCC4Component::stop_continuous_measurement_() {
  // Takes the sensor out of continuous measurement mode and into idle mode, takes approx 1.2 seconds
  if (!this->write_command((uint16_t) SensorCommand::STOP_CONTINUOUS_MEASUREMENT)) {
    ESP_LOGW(TAG, "Failed to enter idle mode");
    this->status_set_warning();
    return;
  }

  delay_microseconds_safe(1000 * 1000 * 2);
  this->state_.is_idle = true;
}

void STCC4Component::read_measurement_(uint16_t *data) {
  // Read sensor data while in continuous measurement mode
  if (this->state_.is_idle) {
    ESP_LOGW(TAG, "Sensor must be in continuous measurement mode to use this command");
    this->status_set_error();
    return;
  }

  if (!this->get_register((uint16_t) SensorCommand::READ_MEASUREMENT, data, 4, (uint8_t) 1)) {
    ESP_LOGW(TAG, "Sensor read failed");
    this->status_set_warning();
    return;
  }
}

void STCC4Component::set_rht_compensation_(uint16_t temp, uint16_t rh) {
  if (this->state_.is_sht45_present) {
    ESP_LOGW(TAG, "This command is only available without an SHT45 on the STCC4's internal i2c bus");
    this->status_set_warning();
    return;
  }

  uint16_t data[2] = {temp, rh};

  if (!this->write_command((uint16_t) SensorCommand::SET_RHT_COMPENSATION, data, 2)) {
    ESP_LOGW(TAG, "Sensor RHT compensation failed");
    this->status_set_error();
    return;
  }
  delay_microseconds_safe(1000 * 2);
  this->state_.is_rht_compensated = true;
}

void STCC4Component::set_pressure_compensation_(uint16_t pressure) {
  if (!this->write_command((uint16_t) SensorCommand::SET_PRESSURE_COMPENSATION, pressure)) {
    ESP_LOGW(TAG, "Sensor pressure compensation failed");
    this->status_set_error();
    return;
  }

  delay_microseconds_safe(1000 * 2);
  this->state_.is_pressure_compensated = true;
}

void STCC4Component::measure_single_shot_(uint16_t *data) {
  if (this->state_.is_sleep) {
    this->exit_sleep_mode_();
    if (this->state_.is_sleep) {
      ESP_LOGW(TAG, "Sensor failed to wake from sleep mode");
      this->status_set_error();
      return;
    }
  }

  if (!this->write_command((uint16_t) SensorCommand::MEASURE_SINGLE_SHOT)) {
    ESP_LOGW(TAG, "Sensor failed to ACK single shot command");
    this->status_set_error();
    return;
  }

  delay_microseconds_safe(1000 * 500);
  this->state_.is_idle = false;
  this->read_measurement_(data);
  this->state_.is_idle = true;

  this->enter_sleep_mode_();
}

void STCC4Component::enter_sleep_mode_() {
  // %TODO% Maybe verify if the device is idle, if not, fail function.

  if (!this->write_command((uint16_t) SensorCommand::ENTER_SLEEP_MODE)) {
    ESP_LOGW(TAG, "Sensor did not enter sleep mode properly");
    this->status_set_error();
    return;
  }
  delay_microseconds_safe(1000 * 2);
  this->state_.is_sleep = true;
}

void STCC4Component::exit_sleep_mode_() {
  // %TODO% Maybe verify if the device is idle, if not, fail function.

  if (!this->write_command((uint16_t) SensorCommand::EXIT_SLEEP_MODE)) {
    ESP_LOGW(TAG, "Sensor did not exit sleep mode properly");
    this->status_set_error();
    return;
  }
  delay_microseconds_safe(1000 * 2);
  this->state_.is_sleep = false;
}

void STCC4Component::perform_conditioning_() {
  if (!this->write_command((uint16_t) SensorCommand::PERFORM_CONDITIONING)) {
    ESP_LOGW(TAG, "Sensor did not ACK conditioning command");
    this->status_set_error();
    return;
  }
  delay_microseconds_safe(1000 * 1000 * 22);
}

void STCC4Component::perform_soft_reset_() {
  if (!this->write_command((uint16_t) SensorCommand::PERFORM_SOFT_RESET)) {
    ESP_LOGW(TAG, "Sensor failed to reset");
    this->status_set_error();
    return;
  }

  delay_microseconds_safe(1000 * 11);
  this->state_.reset();
}

void STCC4Component::perform_factory_reset_() {
  uint16_t buffer[1];
  if (!this->get_register((uint16_t) SensorCommand::PERFORM_FACTORY_RESET, buffer, (uint8_t) 90)) {
    ESP_LOGW(TAG, "Sensor failed to ACK factory reset command");
    this->status_set_error();
    return;
  }

  // Fail if buffer is not 0
  if (buffer[0]) {
    ESP_LOGW(TAG, "Sensor failed to factory reset");
    this->status_set_error();
    return;
  }

  this->state_.reset();  // Reset all states
  this->setup();
}

void STCC4Component::perform_self_test_() {
  uint16_t result = 0;

  if (!this->get_register((uint16_t) SensorCommand::PERFORM_SELF_TEST, &result, (uint8_t) 360)) {
    ESP_LOGW(TAG, "Sensor failed to ACK test command");
    this->status_set_error();
    return;
  }

  // PASS
  if (result == 0x0000 || result == 0x0010) {
    this->state_.is_sht45_present = true;
    this->status_clear_warning();
    return;
  }

  // FAIL decode
  if (result & 0x0001) {
    ESP_LOGW(TAG, "Sensor voltage out of range");
    this->status_set_error();
    return;
  }

  if (result & 0x000E) {
    ESP_LOGW(TAG, "Sensor failed, contact Sensirion with code 0x%04X", result);
    this->status_set_error();
    return;
  }

  if (result & 0x0010) {
    ESP_LOGW(TAG, "Sensor failed, no SHT45; using default values for compensation");
    this->state_.is_sht45_present = false;
    this->status_set_warning();
    return;
  }

  if (result & 0x0060) {
    ESP_LOGW(TAG, "Sensor failed, memory error; attempting reset");
    this->status_set_warning();
    this->perform_soft_reset_();

    // one retry
    uint16_t retry = 0;
    if (this->get_register((uint16_t) SensorCommand::PERFORM_SELF_TEST, &retry, (uint8_t) 360) &&
        (retry == 0x0000 || retry == 0x0010)) {
      this->state_.is_sht45_present = true;
      this->status_clear_warning();
      return;
    }

    ESP_LOGW(TAG, "Sensor still reports memory error (0x%04X); performance may be degraded", retry);
    this->status_set_warning();
    return;
  }

  // If we got here: some unexpected bit pattern
  ESP_LOGW(TAG, "Sensor self-test returned unexpected code 0x%04X", result);
  this->status_set_warning();
}

void STCC4Component::read_serial_number_() {
  uint16_t buffer[6];
  if (!this->get_register((uint16_t) SensorCommand::GET_PRODUCT_ID, buffer, 6, (uint8_t) 1)) {
    ESP_LOGE(TAG, "Get serial number failed");
    this->serial_number_ = 0;
    return;
  }
  // Combine last 4 words into 64-bit serial number
  this->serial_number_ =
      (uint64_t(buffer[2]) << 48) | (uint64_t(buffer[3]) << 32) | (uint64_t(buffer[4]) << 16) | (uint64_t(buffer[5]));

  ESP_LOGD(TAG, "Serial number: %016" PRIx64, this->serial_number_);
}

void STCC4Component::setup() {
  this->perform_self_test_();
  if (this->status_has_error()) {
    return;
  }

  if (continuous_) {
    this->start_continuous_measurement_();
  }
}

void STCC4Component::update() {
  // Send command and read result
  uint16_t buffer[4] = {0};
  if (continuous_) {
    this->read_measurement_(buffer);
  } else {
    this->measure_single_shot_(buffer);
  }

  if (this->co2_sensor_ != nullptr) {
    // CO2 is contained in the first word
    float sensor_value = buffer[0];
    this->co2_sensor_->publish_state(sensor_value);
  }

  if (this->temp_sensor_ != nullptr) {
    // Temp is contained in the second result word
    float sensor_value_temp = buffer[1];
    float temp = -45.0f + 175.0f * sensor_value_temp / 65535.0f;

    this->temp_sensor_->publish_state(temp);
  }

  if (this->humidity_sensor_ != nullptr) {
    // Relative humidity is in the third result word
    float sensor_value_rh = buffer[2];
    float rh = -6.0f + 125.0f * sensor_value_rh / 65535.0f;

    this->humidity_sensor_->publish_state(rh);
  }
}

}  // namespace stcc4
}  // namespace esphome
