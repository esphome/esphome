#pragma once
#include <cstddef>
#include <functional>

#include "decoder.h"
#include "encoder.h"
#include "helpers.h"

namespace esphome {
namespace bthome {
namespace server {

class BTHomeLocalBase {
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

class BTHomeLocalSensor : public BTHomeLocalBase {
 public:
  void set_source(sensor::Sensor *source) { this->source_ = source; }
  size_t get_encoded_size() const override {
    if (std::isnan(source_->get_state())) {
      return 0;  // Don't include in frame if state is not a number
    }

    return sizeof(BTHomeHeader) + get_bthome_value_length(this->object_type_);
  }
  bool write(BTHomeEncoder &encoder) const override {
    return encoder.write_float(this->object_type_, this->source_->state);
  }
  void register_immediate_callback(std::function<void()> &&callback) override {
    this->source_->add_on_state_callback([callback](float) { callback(); });
  }

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

class BTHomeLocalBinarySensor : public BTHomeLocalBase {
 public:
  void set_source(binary_sensor::BinarySensor *source) { this->source_ = source; }
  size_t get_encoded_size() const override {
    if (!source_->has_state()) {
      return 0;  // Don't include in frame if state is not valid
    }
    return sizeof(BTHomeHeader) + get_bthome_value_length(this->object_type_);
  }
  bool write(BTHomeEncoder &encoder) const override {
    return encoder.write_bool(this->object_type_, this->source_->state);
  }
  void register_immediate_callback(std::function<void()> &&callback) override {
    this->source_->add_on_state_callback([callback](bool) { callback(); });
  }

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

class BTHomeLocalTextSensor : public BTHomeLocalBase {
 public:
  void set_source(text_sensor::TextSensor *source) { this->source_ = source; }
  void set_max_length(size_t max_length) { this->max_length_ = max_length; }

  size_t get_encoded_size() const override {
    if (!this->source_->has_state())
      return 0;
    size_t content_len = std::min(this->source_->state.size(), this->max_length_);
    return 1 + 1 + content_len;  // type + length_byte + content
  }

  bool write(BTHomeEncoder &encoder) const override {
    return encoder.write_text(this->object_type_, this->source_->state.c_str(), this->source_->state.size(),
                              this->max_length_);
  }

  void register_immediate_callback(std::function<void()> &&callback) override {
    this->source_->add_on_state_callback([callback](const std::string &) { callback(); });
  }

 protected:
  text_sensor::TextSensor *source_{nullptr};
  size_t max_length_{5};
};

}  // namespace server
}  // namespace bthome
}  // namespace esphome

#endif
