#pragma once
#include "../remote_object.h"
#include "esphome/components/event/event.h"

#include <cstdint>

namespace esphome::bthome::client {

class BTHomeEvent : public BTHomeRemoteObject, public esphome::event::Event {
 public:
  /// Which object of this type in the payload, from 0: the button number minus one.
  void set_index(uint8_t index) { this->index_ = index; }
  bool process_object(const BTHomeObject &object) override;
  void log_config() override;

 protected:
  uint8_t index_{0};
};

}  // namespace esphome::bthome::client
