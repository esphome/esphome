#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "Wiegand.h"

namespace esphome {

namespace wiegand_reader {

class WiegandReaderTrigger;

class WiegandReader : public PollingComponent {
 public:
  void setup() override;
  void set_data_pins(GPIOPin *pin_d0, GPIOPin *pin_d1);
  void dump_config() override;

  void update() override;
  float get_setup_priority() const override;

  void register_trigger(WiegandReaderTrigger *trig) { this->triggers_.push_back(trig); }

  Wiegand wiegand_;
  GPIOPin *pin_d0_;
  GPIOPin *pin_d1_;
  std::vector<WiegandReaderTrigger *> triggers_;
 protected:

};


class WiegandReaderTrigger : public Trigger<std::string> {
 public:
  void process(String tag);
};

}  // namespace pn532
}  // namespace esphome
