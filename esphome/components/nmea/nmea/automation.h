#pragma once

#include "nmea.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace nmea {

class NMEAOnUpdateTrigger : public Trigger<> {
 public:
  explicit NMEAOnUpdateTrigger(NMEAComponent *parent) {
    parent->add_on_update_callback([this]() { this->trigger(); });
  }
};

}  // namespace nmea
}  // namespace esphome
