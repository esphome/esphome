#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/log.h"
#include "esphome/components/opentherm/hub.h"

namespace esphome {
namespace opentherm {

class AbstractOpenthermSwitch : public switch_::Switch, public Component, public MessageProcessor {
 protected:
  void write_state(bool state) override;

 public:
  void setup() override;
  void dump_config() override;
};

template<typename T> class OpenthermSwitch : public AbstractOpenthermSwitch {
 public:
  void prepare_data_out(OpenthermData &data) const override {
    data.type = MessageType::WRITE_DATA;
    T::set(data, this->state);
  }

  const char *get_type_name() const override { return "switch"; }
};

}  // namespace opentherm
}  // namespace esphome
