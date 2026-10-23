#include "smt100.h"
#include "esphome/core/log.h"

namespace esphome::smt100 {

ESPHOME_LOG_TAG(TAG, "smt100");

void SMT100Component::update() {
  ESP_LOGV(TAG, "Sending measurement request");
  this->write_str("GetAllMeasurements!\r");
}

void SMT100Component::loop() {
  this->read_available([this](uint8_t readch) {
    if (readch == 0)
      return;
    char *line = this->line_reader_.feed(readch);
    this->line_reader_.log_if_overflowed(TAG);
    if (line != nullptr && line[0] != '\0') {
      char *token = strtok(line, ",");
      if (!token)
        return;
      int counts = (int) strtol(token, nullptr, 10);
      token = strtok(nullptr, ",");
      if (!token)
        return;
      float permittivity = (float) strtod(token, nullptr);
      token = strtok(nullptr, ",");
      if (!token)
        return;
      float moisture = (float) strtod(token, nullptr);
      token = strtok(nullptr, ",");
      if (!token)
        return;
      float temperature = (float) strtod(token, nullptr);
      token = strtok(nullptr, ",");
      if (!token)
        return;
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
  });
}

void SMT100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SMT100:");

  LOG_SENSOR(TAG, "Counts", this->counts_sensor_);
  LOG_SENSOR(TAG, "Permittivity", this->permittivity_sensor_);
  LOG_SENSOR(TAG, "Temperature", this->temperature_sensor_);
  LOG_SENSOR(TAG, "Moisture", this->moisture_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::smt100
