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
