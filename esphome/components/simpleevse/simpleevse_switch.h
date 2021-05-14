#pragma once

#include "esphome/components/switch/switch.h"
#include "simplesevse.h"

namespace esphome {
namespace simpleevse {

class SimpleEvseChargingSwitch : public switch_::Switch, public UpdateListener {
  public:
    explicit SimpleEvseChargingSwitch(SimpleEvseComponent *parent) : parent_(parent) { parent->add_observer(this); }
    void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register);

  protected:
    void write_state(bool state) override;

    SimpleEvseComponent *const parent_;
    uint16_t active_writes_{0};
};

}  // namespace simpleevse
}  // namespace esphome
