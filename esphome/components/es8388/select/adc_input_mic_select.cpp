#include "adc_input_mic_select.h"

namespace esphome::es8388 {

void ADCInputMicSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_adc_input_mic(static_cast<AdcInputMicLine>(index));
}

}  // namespace esphome::es8388
