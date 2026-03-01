#pragma once
#include "decoder.h"

namespace esphome {
namespace bthome {

class BTHomeObjectHandler {
 public:
  void set_object_type(BTHomeObjectType object_type) { this->object_type_ = object_type; }
  virtual bool process_object(const BTHomeObject &object) = 0;

 protected:
  BTHomeObjectType object_type_{};
};

}  // namespace bthome
}  // namespace esphome
