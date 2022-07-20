#pragma once

#include "esphome/core/component.h"
#include "esphome/components/input_text/input_text.h"

namespace esphome {
namespace copy {

class CopyInputText : public input_text::InputText, public Component {
 public:
  void set_source(input_text::InputText *source) { source_ = source; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const std::string &value) override;

  input_text::InputText *source_;
};

}  // namespace copy
}  // namespace esphome
