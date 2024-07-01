#pragma once
#include <map>
#include <string>
#include "esphome/components/homeassistant/HomeAssistantComponent.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace homeassistant_number {

class HomeAssistantNumber : public number::Number, public homeassistant::HomeAssistantComponent {
 private:
  void publish_api_state_(float state);
  void state_changed_(const std::string &state);
  void min_changed_(const std::string &min);
  void max_changed_(const std::string &max);
  void step_changed_(const std::string &step);
  void setup() override;
  void control(float value) override;
};
}  // namespace homeassistant_number
}  // namespace esphome
