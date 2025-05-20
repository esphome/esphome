#include "sht4x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sht4x {

static const char *const TAG = "sht4x";

static const uint8_t MEASURECOMMANDS[] = {0xFD, 0xF6, 0xE0};

void SHT4XComponent::start_heater_() {
  uint8_t cmd[] = {MEASURECOMMANDS[this->heater_command_]};

  ESP_LOGD(TAG, "Heater turning on");
  if (this->write(cmd, 1) != i2c::ERROR_OK) {
    this->status_set_error("Failed to turn on heater");
  }
}

void SHT4XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sht4x...");

  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }

  if (std::isfinite(this->duty_cycle_) && this->duty_cycle_ > 0.0f) {
    uint32_t heater_interval = static_cast<uint32_t>(static_cast<uint16_t>(this->heater_time_) / this->duty_cycle_);
    ESP_LOGD(TAG, "Heater interval: %" PRIu32, heater_interval);

    if (this->heater_power_ == SHT4X_HEATERPOWER_HIGH) {
      if (this->heater_time_ == SHT4X_HEATERTIME_LONG) {
        this->heater_command_ = 0x39;
      } else {
        this->heater_command_ = 0x32;
      }
    } else if (this->heater_power_ == SHT4X_HEATERPOWER_MED) {
      if (this->heater_time_ == SHT4X_HEATERTIME_LONG) {
        this->heater_command_ = 0x2F;
      } else {
        this->heater_command_ = 0x24;
      }
    } else {
      if (this->heater_time_ == SHT4X_HEATERTIME_LONG) {
        this->heater_command_ = 0x1E;
      } else {
        this->heater_command_ = 0x15;
      }
    }
    ESP_LOGD(TAG, "Heater command: %x", this->heater_command_);

    this->set_interval(heater_interval, std::bind(&SHT4XComponent::start_heater_, this));
  }
}

void SHT4XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SHT4x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SHT4x failed!");
  }
}

void SHT4XComponent::update() {
  // Send command
  if (!this->write_command(MEASURECOMMANDS[this->precision_])) {
    // Warning will be printed only if warning status is not set yet
    this->status_set_warning("Failed to send measurement command");
    return;
  }

  this->set_timeout(10, [this]() {
    uint16_t buffer[2];

    // Read measurement
    if (!this->read_data(buffer, 2)) {
      // Using ESP_LOGW to force the warning to be printed
      ESP_LOGW(TAG, "Sensor read failed");
      this->status_set_warning();
      return;
    }

    this->status_clear_warning();

    // Evaluate and publish measurements
    if (this->temp_sensor_ != nullptr) {
      // Temp is contained in the first result word
      float sensor_value_temp = buffer[0];
      float temp = -45 + 175 * sensor_value_temp / 65535;

      this->temp_sensor_->publish_state(temp);
    }

    if (this->humidity_sensor_ != nullptr) {
      // Relative humidity is in the second result word
      float sensor_value_rh = buffer[1];
      float rh = -6 + 125 * sensor_value_rh / 65535;

      this->humidity_sensor_->publish_state(rh);
    }
  });
}

}  // namespace sht4x
}  // namespace esphome
