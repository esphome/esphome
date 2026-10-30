#include "vbus_sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::vbus {

static inline uint16_t get_u16(std::vector<uint8_t> &message, int start) {
  return encode_value_le<uint16_t>(&message[start]);
}

static inline int16_t get_i16(std::vector<uint8_t> &message, int start) {
  return encode_value_le<int16_t>(&message[start]);
}

void DeltaSolBSPlusSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  PUBLISH_STATE(this->pump_speed2_sensor_, message[9]);
  PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 16));
  PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 18));
  PUBLISH_STATE(this->heat_quantity_sensor_,
                get_u16(message, 20) + get_u16(message, 22) * 1000.0f + get_u16(message, 24) * 1000000.0f);
  PUBLISH_STATE(this->time_sensor_, get_u16(message, 12));
  PUBLISH_STATE(this->version_sensor_, get_u16(message, 26) * 0.01f);
}

void DeltaSolBS2009Sensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  PUBLISH_STATE(this->pump_speed2_sensor_, message[12]);
  PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 10));
  PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 18));
  PUBLISH_STATE(this->heat_quantity_sensor_, get_u16(message, 28) + get_u16(message, 30) * 1000);
  PUBLISH_STATE(this->time_sensor_, get_u16(message, 22));
  PUBLISH_STATE(this->version_sensor_, get_u16(message, 32) * 0.01f);
}

void DeltaSolCSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  PUBLISH_STATE(this->pump_speed2_sensor_, message[9]);
  PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 12));
  PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 14));
  PUBLISH_STATE(this->heat_quantity_sensor_,
                get_u16(message, 16) + get_u16(message, 18) * 1000.0f + get_u16(message, 20) * 1000000.0f);
  PUBLISH_STATE(this->time_sensor_, get_u16(message, 22));
}

void DeltaSolCS2Sensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  PUBLISH_STATE(this->pump_speed_sensor_, message[12]);
  PUBLISH_STATE(this->operating_hours_sensor_, get_u16(message, 14));
  PUBLISH_STATE(this->heat_quantity_sensor_,
                (static_cast<uint32_t>(get_u16(message, 26)) << 16) | get_u16(message, 24));
  PUBLISH_STATE(this->version_sensor_, get_u16(message, 28) * 0.01f);
}

void DeltaSolCS4Sensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  PUBLISH_STATE(this->temperature5_sensor_, get_i16(message, 36) * 0.1f);
  PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  PUBLISH_STATE(this->pump_speed2_sensor_, message[12]);
  PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 10));
  PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 14));
  PUBLISH_STATE(this->heat_quantity_sensor_,
                (static_cast<uint32_t>(get_u16(message, 30)) << 16) | get_u16(message, 28));
  PUBLISH_STATE(this->time_sensor_, get_u16(message, 22));
  PUBLISH_STATE(this->version_sensor_, get_u16(message, 32) * 0.01f);
  PUBLISH_STATE(this->flow_rate_sensor_, get_u16(message, 38));
}

void DeltaSolCSPlusSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  PUBLISH_STATE(this->temperature5_sensor_, get_i16(message, 36) * 0.1f);
  PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  PUBLISH_STATE(this->pump_speed2_sensor_, message[12]);
  PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 10));
  PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 14));
  PUBLISH_STATE(this->heat_quantity_sensor_,
                (static_cast<uint32_t>(get_u16(message, 30)) << 16) | get_u16(message, 28));
  PUBLISH_STATE(this->time_sensor_, get_u16(message, 22));
  PUBLISH_STATE(this->version_sensor_, get_u16(message, 32) * 0.01f);
  PUBLISH_STATE(this->flow_rate_sensor_, get_u16(message, 38));
}

void DeltaSolBS2Sensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->temperature1_sensor_, get_i16(message, 0) * 0.1f);
  PUBLISH_STATE(this->temperature2_sensor_, get_i16(message, 2) * 0.1f);
  PUBLISH_STATE(this->temperature3_sensor_, get_i16(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature4_sensor_, get_i16(message, 6) * 0.1f);
  PUBLISH_STATE(this->pump_speed1_sensor_, message[8]);
  PUBLISH_STATE(this->pump_speed2_sensor_, message[9]);
  PUBLISH_STATE(this->operating_hours1_sensor_, get_u16(message, 12));
  PUBLISH_STATE(this->operating_hours2_sensor_, get_u16(message, 14));
  PUBLISH_STATE(this->heat_quantity_sensor_,
                get_u16(message, 16) + get_u16(message, 18) * 1000.0f + get_u16(message, 20) * 1000000.0f);
  PUBLISH_STATE(this->version_sensor_, get_u16(message, 24) * 0.01f);
}

void VBusCustomSensor::handle_message(std::vector<uint8_t> &message) {
  for (VBusCustomSubSensor *sensor : this->sensors_)
    sensor->parse_message(message);
}

void VBusCustomSubSensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace esphome::vbus
