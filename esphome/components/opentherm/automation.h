#pragma once

#include "esphome/core/automation.h"
#include "hub.h"
#include "opentherm.h"

namespace esphome {
namespace opentherm {

class BeforeSendTrigger : public Trigger<OpenthermData &> {
 public:
  BeforeSendTrigger(OpenthermHub *hub) {
    hub->add_on_before_send_callback([this](OpenthermData &x) { this->trigger(x); });
  }
};

class BeforeProcessResponseTrigger : public Trigger<OpenthermData &> {
 public:
  BeforeProcessResponseTrigger(OpenthermHub *hub) {
    hub->add_on_before_process_response_callback([this](OpenthermData &x) { this->trigger(x); });
  }
};

}  // namespace opentherm
}  // namespace esphome
