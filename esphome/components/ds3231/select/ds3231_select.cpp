#include "ds3231_select.h"

#ifdef USE_DS3231_SQUARE_WAVE

namespace esphome::ds3231 {

void DS3231OutputModeSelect::control(size_t index) {
  if (this->parent_->set_square_wave_output_enabled(index == 1)) {
    this->publish_state(index);
  }
}

void DS3231SquareWaveFrequencySelect::control(size_t index) {
  if (this->parent_->set_square_wave_frequency(static_cast<DS3231SquareWaveFrequency>(index))) {
    this->publish_state(index);
  }
}

}  // namespace esphome::ds3231

#endif  // USE_DS3231_SQUARE_WAVE
