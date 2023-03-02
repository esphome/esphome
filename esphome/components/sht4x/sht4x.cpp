#include "sht4x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sht4x {

static const char *const TAG = "sht4x";

static const uint8_t MEASURECOMMANDS[] = {0xFD, 0xF6, 0xE0};

static const char *precision_to_str(SHT4XPRECISION precision) {
  switch (precision) {
    case SHT4X_PRECISION_HIGH:
      return "High";
    case SHT4X_PRECISION_MED:
      return "Medium";
    case SHT4X_PRECISION_LOW:
      return "Low";
    default:
      return "UNKNOWN";
  }
}

static const char *heater_power_to_str(SHT4XHEATERPOWER heater_power) {
  switch (heater_power) {
    case SHT4X_HEATERPOWER_HIGH:
      return "High";
    case SHT4X_HEATERPOWER_MED:
      return "Medium";
    case SHT4X_HEATERPOWER_LOW:
      return "Low";
    default:
      return "UNKNOWN";
  }
}

static const char *heater_time_to_str(SHT4XHEATERTIME heater_time) {
  switch (heater_time) {
    case SHT4X_HEATERTIME_LONG:
      return "Long (1s)";
    case SHT4X_HEATERTIME_SHORT:
      return "Short (100ms)";
    default:
      return "UNKNOWN";
  }
}

void SHT4XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sht4x...");

  if (this->heater_period_ > 0) {
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
    ESP_LOGD(TAG, "Heater command: 0x%x", this->heater_command_);
    float duty = float(this->heater_time_) / (this->update_interval_ * this->heater_period_);
    ESP_LOGD(TAG, "Heater duty cycle: %.2f%%", duty * 100);
  }
}

void SHT4XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SHT4X");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this)
  ESP_LOGCONFIG(TAG, "  Precision: %s", precision_to_str(this->precision_));
  ESP_LOGCONFIG(TAG, "  Heater Period: %u", this->heater_period_);
  if (this->heater_period_ > 0) {
    ESP_LOGCONFIG(TAG, "  Heater Power: %s", heater_power_to_str(this->heater_power_));
    ESP_LOGCONFIG(TAG, "  Heater Time: %s", heater_time_to_str(this->heater_time_));
  }
  LOG_SENSOR("  ", "Temperature", this->temp_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void SHT4XComponent::update() {
  // Send command
  this->write_command(MEASURECOMMANDS[this->precision_]);

  this->set_timeout(10, [this]() {
    uint16_t buffer[2];

    // Read measurement
    bool read_status = this->read_data(buffer, 2);

    if (read_status) {
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
    } else {
      ESP_LOGD(TAG, "Sensor read failed");
    }

    if (this->heater_period_ > 0 && this->update_count_ % this->heater_period_ == 0) {
      ESP_LOGD(TAG, "Heater turning on");
      this->write_command(this->heater_command_);
    }
    this->update_count_++;
  });
}

}  // namespace sht4x
}  // namespace esphome
