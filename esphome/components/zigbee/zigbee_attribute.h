#pragma once

#include <type_traits>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include "esp_zigbee_core.h"
#include "zigbee.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::zigbee {

class ZigBeeAttribute : public Component {
 public:
  ZigBeeAttribute(ZigBeeComponent *parent, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                  uint8_t attr_type, float scale, uint8_t max_size)
      : zb_(parent),
        endpoint_id_(endpoint_id),
        cluster_id_(cluster_id),
        role_(role),
        attr_id_(attr_id),
        attr_type_(attr_type),
        scale_(scale),
        max_size_(max_size) {}
  void loop() override;

  template<typename T> void add_attr(uint8_t attr_access, T value);
  void set_report(bool force);
  void report();
  template<typename T> void set_attr(const T &value);

  uint8_t attr_type() { return attr_type_; }

  void add_on_value_callback(std::function<void(esp_zb_zcl_attribute_t attribute)> callback) {
    on_value_callback_.add(std::move(callback));
  }
  void on_value(esp_zb_zcl_attribute_t attribute) { this->on_value_callback_.call(attribute); }

#ifdef USE_BINARY_SENSOR
  template<typename T> void connect(binary_sensor::BinarySensor *sensor);
#endif

 protected:
  void set_attr_();
  void report_();
  void report_(bool has_lock);
  ZigBeeComponent *zb_;
  uint8_t endpoint_id_;
  uint16_t cluster_id_;
  uint8_t role_;
  uint16_t attr_id_;
  uint8_t attr_type_;
  uint8_t max_size_;
  float scale_;
  CallbackManager<void(esp_zb_zcl_attribute_t attribute)> on_value_callback_{};
  void *value_p_{nullptr};
  bool set_attr_requested_{false};
  bool report_requested_{false};
  bool force_report_{false};
};

template<typename T> void ZigBeeAttribute::add_attr(uint8_t attr_access, T value) {
  this->zb_->add_attr(this, this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_, this->attr_type_,
                      attr_access, this->max_size_, std::move(value));
}

template<typename T> void ZigBeeAttribute::set_attr(const T &value) {
  if constexpr (std::is_convertible<T, const char *>::value) {
    auto zcl_str = get_zcl_string(value, this->max_size_);

    if (this->value_p_ != nullptr) {
      delete[](char *) this->value_p_;
    }
    this->value_p_ = (void *) zcl_str;
  } else if constexpr (std::is_same<T, std::string>::value) {
    auto zcl_str = get_zcl_string(value.c_str(), this->max_size_);

    if (this->value_p_ != nullptr) {
      delete[](char *) this->value_p_;
    }
    this->value_p_ = (void *) zcl_str;
  } else {
    if (this->value_p_ != nullptr) {
      delete (T *) this->value_p_;
    }
    T *value_p = new T;
    *value_p = value;
    this->value_p_ = (void *) value_p;
  }
  this->set_attr_requested_ = true;
  this->enable_loop();
}

#ifdef USE_BINARY_SENSOR
template<typename T> void ZigBeeAttribute::connect(binary_sensor::BinarySensor *sensor) {
  sensor->add_on_state_callback([this](bool value) { this->set_attr((T) (this->scale_ * value)); });
}
#endif

}  // namespace esphome::zigbee

#endif
#endif
