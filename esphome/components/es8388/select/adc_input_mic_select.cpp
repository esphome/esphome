#include "adc_input_mic_select.h"

namespace esphome {
namespace es8388 {

void ADCInputMicSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_adc_input_mic(static_cast<AdcInputMicLine>(this->index_of(value).value()));
}

}  // namespace es8388
}  // namespace esphome
