#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace tuya {

class TuyaTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_sensor_id(uint8_t sensor_id) { this->sensor_id_ = sensor_id; }

  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }

 protected:
  Tuya *parent_;
  uint8_t sensor_id_{0};
};

// helper functions
uint8_t hexchar_to_int(const std::string& dp_data_string, const uint8_t index_in_string);
uint16_t hexpair_to_int(const std::string& dp_data_string, const uint8_t index_in_string);
uint32_t hexquad_to_int(const std::string& dp_data_string, const uint8_t index_in_string);
std::vector<uint8_t> raw_decode(const std::string& dp_data_string);
std::string raw_encode(const uint8_t *data, uint32_t len);
template<typename T> std::string raw_encode(const T &data) { return raw_encode(data.data(), data.size()); }

}  // namespace tuya
}  // namespace esphome
