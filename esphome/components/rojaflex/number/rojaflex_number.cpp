#include "rojaflex_number.h"

namespace esphome::rojaflex {

void RojaflexNumber::setup() {
  if (this->parent_ == nullptr) {
    return;
  }
  switch (this->type_) {
    case RojaflexNumberType::TX_REPETITIONS:
      this->publish_state(this->parent_->get_tx_repetitions());
      break;
  }
}

void RojaflexNumber::control(float value) {
  if (this->parent_ == nullptr) {
    return;
  }
  const auto rounded = static_cast<uint8_t>(value);
  switch (this->type_) {
    case RojaflexNumberType::TX_REPETITIONS:
      this->parent_->set_tx_repetitions_runtime(rounded);
      this->publish_state(this->parent_->get_tx_repetitions());
      break;
  }
}

}  // namespace esphome::rojaflex
