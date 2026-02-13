#include "stcc4.h"
#include "esphome/core/log.h"

namespace esphome {
namespace stcc4 {

static const char *const TAG = "stcc4";

void STCC4Component::start_continuous_measurement_() {
  // Takes the sensor out of idle mode to begin taking measurements
  this->write_command((uint16_t) SensorCommand::START_CONTINUOUS_MEASUREMENT);
  this->state_.is_idle = false;
}

void STCC4Component::stop_continuous_measurement_() {
  // Takes the sensor out of continuous measurement mode and into idle mode, takes approx 1.2 seconds
  this->write_command((uint16_t) SensorCommand::STOP_CONTINUOUS_MEASUREMENT);

  this->state_.is_idle = true;
}

void STCC4Component::read_measurement_(uint16_t *data) {
  // Read sensor data while in continuous measurement mode
  this->read_data(data, 4);
  this->state_.is_measuring = true;
}

void STCC4Component::set_rht_compensation_(uint16_t temp, uint16_t rh) {
  if (this->state_.is_sht45_present) {
    ESP_LOGW(TAG, "This command is only available without an SHT45 on the STCC4's internal i2c bus");
    this->status_set_warning();
    return;
  }

  uint16_t data[2] = {temp, rh};

  write_command((uint16_t) SensorCommand::SET_RHT_COMPENSATION, data, 2);
  delay_microseconds_safe(1000 * 2);
  this->state_.is_rht_compensated = true;
}

void STCC4Component::set_pressure_compensation_(float pressure_hpa) {
  uint16_t pressure_half_pa = (uint16_t) (pressure_hpa * 50.0f);

  if (pressure_half_pa < 20000)
    pressure_half_pa = 20000;
  if (pressure_half_pa > 55000)
    pressure_half_pa = 55000;

  write_command((uint16_t) SensorCommand::SET_PRESSURE_COMPENSATION, pressure_half_pa);
  delay_microseconds_safe(1000 * 2);
  this->state_.is_pressure_compensated = true;

  // Reset read cycle
  this->write_command((uint16_t) SensorCommand::READ_MEASUREMENT);
  delay_microseconds_safe(1 * 1000);
}

void STCC4Component::measure_single_shot_(uint16_t *data) {
  if (this->state_.is_sleep) {
    this->exit_sleep_mode_();
    delay_microseconds_safe(5 * 1000);
  }

  else if (!this->state_.is_idle) {
    ESP_LOGE(TAG, "Sensor must be idle to run this command");
    return;
  }

  this->write_command((uint16_t) SensorCommand::MEASURE_SINGLE_SHOT);
  this->set_timeout(500, [this, data] {
    this->write_command((uint16_t) SensorCommand::READ_MEASUREMENT);
    delay_microseconds_safe(1 * 1000);
    this->read_measurement_(data);
    this->enter_sleep_mode_();
    return;
  });
}

void STCC4Component::enter_sleep_mode_() {
  // %TODO% Maybe verify if the device is idle, if not, fail function.
  if (!this->state_.is_idle || this->state_.is_sleep) {
    return;
  }
  this->write_command((uint16_t) SensorCommand::ENTER_SLEEP_MODE);
  this->state_.is_sleep = true;
}

bool STCC4Component::exit_sleep_mode_() {
  // %TODO% Maybe verify if the device is idle, if not, fail function.

  if (!this->state_.is_sleep) {
    return false;  // Sensor not sleeping
  }
  this->write_command((uint16_t) SensorCommand::EXIT_SLEEP_MODE);
  this->state_.is_sleep = false;
  return true;  // Sensor out of sleep
}

void STCC4Component::perform_conditioning_() { this->write_command((uint16_t) SensorCommand::PERFORM_CONDITIONING); }

void STCC4Component::perform_soft_reset_() {
  this->write_command((uint16_t) SensorCommand::PERFORM_SOFT_RESET);
  this->state_.reset();
}

void STCC4Component::perform_factory_reset_() {
  // %todo% rewrite using FSM

  // uint16_t buffer[1];
  // if (!this->get_register((uint16_t) SensorCommand::PERFORM_FACTORY_RESET, buffer, (uint8_t) 90)) {
  //   ESP_LOGW(TAG, "Sensor failed to ACK factory reset command");
  //   this->status_set_error();
  //   return;
  // }

  // // Fail if buffer is not 0
  // if (buffer[0]) {
  //   ESP_LOGW(TAG, "Sensor failed to factory reset");
  //   this->status_set_error();
  //   return;
  // }

  // this->state_.reset();  // Reset all states
  // this->setup();
}

bool STCC4Component::perform_self_test_() {
  uint16_t result;

  this->read_data(&result, 2);

  // PASS
  if (result == 0x0000 || result == 0x0010) {
    ESP_LOGI(TAG, "STCC4 Nominal");
    this->state_.is_sht45_present = true;
    this->status_clear_warning();
    return true;
  }

  // FAIL decode
  if (result & 0x0001) {
    ESP_LOGW(TAG, "Sensor voltage out of range, sensor may stabilize");
    this->status_set_warning();
    return true;
  }

  if (result & 0x000E) {
    ESP_LOGW(TAG, "Sensor failed, contact Sensirion with code 0x%04X", result);
    this->status_set_error();
    this->mark_failed();
    return true;
  }

  if (result & 0x0010) {
    ESP_LOGW(TAG, "Sensor failed, no SHT45; using default values for compensation");
    this->state_.is_sht45_present = false;
    this->status_set_warning();
    return true;
  }

  if (result & 0x0060) {
    ESP_LOGW(TAG, "Sensor failed, memory error; attempting reset");
    this->status_set_warning();
    this->perform_soft_reset_();

    ESP_LOGW(TAG, "Sensor still reports memory error; performance may be degraded");
    this->status_set_warning();
    return true;
  }

  if (result == 0xFFFF) {
    ESP_LOGW(TAG, "Self test not ready!");
    return false;
  }

  // If we got here: some unexpected bit pattern
  ESP_LOGW(TAG, "Sensor self-test returned unexpected code 0x%04X", result);
  this->status_set_warning();
  return true;
}

void STCC4Component::read_serial_number_() {
  if (this->state_.is_measuring) {
    ESP_LOGW(TAG, "Serial number can not be read during continuous measurement!");
    return;
  }

  uint16_t buffer[6];
  if (!this->get_register((uint16_t) SensorCommand::GET_PRODUCT_ID, buffer, 6, 1)) {
    ESP_LOGW(TAG, "Failed to read serial number");
  }

  this->serial_number_ = ((uint64_t) buffer[2] << 48) | ((uint64_t) buffer[3] << 32) | ((uint64_t) buffer[4] << 16) |
                         ((uint64_t) buffer[5]);
  // %todo% rewrite function using FSM if needed

  // uint16_t buffer[6];
  // if (!this->get_register((uint16_t) SensorCommand::GET_PRODUCT_ID, buffer, 6, (uint8_t) 1)) {
  //   ESP_LOGE(TAG, "Get serial number failed");
  //   this->serial_number_ = 0;
  //   return;
  // }
  // // Combine last 4 words into 64-bit serial number
  // this->serial_number_ =
  //     (uint64_t(buffer[2]) << 48) | (uint64_t(buffer[3]) << 32) | (uint64_t(buffer[4]) << 16) |
  //     (uint64_t(buffer[5]));

  // ESP_LOGD(TAG, "Serial number: %016" PRIx64, this->serial_number_);
}

void STCC4Component::setup() {
  this->stage_ = 0;
  this->continue_setup_();
}

void STCC4Component::continue_setup_() {
  switch (stage_) {
    case 0:
      this->exit_sleep_mode_();
      this->stage_++;
      this->set_timeout("Sensor Wakeup", 25, [this]() { continue_setup_(); });
      break;

    case 1:
      this->perform_soft_reset_();
      this->stage_++;
      this->set_timeout("Sensor reset", 10, [this]() { continue_setup_(); });
      break;

    case 2:
      this->write_command((uint16_t) SensorCommand::PERFORM_SELF_TEST);
      this->stage_++;
      this->set_timeout("Sensor self-test command sent!", 500, [this]() { continue_setup_(); });
      break;

    case 3:
      if (!this->perform_self_test_()) {
        this->set_timeout("Running test again", 500, [this]() { continue_setup_(); });
        break;
      }
      ESP_LOGI(TAG, "Self check completed");

      this->perform_conditioning_();
      this->stage_++;
      this->set_timeout("Beginning Conditioning", 22000, [this]() { continue_setup_(); });
      break;

    case 4: {
      if (this->continuous_) {
        ESP_LOGI(TAG, "Conditioning Complete!");
        this->start_continuous_measurement_();
        this->stage_++;
        ESP_LOGI(TAG, "Beginning Cont Measurement");
        this->set_timeout("Beginning cont measurement", 1200, [this]() { continue_setup_(); });
        break;
      }
    }
    case 5: {
      // Try to read sensor
      uint16_t data[3] = {0};
      this->read_measurement_(data);

      if (data[0] == 0) {
        ESP_LOGI(TAG, "CO2 Data Invalid");
        // CO2 ppm is 0, which is not possible, retry
        this->set_timeout("Retrying read", 150, [this]() { continue_setup_(); });
        break;
      }
      this->stage_ = 99;
      break;
    }

    case 99:
      ESP_LOGI(TAG, "STCC4 Init completed!");
      break;
    default:
      break;
  }
}

void STCC4Component::update() {
  if (this->state_.is_conditioning) {
    return;
  }

  float pressure = NAN;
  if (this->pressure_sensor_ != nullptr) {
    pressure = this->pressure_sensor_->state;
  }

  if (!std::isnan(pressure)) {  // Use isnan() to check for NAN
    this->set_pressure_compensation_(pressure);
  }

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
