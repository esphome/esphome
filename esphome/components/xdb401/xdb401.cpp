#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "xdb401.h"

namespace esphome::xdb401 {

static const char *const TAG = "xdb401";

static const uint8_t REG_PRESSURE = 0x06;
static const uint8_t REG_TEMPERATURE = 0x09;
static const uint8_t REG_MAKE_MEASURE = 0x30;
static const uint8_t CMD_MAKE_MEASURE = 0x0A;
static const uint8_t MASK_MEASURE_READY = 0x08;
static const float CONVERT_PRESSURE = 8388608.0f;  // 0x800000

static const uint32_t CHECK_DELAY = 5;
static const uint8_t CHECK_ATTEMPTS = 6;
static const uint8_t MARK_FAIL_AFTER = 5;

void XDB401Component::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  uint8_t meas_resp[1] = {};
  i2c::ErrorCode err_code = this->read_register(REG_MAKE_MEASURE, meas_resp, sizeof(meas_resp));
  if (err_code != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("I2C communication failed"));
    return;
  }

  this->comm_err_counter_ = 0;
}

void XDB401Component::dump_config() {
  ESP_LOGCONFIG(TAG, "XDB401:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Pressure Range: %u bar", this->pressure_range_bar_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

void XDB401Component::handle_comm_failure_(const char *message) {
  this->status_set_warning(message);

  if (this->comm_err_counter_ >= MARK_FAIL_AFTER) {
    this->mark_failed(LOG_STR("Too many consecutive I2C communication errors"));
  } else {
    this->comm_err_counter_++;
  }

  this->measurement_in_progress_ = false;
}

i2c::ErrorCode XDB401Component::start_measurement_() {
  i2c::ErrorCode err_code = this->write_register(REG_MAKE_MEASURE, &CMD_MAKE_MEASURE, sizeof(CMD_MAKE_MEASURE));
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error starting measurement, code: %u", err_code);
    return err_code;
  }

  return i2c::ERROR_OK;
}

void XDB401Component::check_measurement_ready_(uint8_t attempt) {
  uint8_t meas_resp[1] = {};
  i2c::ErrorCode err_code = this->read_register(REG_MAKE_MEASURE, meas_resp, sizeof(meas_resp));
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading measurement status, code: %u", err_code);
    this->handle_comm_failure_("I2C communication failed");
    return;
  }

  ESP_LOGV(TAG, "Config response %02X", meas_resp[0]);

  // Bit 3 shall be 0 when measurement is ready
  if ((meas_resp[0] & MASK_MEASURE_READY) == 0) {
    ESP_LOGV(TAG, "Meas mode entered after %u ms", attempt * CHECK_DELAY);
    this->read_measurement_();
    return;
  }

  if (attempt >= CHECK_ATTEMPTS) {
    ESP_LOGE(TAG, "Device not in measurement mode after timeout of %u ms", CHECK_DELAY * CHECK_ATTEMPTS);
    this->handle_comm_failure_("Measurement timeout");
    return;
  }

  this->set_timeout(CHECK_DELAY, [this, attempt]() { this->check_measurement_ready_(attempt + 1); });
}

void XDB401Component::read_measurement_() {
  float temperature{};
  float pressure{};

  i2c::ErrorCode err_code = this->read_pressure_(pressure);
  if (err_code != i2c::ERROR_OK) {
    this->handle_comm_failure_("Could not read pressure data");
    return;
  }

  err_code = this->read_temperature_(temperature);
  if (err_code != i2c::ERROR_OK) {
    this->handle_comm_failure_("Could not read temperature data");
    return;
  }

  ESP_LOGD(TAG, "Got pressure=%.1f Pa, temperature=%.2f°C", pressure, temperature);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure);

  this->comm_err_counter_ = 0;
  this->status_clear_warning();
  this->measurement_in_progress_ = false;
}

i2c::ErrorCode XDB401Component::read_pressure_(float &pressure) {
  uint8_t p_data[3]{};
  i2c::ErrorCode err_code = this->read_register(REG_PRESSURE, p_data, 3);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading pressure register");
    return err_code;
  }
  char pressure_buf[format_hex_pretty_size(3)];
  format_hex_pretty_to(pressure_buf, sizeof(pressure_buf), p_data, 3);
  ESP_LOGV(TAG, "Got pressure data: %s", pressure_buf);

  // Sign-extend 24-bit big-endian pressure value to int32_t.
  int32_t raw_pressure = static_cast<int32_t>(encode_uint24(p_data[0], p_data[1], p_data[2]) << 8) >> 8;
  ESP_LOGD(TAG, "Pressure data raw %i", raw_pressure);

  pressure = (static_cast<float>(raw_pressure) / CONVERT_PRESSURE) *
             XDB401Component::full_scale_pressure_pa(this->pressure_range_bar_);

  return err_code;
}

i2c::ErrorCode XDB401Component::read_temperature_(float &temperature) {
  uint8_t t_data[2]{};
  i2c::ErrorCode err_code = this->read_register(REG_TEMPERATURE, t_data, 2);
  if (err_code != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading temperature register");
    return err_code;
  }

  char temperature_buf[format_hex_pretty_size(2)];
  format_hex_pretty_to(temperature_buf, sizeof(temperature_buf), t_data, 2);
  ESP_LOGV(TAG, "Got temperature data: %s", temperature_buf);

  // Temperature is a signed 16-bit big-endian value in 1/256 °C (Q8.8 fixed point).
  int16_t raw_temperature = static_cast<int16_t>(encode_uint16(t_data[0], t_data[1]));
  ESP_LOGD(TAG, "Temperature data raw %i", raw_temperature);

  temperature = static_cast<float>(raw_temperature) / 256.0f;

  return err_code;
}

void XDB401Component::update() {
  if (this->measurement_in_progress_) {
    ESP_LOGV(TAG, "Skipping update, measurement already in progress");
    return;
  }

  i2c::ErrorCode err_code = this->start_measurement_();
  if (err_code != i2c::ERROR_OK) {
    this->handle_comm_failure_("I2C communication failed");
    return;
  }

  this->measurement_in_progress_ = true;
  this->set_timeout(CHECK_DELAY, [this]() { this->check_measurement_ready_(1); });
}

}  // namespace esphome::xdb401
