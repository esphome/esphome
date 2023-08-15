#include "scd4x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace scd4x {

static const char *const TAG = "scd4x";

static const uint16_t SCD4X_CMD_GET_SERIAL_NUMBER = 0x3682;
static const uint16_t SCD4X_CMD_TEMPERATURE_OFFSET = 0x241d;
static const uint16_t SCD4X_CMD_ALTITUDE_COMPENSATION = 0x2427;
static const uint16_t SCD4X_CMD_AMBIENT_PRESSURE_COMPENSATION = 0xe000;
static const uint16_t SCD4X_CMD_AUTOMATIC_SELF_CALIBRATION = 0x2416;
static const uint16_t SCD4X_CMD_START_CONTINUOUS_MEASUREMENTS = 0x21b1;
static const uint16_t SCD4X_CMD_START_LOW_POWER_CONTINUOUS_MEASUREMENTS = 0x21ac;
static const uint16_t SCD4X_CMD_START_LOW_POWER_SINGLE_SHOT = 0x219d;  // SCD41 only
static const uint16_t SCD4X_CMD_START_LOW_POWER_SINGLE_SHOT_RHT_ONLY = 0x2196;
static const uint16_t SCD4X_CMD_GET_DATA_READY_STATUS = 0xe4b8;
static const uint16_t SCD4X_CMD_READ_MEASUREMENT = 0xec05;
static const uint16_t SCD4X_CMD_PERFORM_FORCED_CALIBRATION = 0x362f;
static const uint16_t SCD4X_CMD_STOP_MEASUREMENTS = 0x3f86;
static const uint16_t SCD4X_CMD_FACTORY_RESET = 0x3632;
static const uint16_t SCD4X_CMD_GET_FEATURESET = 0x202f;
static const float SCD4X_TEMPERATURE_OFFSET_MULTIPLIER = (1 << 16) / 175.0f;
static const uint16_t SCD41_ID = 0x1408;
static const uint16_t SCD40_ID = 0x440;

