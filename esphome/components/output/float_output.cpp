#include "float_output.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace output {

void FloatOutput::set_level(float state) {
  state = clamp(state, 0.0f, 1.0f);

  if (this->filter_list_ == nullptr) {
#ifdef USE_POWER_SUPPLY
    if (state > 0.0f) {  // ON
      this->power_.request();
    } else {  // OFF
      this->power_.unrequest();
    }
#endif
    this->write_state(state);
  } else {
    this->filter_list_->input(state);
  }
}

void FloatOutput::write_state(bool state) { this->set_level(state ? 1.0f : 0.0f); }

}  // namespace output
}  // namespace esphome
