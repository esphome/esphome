#pragma once

#include <cstdint>

#include "esphome/components/button/button.h"
#include "esphome/components/output/binary_output.h"

namespace esphome::output {

/// Turn the output on and off again after duration ms; a second press restarts the wait.
void press_output_button(button::Button *button, BinaryOutput *output, uint32_t duration);

}  // namespace esphome::output
