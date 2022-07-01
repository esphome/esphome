#include "fastled_bus_output.h"

namespace esphome {
namespace fastled_bus {

static void setup_mapping(Mapping *map) {
  if (map->num_chips_ + map->ofs_ >= map->bus_->num_chips_) {
    ESP_LOGE("fastled:bus:output", "Number of chips (%d) + offset (%d) > (%d)", map->num_chips_, map->ofs_,
             map->bus_->num_chips_);
    return;
  }
  ESP_LOGCONFIG("fastled:bus:output", "Setting up FastLEDBusOutput...[%d:%d:%d:%d]", map->num_chips_, map->ofs_,
                map->channel_offset_, map->repeat_distance_);
}

void Output::setup() {
  for (uint8_t i = 0; i < this->len_; i++) {
    setup_mapping(&this->mappings_[i]);
  }
}

static void write_mapping(Mapping *map, float state) {
  auto *chips = &(map->bus_->chips()[map->ofs_ * map->bus_->chip_channels_]);
  auto val = uint8_t(state * 255);
  for (uint16_t i = (uint16_t)(map->channel_offset_ / map->bus_->chip_channels_);
       i < map->num_chips_ * map->bus_->chip_channels_;
       i += (uint16_t)(map->repeat_distance_ / map->bus_->chip_channels_)) {
    chips[i + (map->channel_offset_ % map->bus_->chip_channels_)] = val;
  }
  map->bus_->schedule_refresh();
}

void Output::write_state(float state) {
  for (uint8_t i = 0; i < this->len_; i++) {
    write_mapping(&this->mappings_[i], state);
  }
};

}  // namespace fastled_bus
}  // namespace esphome
