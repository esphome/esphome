#include "esphome/core/log.h"
#include "tuya_sensor.h"
#include <cinttypes>

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.sensor";

void TuyaSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.type == TuyaDatapointType::BOOLEAN) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %s", datapoint.id, ONOFF(datapoint.value_bool));
      this->publish_state(datapoint.value_bool);
    } else if (datapoint.type == TuyaDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %d", datapoint.id, datapoint.value_int);
      this->publish_state(datapoint.value_int);
    } else if (datapoint.type == TuyaDatapointType::ENUM) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %u", datapoint.id, datapoint.value_enum);
      this->publish_state(datapoint.value_enum);
    } else if (datapoint.type == TuyaDatapointType::BITMASK) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %" PRIx32, datapoint.id, datapoint.value_bitmask);
      this->publish_state(datapoint.value_bitmask);
    }
  });
}

void TuyaSensor::dump_config() {
  LOG_SENSOR("", "Tuya Sensor", this);
  ESP_LOGCONFIG(TAG, "  Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya
}  // namespace esphome
