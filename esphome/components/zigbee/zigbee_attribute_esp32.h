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
#ifdef USE_COVER
#include "esphome/components/cover/cover.h"
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
#ifdef USE_COVER
  template<typename T> void connect(cover::Cover *device);
  void setup() override;
  template<typename F> void add_on_command_callback(F &&callback) {
    this->on_command_callback_.add(std::forward<F>(callback));
  }
  void on_command(uint8_t cmd_id, uint8_t percentage) { this->on_command_callback_.call(cmd_id, percentage); }
#endif

 protected:
  float scale_;
  LazyCallbackManager<void(ezb_zcl_attribute_t attribute)> on_value_callback_{};
#ifdef USE_COVER
  LazyCallbackManager<void(uint8_t cmd_id, uint8_t percentage)> on_command_callback_{};
  cover::Cover *cover_{nullptr};
#endif
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
#ifdef USE_COVER
template<typename T> void ZigbeeAttribute::connect(cover::Cover *device) {
  this->cover_ = device;
  this->add_on_command_callback([device](uint8_t cmd_id, uint8_t percentage) {
    auto call = device->make_call();
    switch (cmd_id) {
      case EZB_ZCL_CMD_WINDOW_COVERING_UP_OPEN_ID:
        call.set_command_open();
        break;
      case EZB_ZCL_CMD_WINDOW_COVERING_DOWN_CLOSE_ID:
        call.set_command_close();
        break;
      case EZB_ZCL_CMD_WINDOW_COVERING_STOP_ID:
        call.set_command_stop();
        break;
      case EZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE_ID:
        call.set_position(1.0f - static_cast<float>(percentage) / 100.0f);
        break;
      default:
        return;
    }
    call.perform();
  });
}

inline void ZigbeeAttribute::setup() {
  if (this->cover_ == nullptr) {
    return;
  }
  if (!this->cover_->get_traits().get_supports_position()) {
    // Without a position only open, close and stop are offered.
    this->zb_->remove_attr(this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_);
    this->report_enabled = false;
    constexpr uint8_t config_status =
        EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_OPERATIONAL | EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_ONLINE;
    this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_,
                        EZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID, 0, EZB_ZCL_ATTR_TYPE_MAP8, 0, config_status);
    return;
  }
  auto publish_position = [this]() {
    if (std::isnan(this->cover_->position)) {
      return;
    }
    this->set_attr(static_cast<uint8_t>(lroundf((1.0f - this->cover_->position) * 100.0f)));
  };
  this->cover_->add_on_state_callback(publish_position);
  publish_position();
}
#endif

}  // namespace esphome::zigbee

#endif
#endif
