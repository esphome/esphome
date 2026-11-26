#pragma once

#include "esphome/components/button/button.h"

namespace esphome::homeassistant {

/// Ask Home Assistant to press entity_id, with input_button.press for an input_button helper.
void press_button_entity(button::Button *button, const char *entity_id);

}  // namespace esphome::homeassistant
