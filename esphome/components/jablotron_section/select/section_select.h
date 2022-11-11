#pragma once
#include "esphome/components/jablotron/jablotron_device.h"
#include "esphome/components/jablotron/string_view.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace jablotron_section {

class SectionSelect : public select::Select, public jablotron::SectionDevice {
 public:
  void control(const std::string &value) override;
  void register_parent(jablotron::JablotronComponent &parent) override;
  void set_state(jablotron::StringView state) override;

 private:
  std::string last_value_;
};

}  // namespace jablotron_section
}  // namespace esphome
