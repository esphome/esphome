#pragma once

#include <cmath>
#include <type_traits>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

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

namespace esphome::zigbee {

enum ZigbeeReportT {
  ZIGBEE_REPORT_DEFAULT,
  ZIGBEE_REPORT_COORDINATOR,
  ZIGBEE_REPORT_ENABLE,
  ZIGBEE_REPORT_FORCE,
};

class ZigbeeAttribute final : public Component {
 public:
  ZigbeeAttribute(ZigbeeComponent *parent, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                  uint8_t attr_type, float scale, uint8_t max_size)
      : zb_(parent),
        endpoint_id_(endpoint_id),
        cluster_id_(cluster_id),
        role_(role),
        attr_id_(attr_id),
        attr_type_(attr_type),
        max_size_(max_size),
        scale_(scale) {}
  void loop() override;
  template<typename T> void add_attr(T value);
  template<typename T> void set_attr(const T &value);
  uint8_t attr_type() { return attr_type_; }
  void set_report(ZigbeeReportT report);
  template<typename T> float get_val_or_nan(float value);
#ifdef USE_SENSOR
  template<typename T> void connect(sensor::Sensor *sensor);
  template<typename T, typename F> void connect(sensor::Sensor *sensor, F &&f);
#endif
#ifdef USE_BINARY_SENSOR
  template<typename T> void connect(binary_sensor::BinarySensor *sensor);
#endif
  bool report_enabled = false;

 protected:
  void set_attr_();
  void report_(bool has_lock);
  ZigbeeComponent *zb_;
  uint8_t endpoint_id_;
  uint16_t cluster_id_;
  uint8_t role_;
  uint16_t attr_id_;
  uint8_t attr_type_;
  uint8_t max_size_;
  float scale_;
  void *value_p_{nullptr};
  bool set_attr_requested_{false};
  bool report_requested_{false};
  bool force_report_{false};
};

template<typename T> void ZigbeeAttribute::add_attr(T value) {
  // Attribute type does never change and add_attr is only called once during startup, so this is safe.
  // For now we need to support only simple numeric/bool types for (binary) sensors.
  // For strings and arrays we would need to allocate a buffer of the maximum size.
  this->value_p_ = (void *) (new T);
  this->zb_->add_attr(this, this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_, this->max_size_,
                      std::move(value));
}

template<typename T> void ZigbeeAttribute::set_attr(const T &value) {
  *static_cast<T *>(this->value_p_) = value;
  this->set_attr_requested_ = true;
  this->enable_loop();
}

template<typename T> float ZigbeeAttribute::get_val_or_nan(float value) {
  if constexpr (std::is_integral<T>::value) {
    if constexpr (std::is_signed<T>::value) {
      if (std::isnan(value)) {
        // For signed integer types, NaN is the minimum value
        return static_cast<float>(std::numeric_limits<T>::min());
      }
      return value;
    }

    if (value < 0.0f || std::isnan(value)) {
      if (this->attr_type_ >= EZB_ZCL_ATTR_TYPE_UINT8 && this->attr_type_ <= EZB_ZCL_ATTR_TYPE_INT64) {
        // For unsigned integer types, NaN is the maximum value
        return static_cast<float>(std::numeric_limits<T>::max());
      }
      // for other types like map, enum, etc., that also use uintN_t we can return 0.0f as a safe default
      return 0.0f;
    }

    return value;
  }

  // should never reach here for non-integral types, but just in case, return 0.0f for NaN
  if (std::isnan(value)) {
    return 0.0f;
  }
  return value;
}

#ifdef USE_SENSOR
template<typename T> void ZigbeeAttribute::connect(sensor::Sensor *sensor) {
  sensor->add_on_state_callback(
      [this](float value) { this->set_attr((T) (this->scale_ * (get_val_or_nan<T>(value)))); });
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

}  // namespace esphome::zigbee

#endif
#endif
