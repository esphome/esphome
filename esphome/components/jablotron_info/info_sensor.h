#pragma once
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/jablotron/jablotron_device.h"

namespace esphome {
namespace jablotron_info {

class InfoSensor : public text_sensor::TextSensor, public jablotron::InfoDevice {
 public:
  void set_state(jablotron::StringView value) override;

 protected:
  void register_parent(jablotron::JablotronComponent &parent) override;
};

}  // namespace jablotron_info
}  // namespace esphome
