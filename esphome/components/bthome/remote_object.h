#pragma once
#include "decoder.h"

namespace esphome::bthome::client {

class BTHomeRemoteObject {
 public:
  void set_object_type(BTHomeObjectType object_type) { this->object_type_ = object_type; }
  virtual bool process_object(const BTHomeObject &object) = 0;

 protected:
  BTHomeObjectType object_type_{};
};

}  // namespace esphome::bthome::client
