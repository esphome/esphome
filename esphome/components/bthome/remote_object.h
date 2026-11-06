#pragma once
#include "decoder.h"

namespace esphome::bthome::client {

class BTHomeRemoteObject {
 public:
  void set_object_type(BTHomeObjectType object_type) { this->object_type_ = object_type; }
  // Returning true consumes the object: no later handler sees it, and the next object is offered from the handler
  // after this one. A handler may read the object and return false to pass it on to the following handlers.
  virtual bool process_object(const BTHomeObject &object) = 0;
  // Logs this entity's configuration, as part of the hub's dump_config.
  virtual void log_config() = 0;

 protected:
  BTHomeObjectType object_type_{};
};

}  // namespace esphome::bthome::client
