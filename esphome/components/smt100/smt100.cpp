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

      PUBLISH_STATE(this->counts_sensor_, counts);

      PUBLISH_STATE(this->permittivity_sensor_, permittivity);

      PUBLISH_STATE(this->moisture_sensor_, moisture);

      PUBLISH_STATE(this->temperature_sensor_, temperature);

      PUBLISH_STATE(this->voltage_sensor_, voltage);
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
