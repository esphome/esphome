#include "smt100.h"
#include "esphome/core/log.h"

namespace esphome {
namespace smt100 {

static const char *const TAG = "smt100";

void SMT100Component::set_temperature_sensor(sensor::Sensor *temperature_sensor) {
  temperature_sensor_ = temperature_sensor;
}
void SMT100Component::set_moisture_sensor(sensor::Sensor *moisture_sensor) { moisture_sensor_ = moisture_sensor; }

void SMT100Component::loop() {
  const uint32_t now = millis();

  if (now - this->boot_up_time_ < SMT_BOOT_MS) {
    if (this->boot_up_time_ == 0) {
      this->boot_up_time_ = now;
    }
    return;
  }

  if (now - this->last_update_ > this->update_interval_) {
    this->write_str("GetAllMeasurements!\r");
    this->last_update_ = now;
  }

  if (this->available() == 0)
    return;

  static char buffer[MAX_LINE_LENGTH];
  while (available()) {
    if (readline(read(), buffer, MAX_LINE_LENGTH) > 0) {
      int counts = (int) atoi((strtok(buffer, ",")));
      float dielectric_constant = (float) atof((strtok(NULL, ",")));

      if (this->moisture_sensor_ != nullptr) {
        float moisture = (float) atof((strtok(NULL, ",")));
        moisture_sensor_->publish_state(moisture);
      }

      if (this->temperature_sensor_ != nullptr) {
        float temperature = (float) atof((strtok(NULL, ",")));
        temperature_sensor_->publish_state(temperature);
      }

      float voltage = (float) atof((strtok(NULL, ",")));
    }
  }
}

void SMT100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SMT100:");

  LOG_SENSOR(TAG, "Temperature", this->temperature_sensor_);
  LOG_SENSOR(TAG, "Water content", this->moisture_sensor_);
  this->check_uart_settings(9600);
}

int SMT100Component::readline(int readch, char *buffer, int len) {
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
