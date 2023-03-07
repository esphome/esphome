#include "scd30.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP8266
#include <Wire.h>
#endif

namespace esphome {
namespace scd30 {

static const char *const TAG = "scd30";

static const uint16_t SCD30_CMD_GET_FIRMWARE_VERSION = 0xd100;
static const uint16_t SCD30_CMD_START_CONTINUOUS_MEASUREMENTS = 0x0010;
static const uint16_t SCD30_CMD_ALTITUDE_COMPENSATION = 0x5102;
static const uint16_t SCD30_CMD_AUTOMATIC_SELF_CALIBRATION = 0x5306;
static const uint16_t SCD30_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SCD30_CMD_READ_MEASUREMENT = 0x0300;

/// Commands for future use
static const uint16_t SCD30_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SCD30_CMD_MEASUREMENT_INTERVAL = 0x4600;
static const uint16_t SCD30_CMD_FORCED_CALIBRATION = 0x5204;
static const uint16_t SCD30_CMD_TEMPERATURE_OFFSET = 0x5403;
static const uint16_t SCD30_CMD_SOFT_RESET = 0xD304;

void SCD30Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up scd30...");

#ifdef USE_ESP8266
  Wire.setClockStretchLimit(150000);
#endif

  /// Firmware version identification
  uint16_t raw_firmware_version[3];
  if (!this->get_register(SCD30_CMD_GET_FIRMWARE_VERSION, raw_firmware_version, 3)) {
    this->error_code_ = FIRMWARE_IDENTIFICATION_FAILED;
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "SCD30 Firmware v%0d.%02d", (uint16_t(raw_firmware_version[0]) >> 8),
           uint16_t(raw_firmware_version[0] & 0xFF));

  if (this->temperature_offset_ != 0) {
    if (!this->write_command(SCD30_CMD_TEMPERATURE_OFFSET, (uint16_t)(temperature_offset_ * 100.0))) {
      ESP_LOGE(TAG, "Sensor SCD30 error setting temperature offset.");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->mark_failed();
      return;
    }
  }
#ifdef USE_ESP32
  // According ESP32 clock stretching is typically 30ms and up to 150ms "due to
  // internal calibration processes". The I2C peripheral only supports 13ms (at
  // least when running at 80MHz).
  // In practice it seems that clock stretching occurs during this calibration
  // calls. It also seems that delays in between calls makes them
  // disappear/shorter. Hence work around with delays for ESP32.
  //
  // By experimentation a delay of 20ms as already sufficient. Let's go
  // safe and use 30ms delays.
  delay(30);
#endif

  if (!this->write_command(SCD30_CMD_MEASUREMENT_INTERVAL, update_interval_)) {
    ESP_LOGE(TAG, "Sensor SCD30 error setting update interval.");
    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }
#ifdef USE_ESP32
  delay(30);
#endif

  // The start measurement command disables the altitude compensation, if any, so we only set it if it's turned on
  if (this->altitude_compensation_ != 0xFFFF) {
    if (!this->write_command(SCD30_CMD_ALTITUDE_COMPENSATION, altitude_compensation_)) {
      ESP_LOGE(TAG, "Sensor SCD30 error setting altitude compensation.");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->mark_failed();
      return;
    }
  }
#ifdef USE_ESP32
  delay(30);
#endif

  if (!this->write_command(SCD30_CMD_AUTOMATIC_SELF_CALIBRATION, enable_asc_ ? 1 : 0)) {
    ESP_LOGE(TAG, "Sensor SCD30 error setting automatic self calibration.");
    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }
#ifdef USE_ESP32
  delay(30);
#endif

  /// Sensor initialization
  if (!this->write_command(SCD30_CMD_START_CONTINUOUS_MEASUREMENTS, this->ambient_pressure_compensation_)) {
    ESP_LOGE(TAG, "Sensor SCD30 error starting continuous measurements.");
    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }

  // check each 500ms if data is ready, and read it in that case
  this->set_interval("status-check", 500, [this]() {
    if (this->is_data_ready_())
      this->update();
  });
}

void SCD30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "scd30:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement Initialization failed!");
        break;
      case FIRMWARE_IDENTIFICATION_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  if (this->altitude_compensation_ == 0xFFFF) {
    ESP_LOGCONFIG(TAG, "  Altitude compensation: OFF");
  } else {
    ESP_LOGCONFIG(TAG, "  Altitude compensation: %dm", this->altitude_compensation_);
  }
  ESP_LOGCONFIG(TAG, "  Automatic self calibration: %s", ONOFF(this->enable_asc_));
  ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %dmBar", this->ambient_pressure_compensation_);
  ESP_LOGCONFIG(TAG, "  Temperature offset: %.2f °C", this->temperature_offset_);
  ESP_LOGCONFIG(TAG, "  Update interval: %ds", this->update_interval_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void SCD30Component::update() {
  uint16_t raw_read_status;
  if (!this->read_data(raw_read_status) || raw_read_status == 0x00) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Data not ready yet!");
    return;
  }

  if (!this->write_command(SCD30_CMD_READ_MEASUREMENT)) {
    ESP_LOGW(TAG, "Error reading measurement!");
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[6];
    if (!this->read_data(raw_data, 6)) {
      this->status_set_warning();
      return;
    }

    union uint32_float_t {
      uint32_t uint32;
      float value;
    };
    uint32_t temp_c_o2_u32 = (((uint32_t(raw_data[0])) << 16) | (uint32_t(raw_data[1])));
    uint32_float_t co2{.uint32 = temp_c_o2_u32};

    uint32_t temp_temp_u32 = (((uint32_t(raw_data[2])) << 16) | (uint32_t(raw_data[3])));
    uint32_float_t temperature{.uint32 = temp_temp_u32};

    uint32_t temp_hum_u32 = (((uint32_t(raw_data[4])) << 16) | (uint32_t(raw_data[5])));
    uint32_float_t humidity{.uint32 = temp_hum_u32};

    ESP_LOGD(TAG, "Got CO2=%.2fppm temperature=%.2f°C humidity=%.2f%%", co2.value, temperature.value, humidity.value);
    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(co2.value);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature.value);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity.value);

    this->status_clear_warning();
  });
}

bool SCD30Component::is_data_ready_() {
  if (!this->write_command(SCD30_CMD_GET_DATA_READY_STATUS)) {
    return false;
  }
  delay(4);
  uint16_t is_data_ready;
  if (!this->read_data(&is_data_ready, 1)) {
    return false;
  }
  return is_data_ready == 1;
}

bool SCD30Component::force_recalibration_with_reference(uint16_t co2_reference) {
  ESP_LOGD(TAG, "Performing CO2 force recalibration with reference %dppm.", co2_reference);
  if (this->write_command(SCD30_CMD_FORCED_CALIBRATION, co2_reference)) {
    ESP_LOGD(TAG, "Force recalibration complete.");
    return true;
  } else {
    ESP_LOGE(TAG, "Failed to force recalibration with reference.");
    this->error_code_ = FORCE_RECALIBRATION_FAILED;
    this->status_set_warning();
    return false;
  }
}

uint16_t SCD30Component::get_forced_calibration_reference() {
  uint16_t forced_calibration_reference;
  // Get current CO2 calibration
  if (!this->get_register(SCD30_CMD_FORCED_CALIBRATION, forced_calibration_reference)) {
    ESP_LOGE(TAG, "Unable to read forced calibration reference.");
  }
  return forced_calibration_reference;
}

}  // namespace scd30
}  // namespace esphome
