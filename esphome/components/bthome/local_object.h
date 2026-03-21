#pragma once
#include <cstddef>
#include <functional>

#include "decoder.h"
#include "encoder.h"
#include "bthome.h"

namespace esphome {
namespace bthome {
namespace server {

class BTHomeLocalObject {
 public:
  void set_object_type(BTHomeObjectType type) { this->object_type_ = type; }
  BTHomeObjectType get_object_type() const { return this->object_type_; }
  void set_advertise_immediately(bool val) { this->advertise_immediately_ = val; }
  bool get_advertise_immediately() const { return this->advertise_immediately_; }
  virtual size_t get_encoded_size() const = 0;
  virtual bool write(BTHomeEncoder &encoder) const = 0;
  virtual void register_immediate_callback(std::function<void()> &&callback) = 0;

 protected:
  BTHomeObjectType object_type_{};
  bool advertise_immediately_{false};
};
}  // namespace server
}  // namespace bthome
}  // namespace esphome

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bthome {
namespace server {

class BTHomeLocalSensor : public BTHomeLocalObject {
 public:
  void set_source(sensor::Sensor *source) { this->source_ = source; }
  size_t get_encoded_size() const override;
  bool write(BTHomeEncoder &encoder) const override;
  void register_immediate_callback(std::function<void()> &&callback) override;

 protected:
  sensor::Sensor *source_{nullptr};
};
}  // namespace server
}  // namespace bthome
}  // namespace esphome

#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bthome {
namespace server {

class BTHomeLocalBinarySensor : public BTHomeLocalObject {
 public:
  void set_source(binary_sensor::BinarySensor *source) { this->source_ = source; }
  size_t get_encoded_size() const override;
  bool write(BTHomeEncoder &encoder) const override;
  void register_immediate_callback(std::function<void()> &&callback) override;

 protected:
  binary_sensor::BinarySensor *source_{nullptr};
};
}  // namespace server
}  // namespace bthome
}  // namespace esphome

#endif

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace bthome {
namespace server {

class BTHomeLocalTextSensor : public BTHomeLocalObject {
 public:
  void set_source(text_sensor::TextSensor *source) { this->source_ = source; }
  void set_max_length(size_t max_length) { this->max_length_ = max_length; }
  size_t get_encoded_size() const override;
  bool write(BTHomeEncoder &encoder) const override;
  void register_immediate_callback(std::function<void()> &&callback) override;

 protected:
  text_sensor::TextSensor *source_{nullptr};
  size_t max_length_{5};
};

}  // namespace server
}  // namespace bthome
}  // namespace esphome

#endif
