#include "smt100.h"
#include "esphome/core/log.h"

namespace esphome::smt100 {

static const char *const TAG = "smt100";

void SMT100Component::update() {
  ESP_LOGV(TAG, "Sending measurement request");
  this->write_str("GetAllMeasurements!\r");
}

void SMT100Component::loop() {
  while (this->available() != 0) {
    if (this->readline_(this->read(), this->readline_buffer_, MAX_LINE_LENGTH) > 0) {
      char *token = strtok(this->readline_buffer_, ",");
      if (!token)
        continue;
      int counts = (int) strtol(token, nullptr, 10);
      token = strtok(nullptr, ",");
      if (!token)
        continue;
      float permittivity = (float) strtod(token, nullptr);
      token = strtok(nullptr, ",");
      if (!token)
        continue;
      float moisture = (float) strtod(token, nullptr);
      token = strtok(nullptr, ",");
      if (!token)
        continue;
      float temperature = (float) strtod(token, nullptr);
      token = strtok(nullptr, ",");
      if (!token)
        continue;
      float voltage = (float) strtod(token, nullptr);

      if (this->counts_sensor_ != nullptr) {
        counts_sensor_->publish_state(counts);
      }

      if (this->permittivity_sensor_ != nullptr) {
        permittivity_sensor_->publish_state(permittivity);
      }

      if (this->moisture_sensor_ != nullptr) {
        moisture_sensor_->publish_state(moisture);
      }

      if (this->temperature_sensor_ != nullptr) {
        temperature_sensor_->publish_state(temperature);
      }

      if (this->voltage_sensor_ != nullptr) {
        voltage_sensor_->publish_state(voltage);
      }
    }
  }
}

void SMT100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SMT100:");

  LOG_SENSOR(TAG, "Counts", this->counts_sensor_);
  LOG_SENSOR(TAG, "Permittivity", this->permittivity_sensor_);
  LOG_SENSOR(TAG, "Temperature", this->temperature_sensor_);
  LOG_SENSOR(TAG, "Moisture", this->moisture_sensor_);
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(9600);
}

int SMT100Component::readline_(int readch, char *buffer, int len) {
  int rpos;

  if (readch > 0) {
    switch (readch) {
      case '\n':  // Ignore new-lines
        break;
      case '\r':  // Return on CR
        rpos = this->readline_pos_;
        this->readline_pos_ = 0;  // Reset position index ready for next time
        return rpos;
      default:
        if (this->readline_pos_ < len - 1) {
          buffer[this->readline_pos_++] = readch;
          buffer[this->readline_pos_] = 0;
        }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}

}  // namespace esphome::smt100
