#include "esphome/core/log.h"
#include "q3da_sensor.h"
#include "../q3da_parser.h"

namespace esphome {
namespace q3da {

static const char *const TAG = "q3da_sensor";

Q3DASensor::Q3DASensor(std::string server_id, std::string obis_code)
    : Q3DAListener(std::move(server_id), std::move(obis_code)) {}

void Q3DASensor::publish_val(const ObisInfo &obis_info) {
  publish_state(obis_info.value);
}

void Q3DASensor::dump_config() {
  LOG_SENSOR("", "Q3DA", this);
  if (!this->server_id.empty()) {
    ESP_LOGCONFIG(TAG, "  Server ID: %s", this->server_id.c_str());
  }
  ESP_LOGCONFIG(TAG, "  OBIS Code: %s", this->obis_code.c_str());
}

}  // namespace q3da
}  // namespace esphome
