#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "Wiegand.h"

namespace esphome {
namespace wiegand_reader {

class WiegandReaderTrigger : public Trigger<std::string> {
 public:
  void process(String tag);
};

class WiegandReader : public PollingComponent {
 public:
  void setup() override;
  void set_data_pins(GPIOPin *pin_d0, GPIOPin *pin_d1);
  void dump_config() override;

  void update() override;
  float get_setup_priority() const override;

  void register_trigger(WiegandReaderTrigger *trig) { this->triggers_.push_back(trig); }

 protected:
  static void received_data_(uint8_t* data, uint8_t bits, WiegandReader *reader);
  static void pin_state_changed_(WiegandReader *reader);
  static void received_data_error_(Wiegand::DataError error, uint8_t* rawData, uint8_t rawBits, const char* message);

  Wiegand wiegand_;
  GPIOPin *pin_d0_;
  GPIOPin *pin_d1_;
  std::vector<WiegandReaderTrigger *> triggers_;
};

}  // namespace wiegand_reader
}  // namespace esphome
