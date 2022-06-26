#include "fastled_bus_output.h"

namespace esphome {
namespace fastled_bus {

void Output::setup() {
  if (this->num_chips_ + this->ofs_ >= this->bus_->num_chips_) {
    ESP_LOGE("fastled:bus:output", "Number of chips (%d) + offset (%d) > (%d)", this->num_chips_, this->ofs_,
             this->bus_->num_chips_);
    return;
  }
  ESP_LOGCONFIG("fastled:bus:output", "Setting up FastLEDBusOutput...[%d:%d:%d:%d]", this->num_chips_, this->ofs_,
                this->channel_offset_, this->repeat_distance_);
}

void Output::write_state(float state) {
  uint8_t *chips = &this->bus_->chips()[this->ofs_ * this->bus_->chip_channels_];
  auto val = uint8_t(state * 255);
  for (uint16_t i = (uint16_t)(this->channel_offset_ / this->bus_->chip_channels_);
       i < this->num_chips_ * this->bus_->chip_channels_;
       i += (uint16_t)(this->repeat_distance_ / this->bus_->chip_channels_)) {
    chips[i + (this->channel_offset_ % this->bus_->chip_channels_)] = val;
  }
  this->bus_->schedule_refresh();
};

}  // namespace fastled_bus
}  // namespace esphome
