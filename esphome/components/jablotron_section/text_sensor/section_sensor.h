#pragma once
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/jablotron/jablotron_device.h"
#include "esphome/components/jablotron/string_view.h"

namespace esphome {
namespace jablotron_section {

class SectionSensor : public text_sensor::TextSensor, public jablotron::SectionDevice {
 public:
  void set_state(jablotron::StringView value) override;

 protected:
  void register_parent(jablotron::JablotronComponent &parent) override;

 private:
  std::string last_value_;
};

}  // namespace jablotron_section
}  // namespace esphome
