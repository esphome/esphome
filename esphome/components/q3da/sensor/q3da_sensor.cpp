#include "esphome/core/log.h"
#include "q3da_sensor.h"
#include "../q3da_parser.h"

namespace esphome {
namespace q3da {

static const char *const TAG = "q3da_sensor";

Q3DASensor::Q3DASensor(std::string server_id, std::string obis_code)
    : Q3DAListener(std::move(server_id), std::move(obis_code)) {}

void Q3DASensor::publish_val(const ObisInfo &obis_info) {
  //if (obis_info.value != obis_info.value) {
  //  publish_state(obis_info.symbol);
  //} else {
  publish_state(obis_info.value);
  //}
  
  /*switch (obis_info.value_type) {
    case Q3DA_INT: {
      publish_state(bytes_to_int(obis_info.value));
      break;
    }
    case Q3DA_BOOL:
    case Q3DA_UINT: {
      publish_state(bytes_to_uint(obis_info.value));
      break;
    }
    case Q3DA_OCTET: {
      ESP_LOGW(TAG, "No number conversion for (%s) %s. Consider using Q3DA TextSensor instead.",
               bytes_repr(obis_info.server_id).c_str(), obis_info.code_repr().c_str());
      break;
    }
  }*/
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