void SCD4XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up scd4x...");
  // the sensor needs 1000 ms to enter the idle state
  this->set_timeout(1000, [this]() {
    this->status_clear_error();
    if (!this->write_command(SCD4X_CMD_STOP_MEASUREMENTS)) {
      ESP_LOGE(TAG, "Failed to stop measurements");
      this->mark_failed();
      return;
    }
    // According to the SCD4x datasheet the sensor will only respond to other commands after waiting 500 ms after
    // issuing the stop_periodic_measurement command
    this->set_timeout(500, [this]() {
      uint16_t raw_serial_number[3];
      if (!this->get_register(SCD4X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 3, 1)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      ESP_LOGD(TAG, "Serial number %02d.%02d.%02d", (uint16_t(raw_serial_number[0]) >> 8),
               uint16_t(raw_serial_number[0] & 0xFF), (uint16_t(raw_serial_number[1]) >> 8));

      if (!this->write_command(SCD4X_CMD_TEMPERATURE_OFFSET,
                               (uint16_t) (temperature_offset_ * SCD4X_TEMPERATURE_OFFSET_MULTIPLIER))) {
        ESP_LOGE(TAG, "Error setting temperature offset.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }

      // If pressure compensation available use it
      // else use altitude
      if (ambient_pressure_compensation_) {
        if (!this->update_ambient_pressure_compensation_(ambient_pressure_)) {
          ESP_LOGE(TAG, "Error setting ambient pressure compensation.");
          this->error_code_ = MEASUREMENT_INIT_FAILED;
          this->mark_failed();
          return;
        }
      } else {
        if (!this->write_command(SCD4X_CMD_ALTITUDE_COMPENSATION, altitude_compensation_)) {
          ESP_LOGE(TAG, "Error setting altitude compensation.");
          this->error_code_ = MEASUREMENT_INIT_FAILED;
          this->mark_failed();
          return;
        }
      }

      if (!this->write_command(SCD4X_CMD_AUTOMATIC_SELF_CALIBRATION, enable_asc_ ? 1 : 0)) {
        ESP_LOGE(TAG, "Error setting automatic self calibration.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }

      initialized_ = true;
      // Finally start sensor measurements
      this->start_measurement_();
      ESP_LOGD(TAG, "Sensor initialized");
    });
  });
}

void SCD4XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "scd4x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement Initialization failed!");
        break;
      case SERIAL_NUMBER_IDENTIFICATION_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  ESP_LOGCONFIG(TAG, "  Automatic self calibration: %s", ONOFF(this->enable_asc_));
  if (this->ambient_pressure_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dynamic ambient pressure compensation using sensor '%s'",
                  this->ambient_pressure_source_->get_name().c_str());
  } else {
    if (this->ambient_pressure_compensation_) {
      ESP_LOGCONFIG(TAG, "  Altitude compensation disabled");
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %dmBar", this->ambient_pressure_);
    } else {
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation disabled");
      ESP_LOGCONFIG(TAG, "  Altitude compensation: %dm", this->altitude_compensation_);
    }
  }
  switch (this->measurement_mode_) {
    case PERIODIC:
      ESP_LOGCONFIG(TAG, "  Measurement mode: periodic (5s)");
      break;
    case LOW_POWER_PERIODIC:
      ESP_LOGCONFIG(TAG, "  Measurement mode: low power periodic (30s)");
      break;
    case SINGLE_SHOT:
      ESP_LOGCONFIG(TAG, "  Measurement mode: single shot");
      break;
    case SINGLE_SHOT_RHT_ONLY:
      ESP_LOGCONFIG(TAG, "  Measurement mode: single shot rht only");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Temperature offset: %.2f °C", this->temperature_offset_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void SCD4XComponent::update() {
  if (!initialized_) {
    return;
  }

  if (this->ambient_pressure_source_ != nullptr) {
    float pressure = this->ambient_pressure_source_->state;
    if (!std::isnan(pressure)) {
      set_ambient_pressure_compensation(pressure);
    }
  }

  uint32_t wait_time = 0;
  if (this->measurement_mode_ == SINGLE_SHOT || this->measurement_mode_ == SINGLE_SHOT_RHT_ONLY) {
    start_measurement_();
    wait_time =
        this->measurement_mode_ == SINGLE_SHOT ? 5000 : 50;  // Single shot measurement takes 5 secs rht mode 50 ms
  }
  this->set_timeout(wait_time, [this]() {
    // Check if data is ready
    if (!this->write_command(SCD4X_CMD_GET_DATA_READY_STATUS)) {
      this->status_set_warning();
      return;
    }

    uint16_t raw_read_status;

    if (!this->read_data(raw_read_status) || raw_read_status == 0x00) {
      this->status_set_warning();
      ESP_LOGW(TAG, "Data not ready yet!");
      return;
    }

    if (!this->write_command(SCD4X_CMD_READ_MEASUREMENT)) {
      ESP_LOGW(TAG, "Error reading measurement!");
      this->status_set_warning();
      return;  // NO RETRY
    }
    // Read off sensor data
    uint16_t raw_data[3];
    if (!this->read_data(raw_data, 3)) {
      this->status_set_warning();
      return;
    }
    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(raw_data[0]);

    if (this->temperature_sensor_ != nullptr) {
      const float temperature = -45.0f + (175.0f * (raw_data[1])) / (1 << 16);
      this->temperature_sensor_->publish_state(temperature);
    }
    if (this->humidity_sensor_ != nullptr) {
      const float humidity = (100.0f * raw_data[2]) / (1 << 16);
      this->humidity_sensor_->publish_state(humidity);
    }
    this->status_clear_warning();
  });  // set_timeout
}

bool SCD4XComponent::perform_forced_calibration(uint16_t current_co2_concentration) {
  /*
    Operate the SCD4x in the operation mode later used in normal sensor operation (periodic measurement, low power
    periodic measurement or single shot) for > 3 minutes in an environment with homogeneous and constant CO2
    concentration before performing a forced recalibration.
  */
  if (!this->write_command(SCD4X_CMD_STOP_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Failed to stop measurements");
    this->status_set_warning();
  }
  this->set_timeout(500, [this, current_co2_concentration]() {
    if (this->write_command(SCD4X_CMD_PERFORM_FORCED_CALIBRATION, current_co2_concentration)) {
      ESP_LOGD(TAG, "setting forced calibration Co2 level %d ppm", current_co2_concentration);
      // frc takes 400 ms
      // because this method will be used very rarly
      // the simple approach with delay is ok
      delay(400);  // NOLINT'
      if (!this->start_measurement_()) {
        return false;
      } else {
        ESP_LOGD(TAG, "forced calibration complete");
      }
      return true;
    } else {
      ESP_LOGE(TAG, "force calibration failed");
      this->error_code_ = FRC_FAILED;
      this->status_set_warning();
      return false;
    }
  });
  return true;
}

bool SCD4XComponent::factory_reset() {
  if (!this->write_command(SCD4X_CMD_STOP_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Failed to stop measurements");
    this->status_set_warning();
    return false;
  }

  this->set_timeout(500, [this]() {
    if (!this->write_command(SCD4X_CMD_FACTORY_RESET)) {
      ESP_LOGE(TAG, "Failed to send factory reset command");
      this->status_set_warning();
      return false;
    }
    ESP_LOGD(TAG, "Factory reset complete");
    return true;
  });
  return true;
}

void SCD4XComponent::set_ambient_pressure_compensation(float pressure_in_hpa) {
  ambient_pressure_compensation_ = true;
  uint16_t new_ambient_pressure = (uint16_t) pressure_in_hpa;
  if (!initialized_) {
    ambient_pressure_ = new_ambient_pressure;
    return;
  }
  // Only send pressure value if it has changed since last update
  if (new_ambient_pressure != ambient_pressure_) {
    update_ambient_pressure_compensation_(new_ambient_pressure);
    ambient_pressure_ = new_ambient_pressure;
  } else {
    ESP_LOGD(TAG, "ambient pressure compensation skipped - no change required");
  }
}

bool SCD4XComponent::update_ambient_pressure_compensation_(uint16_t pressure_in_hpa) {
  if (this->write_command(SCD4X_CMD_AMBIENT_PRESSURE_COMPENSATION, pressure_in_hpa)) {
    ESP_LOGD(TAG, "setting ambient pressure compensation to %d hPa", pressure_in_hpa);
    return true;
  } else {
    ESP_LOGE(TAG, "Error setting ambient pressure compensation.");
    return false;
  }
}

bool SCD4XComponent::start_measurement_() {
  uint16_t measurement_command = SCD4X_CMD_START_CONTINUOUS_MEASUREMENTS;
  switch (this->measurement_mode_) {
    case PERIODIC:
      measurement_command = SCD4X_CMD_START_CONTINUOUS_MEASUREMENTS;
      break;
    case LOW_POWER_PERIODIC:
      measurement_command = SCD4X_CMD_START_LOW_POWER_CONTINUOUS_MEASUREMENTS;
      break;
    case SINGLE_SHOT:
      measurement_command = SCD4X_CMD_START_LOW_POWER_SINGLE_SHOT;
      break;
    case SINGLE_SHOT_RHT_ONLY:
      measurement_command = SCD4X_CMD_START_LOW_POWER_SINGLE_SHOT_RHT_ONLY;
      break;
  }

  static uint8_t remaining_retries = 3;
  while (remaining_retries) {
    if (!this->write_command(measurement_command)) {
      ESP_LOGE(TAG, "Error starting measurements.");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->status_set_warning();
      if (--remaining_retries == 0)
        return false;
      delay(50);  // NOLINT wait 50 ms and try again
    }
    this->status_clear_warning();
    return true;
  }
  return false;
}

}  // namespace scd4x
}  // namespace esphome
