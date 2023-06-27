#include "smt100.h"
#include "esphome/core/log.h"

namespace esphome {
namespace smt100 {

static const char *const TAG = "smt100";

void SMT100Component::update() {
  ESP_LOGV(TAG, "Sending measurement request");
  this->write_str("GetAllMeasurements!\r");
}

void SMT100Component::loop() {
  static char buffer[MAX_LINE_LENGTH];
  while (this->available() != 0) {
    if (readline_(read(), buffer, MAX_LINE_LENGTH) > 0) {
      int counts = (int) strtol((strtok(buffer, ",")), nullptr, 10);
      float dielectric_constant = (float) strtod((strtok(nullptr, ",")), nullptr);
      float moisture = (float) strtod((strtok(nullptr, ",")), nullptr);
      float temperature = (float) strtod((strtok(nullptr, ",")), nullptr);
      float voltage = (float) strtod((strtok(nullptr, ",")), nullptr);

      if (this->counts_sensor_ != nullptr) {
        counts_sensor_->publish_state(counts);
      }

      if (this->dielectric_constant_sensor_ != nullptr) {
        dielectric_constant_sensor_->publish_state(dielectric_constant);
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

float SMT100Component::get_setup_priority() const { return setup_priority::DATA; }

void SMT100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SMT100:");

  LOG_SENSOR(TAG, "Counts", this->temperature_sensor_);
  LOG_SENSOR(TAG, "Dielectric Constant", this->temperature_sensor_);
  LOG_SENSOR(TAG, "Temperature", this->temperature_sensor_);
  LOG_SENSOR(TAG, "Moisture", this->moisture_sensor_);
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(9600);
}

int SMT100Component::readline_(int readch, char *buffer, int len) {
  static int pos = 0;
  int rpos;

  if (readch > 0) {
    switch (readch) {
      case '\n':  // Ignore new-lines
        break;
      case '\r':  // Return on CR
        rpos = pos;
        pos = 0;  // Reset position index ready for next time
        return rpos;
      default:
        if (pos < len - 1) {
          buffer[pos++] = readch;
          buffer[pos] = 0;
        }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}

}  // namespace smt100
}  // namespace esphome
