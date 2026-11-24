#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "esphome/core/defines.h"
#include "zigbee_attribute_base_esp32.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include "esp_zigbee.h"
#include "zigbee_esp32.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::zigbee {

class ZigbeeAttribute final : public ZigbeeAttributeBase {
 public:
  ZigbeeAttribute(ZigbeeComponent *parent, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                  uint8_t attr_type, float scale, uint8_t max_size)
      : ZigbeeAttributeBase(parent, endpoint_id, cluster_id, role, attr_id, attr_type, max_size), scale_(scale) {}

  template<typename F> void add_on_value_callback(F &&callback) { on_value_callback_.add(std::forward<F>(callback)); }
  void on_value(ezb_zcl_attribute_t attribute) override {
    if (attribute.data.type == this->attr_type() && attribute.data.value) {
      this->on_value_callback_.call(attribute);
    }
  }

#ifdef USE_SENSOR
  template<typename T> void connect(sensor::Sensor *sensor);
  template<typename T, typename F> void connect(sensor::Sensor *sensor, F &&f);
#endif
#ifdef USE_BINARY_SENSOR
  template<typename T> void connect(binary_sensor::BinarySensor *sensor);
#endif
#ifdef USE_SWITCH
  template<typename T> void connect(switch_::Switch *device);
#endif
 protected:
  float scale_;
  LazyCallbackManager<void(ezb_zcl_attribute_t attribute)> on_value_callback_{};
};

#ifdef USE_SENSOR
template<typename T> void ZigbeeAttribute::connect(sensor::Sensor *sensor) {
  sensor->add_on_state_callback([this](float value) { this->set_attr(this->scale_value_<T>(value, this->scale_)); });
}
template<typename T, typename F> void ZigbeeAttribute::connect(sensor::Sensor *sensor, F &&f) {
  sensor->add_on_state_callback([f = std::forward<F>(f), this](float value) { this->set_attr((T) f(value)); });
}
#endif
#ifdef USE_BINARY_SENSOR
template<typename T> void ZigbeeAttribute::connect(binary_sensor::BinarySensor *sensor) {
  sensor->add_on_state_callback([this](bool value) { this->set_attr((T) (this->scale_ * value)); });
}
#endif
#ifdef USE_SWITCH
template<typename T> void ZigbeeAttribute::connect(switch_::Switch *device) {
  this->add_on_value_callback(
      [device](ezb_zcl_attribute_t attribute) { device->control(*(T *) attribute.data.value); });
  device->add_on_state_callback([this](bool value) { this->set_attr((T) (this->scale_ * value)); });
}
#endif

}  // namespace esphome::zigbee

#endif
#endif
