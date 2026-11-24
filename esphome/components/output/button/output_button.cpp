#include "output_button.h"
#include "esphome/core/application.h"

namespace esphome::output {

void press_output_button(button::Button *button, BinaryOutput *output, uint32_t duration) {
  output->turn_on();
  // Keyed on the button, so a second press replaces the pending reset instead of stacking another
  App.scheduler.set_timeout(button, duration, [output]() { output->turn_off(); });
}

}  // namespace esphome::output
