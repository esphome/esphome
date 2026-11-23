#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "esphome/core/defines.h"
#include "zigbee_entity_base_esp32.h"

#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include "esp_zigbee.h"
#include "zigbee_esp32.h"

namespace esphome::zigbee {

class ZigbeeAttributeBase : public ZigbeeEntityBase {
 public:
  ZigbeeAttributeBase(ZigbeeComponent *parent, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                      uint8_t attr_type, uint8_t max_size)
      : ZigbeeEntityBase(parent, endpoint_id),
        cluster_id_(cluster_id),
        role_(role),
        attr_id_(attr_id),
        attr_type_(attr_type),
        max_size_(max_size) {}
  ZigbeeAttributeBase(ZigbeeComponent *parent, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                      uint8_t attr_type)
      : ZigbeeEntityBase(parent, endpoint_id),
        cluster_id_(cluster_id),
        role_(role),
        attr_id_(attr_id),
        attr_type_(attr_type) {}

  template<typename T> void add_attr(uint8_t attr_access, T value);
  template<typename T> void set_attr(const T &value);
  uint8_t attr_type() { return attr_type_; }

 protected:
  void set_attr_internal() override;
  void report_internal(bool has_lock) override;
  uint16_t cluster_id_;
  uint8_t role_;
  uint16_t attr_id_;
  uint8_t attr_type_;
  uint8_t max_size_{0};
  void *value_p_{nullptr};
  template<typename T> T scale_value_(float value, float scale);
  template<typename T> T invalid_value_();
};

template<typename T> void ZigbeeAttributeBase::add_attr(uint8_t attr_access, T value) {
  // Attribute type does never change and add_attr is only called once during startup, so this is safe.
  // For now we need to support only simple numeric/bool types for (binary) sensors.
  // For strings and arrays we would need to allocate a buffer of the maximum size.
  this->value_p_ = (void *) (new T);
  this->zb_->add_attr(this, this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_, this->max_size_,
                      this->attr_type_, attr_access, std::move(value));
}

template<typename T> void ZigbeeAttributeBase::set_attr(const T &value) {
  *static_cast<T *>(this->value_p_) = value;
  this->set_attr_requested_ = true;
  this->enable_loop();
}

template<typename T> T ZigbeeAttributeBase::scale_value_(float value, float scale) {
  static_assert(sizeof(T) <= 2 || std::is_floating_point_v<T>);
  if constexpr (std::is_integral<T>::value) {
    const float scaled = scale * value;
    if (std::isnan(value) || scaled < static_cast<float>(std::numeric_limits<T>::lowest()) ||
        scaled > static_cast<float>(std::numeric_limits<T>::max())) {
      return this->invalid_value_<T>();  // 0x8000 / 0xFFFF / 0 for bitmaps
    }
    return static_cast<T>(lroundf(scaled));
  }
  return static_cast<T>(scale * value);
}

template<typename T> T ZigbeeAttributeBase::invalid_value_() {
  if constexpr (std::is_integral_v<T>) {
    if constexpr (std::is_signed_v<T>) {
      // For signed integer types, NaN is represented by the minimum value
      return static_cast<T>(std::numeric_limits<T>::min());
    }

    if (this->attr_type_ >= EZB_ZCL_ATTR_TYPE_UINT8 && this->attr_type_ <= EZB_ZCL_ATTR_TYPE_ENUM16) {
      // For unsigned integer types and enum, NaN is represented by the maximum value
      return static_cast<T>(std::numeric_limits<T>::max());
    }

    // For other integer types, return 0 as a fallback
    return static_cast<T>(0);
  }

  return std::numeric_limits<T>::quiet_NaN();  // For floating-point types, return NaN
}

}  // namespace esphome::zigbee

#endif
#endif
