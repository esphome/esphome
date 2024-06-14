#pragma once
#include <map>
#include <string>
#include "esphome/components/homeassistant/HomeAssistantComponent.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace homeassistant_number {

class HomeAssistantNumber : public number::Number, public homeassistant::HomeAssistantComponent {
 private:
  void publish_api_state(float state);
  void state_changed(std::string state);
  void min_changed(std::string min);
  void max_changed(std::string max);
  void step_changed(std::string step);
  void setup();
  void control(float value);
};
}  // namespace homeassistant_number
}  // namespace esphome
