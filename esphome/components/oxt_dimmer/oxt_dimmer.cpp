/*
  Copyright © 2023
*/

/*********************************************************************************************\
 * The OXT dimmer uses simple protocol over serial @9600bps where each frame looks like:
 *  0  1  2  3  4  5  6  7  8  9  A  B
 * 00 00 ff 55                             - Header
 *             01                          - Channel being updated
 *                4b                       - Light brightness of channel 1 [00..ff]
 *                   4c                    - Light brightness of channel 2 [00..ff]
 *                      05 dc 0a 00 00     - Footer
\*********************************************************************************************/
#include "oxt_dimmer.h"

namespace esphome {
namespace oxt_dimmer {

static const char *const TAG = "oxt_dimmer";

void OxtController::send_to_mcu_(const OxtDimmerChannel *updated_channel) {
  struct {
    uint8_t header[3];
    uint8_t update;
    uint8_t channel[2];
    uint8_t footer[4];
  } frame{{0x00, 0xff, 0x55}, 0x00, {0x00, 0x00}, {0x05, 0xdc, 0x0a, 0x00}};

  for (size_t index = 0; index < MAX_CHANNELS; index++) {
    auto *channel = this->channels_[index];
    if (channel == nullptr)
      continue;

    auto brightness = channel->brightness();
    frame.channel[index] = brightness;

    if (channel == updated_channel) {
      frame.update = index + 1;
      ESP_LOGD(TAG, "Setting channel %u, raw brightness=%d", index, brightness);
    }
  }

  if (frame.update == 0) {
    ESP_LOGE(TAG, "Unable to find channel index");
    return;
  }
  this->write_array(reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
  char vbuf[sizeof(frame) * 2 + 1];
  ESP_LOGV(TAG, "Frame: %s", format_hex_pretty_to(vbuf, reinterpret_cast<uint8_t *>(&frame), sizeof(frame)));
}

light::LightTraits OxtDimmerChannel::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void OxtDimmerChannel::write_state(light::LightState *state) {
  if (this->controller_ == nullptr) {
    ESP_LOGE(TAG, "No controller - state change ignored");
    return;
  }

  float brightness;

  // Fill our variables with the device's current state
  state->current_values_as_brightness(&brightness);

  // Convert ESPHome's brightness (0-1) to the internal brightness (0-255)
  const uint8_t calculated_brightness = remap<uint8_t, float>(brightness, 0.0, 1.0f, min_value_, max_value_);

  // If a new value, write to the dimmer
  if (calculated_brightness != brightness_) {
    this->brightness_ = calculated_brightness;
    this->controller_->send_to_mcu_(this);
  }
}

void OxtDimmerChannel::dump_config() {
  ESP_LOGCONFIG(TAG, "OXT channel: '%s'", this->light_state_ ? this->light_state_->get_name().c_str() : "");
  ESP_LOGCONFIG(TAG, "  Minimal brightness: %d", this->min_value_);
  ESP_LOGCONFIG(TAG, "  Maximal brightness: %d", this->max_value_);
}

void OxtController::dump_config() { ESP_LOGCONFIG(TAG, "Oxt dimmer"); }

}  // namespace oxt_dimmer
}  // namespace esphome
