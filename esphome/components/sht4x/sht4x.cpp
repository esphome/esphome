#include "sht4x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sht4x {

static const char *const TAG = "sht4x";

static const uint8_t MEASURECOMMANDS[] = {0xFD, 0xF6, 0xE0};
static const uint8_t SERIAL_NUMBER_COMMAND = 0x89;

// Conversion constants from SHT4x datasheet
static constexpr float TEMPERATURE_OFFSET = -45.0f;
static constexpr float TEMPERATURE_SPAN = 175.0f;
static constexpr float HUMIDITY_OFFSET = -6.0f;
static constexpr float HUMIDITY_SPAN = 125.0f;
static constexpr float RAW_MAX = 65535.0f;

void SHT4XComponent::read_serial_number_() {
  uint16_t buffer[2];
  if (!this->get_8bit_register(SERIAL_NUMBER_COMMAND, buffer, 2, 1)) {
    ESP_LOGE(TAG, "Get serial number failed");
    this->serial_number_ = 0;
    return;
  }
  this->serial_number_ = (uint32_t(buffer[0]) << 16) | (uint32_t(buffer[1]));
  ESP_LOGD(TAG, "Serial number: %08" PRIx32, this->serial_number_);
}

void SHT4XComponent::setup() {
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }

  this->read_serial_number_();

  if (std::isfinite(this->duty_cycle_) && this->duty_cycle_ > 0.0f) {
    this->heater_interval_ = static_cast<uint32_t>(static_cast<uint16_t>(this->heater_time_) / this->duty_cycle_);
    ESP_LOGD(TAG, "Heater interval: %" PRIu32, this->heater_interval_);

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
  }
}

void SHT4XComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SHT4x:\n"
                "  Serial number: %08" PRIx32,
                this->serial_number_);

  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  if (this->serial_number_ == 0) {
    ESP_LOGW(TAG, "Get serial number failed");
  }
}

void SHT4XComponent::update() {
  // Send command
  if (!this->write_command(MEASURECOMMANDS[this->precision_])) {
    // Warning will be printed only if warning status is not set yet
    this->status_set_warning(LOG_STR("Failed to send measurement command"));
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
      float temp = TEMPERATURE_OFFSET + TEMPERATURE_SPAN * static_cast<float>(buffer[0]) / RAW_MAX;
      this->temp_sensor_->publish_state(temp);
    }

    if (this->humidity_sensor_ != nullptr) {
      // Relative humidity is in the second result word
      float rh = HUMIDITY_OFFSET + HUMIDITY_SPAN * static_cast<float>(buffer[1]) / RAW_MAX;
      this->humidity_sensor_->publish_state(rh);
    }

    // Fire heater after measurement to maximize cooldown time before the next reading.
    // The heater command produces a measurement that we don't need (datasheet 4.9).
    if (this->heater_interval_ > 0) {
      uint32_t now = millis();
      if (now - this->last_heater_millis_ >= this->heater_interval_) {
        ESP_LOGD(TAG, "Heater turning on");
        if (this->write_command(this->heater_command_)) {
          this->last_heater_millis_ = now;
        }
      }
    }
  });
}

}  // namespace esphome::sht4x
