#include "fastled_bus_output.h"

namespace esphome {
namespace fastled_bus {

void setup_mapping(Mapping &map) {
  if (map.num_chips_ + map.ofs_ > map.bus_->num_chips_) {
    ESP_LOGE("fastled:bus:setup_mapping", "Number of chips (%d) + offset (%d) > (%d)", map.num_chips_, map.ofs_,
             map.bus_->num_chips_);
    return;
  }
  // ESP_LOGCONFIG("fastled:bus:setup_mapping", "Setting up FastLEDBusOutput...[%d:%d:%d:%d]", map.num_chips_, map.ofs_,
  //               map.channel_offset_, map.repeat_distance_);
}

void write_mapping(Mapping &map, float state) {
  auto *chips = &(map.bus_->chips()[map.ofs_ * map.bus_->chip_channels_]);
  auto val = uint8_t(state * 255);
  for (uint16_t i = (uint16_t)(map.channel_offset_ / map.bus_->chip_channels_);
       i < map.num_chips_ * map.bus_->chip_channels_;
       i += (uint16_t)(map.repeat_distance_)) {
    chips[i + (map.channel_offset_ % map.bus_->chip_channels_)] = val;
    // ESP_LOGCONFIG("fastled:bus:write_mapping", "%d:%x:controller:%p:%p:%p:%d:%d:%d:%d",
    // i, val,
    // map.bus_->controller(), chips, map.bus_->chips(),
    // map.ofs_, map.bus_->chip_channels_, map.num_chips_,
    // map.repeat_distance_);
  }
  map.bus_->schedule_refresh();
}

}  // namespace fastled_bus
}  // namespace esphome
