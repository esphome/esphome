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

static const char *const TAG = "oxt";

void OxtController::update() {
  for (const auto &channel : channels_) {
    if (channel)
      channel->update_sensing_input();
  }
}

void OxtController::send_to_mcu_(const OxtDimmerChannel *updated_channel) {
  struct {
    uint8_t header[3];
    uint8_t update;
    uint8_t channel[2];
    uint8_t footer[4];
  } frame{{0x00, 0xff, 0x55}, 0x00, {0x00, 0x00}, {0x05, 0xdc, 0x0a, 0x00}};

  for (size_t index = 0; index < MAX_CHANNELS; index++) {
    auto *channel = channels_[index];
    if (channel == nullptr)
      continue;

    auto binary = channel->is_on();
    auto brightness = channel->brightness();
    if (binary) {
      frame.channel[index] = brightness;
    }

    if (channel == updated_channel) {
      frame.update = index + 1;
      ESP_LOGI(TAG, "Setting channel %u state=%s, raw brightness=%d", index, ONOFF(binary), brightness);
    }
  }

  if (frame.update == 0) {
    ESP_LOGE(TAG, "Unable to find channel index");
    return;
  }

  ESP_LOGV(TAG, "Frame: %s", format_hex_pretty(reinterpret_cast<uint8_t *>(&frame), sizeof(frame)).c_str());
  this->write_array(reinterpret_cast<uint8_t *>(&frame), sizeof(frame));
}

light::LightTraits OxtDimmerChannel::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void OxtDimmerChannel::write_state(light::LightState *state) {
  if (controller_ == nullptr) {
    ESP_LOGE(TAG, "No controller - state change ignored");
    return;
  }

  bool binary;
  float brightness;

  // Fill our variables with the device's current state
  state->current_values_as_binary(&binary);
  state->current_values_as_brightness(&brightness);

  // Convert ESPHome's brightness (0-1) to the internal brightness (0-255)
  const uint8_t calculated_brightness = remap<uint8_t, float>(brightness, 0.0, 1.0f, min_value_, max_value_);

  if (calculated_brightness == 0) {
    binary = false;
  }

  // If a new value, write to the dimmer
  if (binary != binary_ || calculated_brightness != brightness_) {
    brightness_ = calculated_brightness;
    binary_ = binary;
    controller_->send_to_mcu_(this);
  }
}

void OxtDimmerChannel::short_press_() {
  ESP_LOGI(TAG, "short_press");
  light_state_->toggle().perform();
}

void OxtDimmerChannel::periodic_long_press_() {
  // Note: This function is operating on ESPHome brightness values in range 0-1 float
  float brightness;
  light_state_->current_values_as_brightness(&brightness);

  ESP_LOGD(TAG, "brightness: %0.2f, direction: %d, millis %u", brightness, sensing_state_.direction_, millis());
  brightness = clamp(brightness + sensing_state_.direction_ * 0.02f, 0.0f, 1.0f);
  ESP_LOGI(TAG, "next brightness: %0.2f", brightness);

  light_state_->make_call()
      .set_brightness(brightness)
      .set_state((brightness > 0))
      .set_transition_length({})  // cancel transition, if any
      .perform();
}

void OxtDimmerChannel::update_sensing_input() {
  if (!sensing_state_.sensing_pin_)
    return;

  bool btn_pressed = sensing_state_.sensing_pin_->digital_read();

  switch (sensing_state_.state_) {
    case SensingStateT::STATE_RELEASED:
      if (btn_pressed) {
        sensing_state_.millis_pressed_ = millis();
        sensing_state_.state_ = SensingStateT::STATE_DEBOUNCING;
      }
      break;

    case SensingStateT::STATE_DEBOUNCING:
      if (!btn_pressed) {
        sensing_state_.millis_pressed_ = 0;
        sensing_state_.state_ = SensingStateT::STATE_RELEASED;
      } else if (millis() - sensing_state_.millis_pressed_ > 50) {
        sensing_state_.state_ = SensingStateT::STATE_PRESSED;
      }
      break;

    case SensingStateT::STATE_PRESSED:
      if (!btn_pressed) {
        short_press_();
        sensing_state_.state_ = SensingStateT::STATE_RELEASED;
      } else if (millis() - sensing_state_.millis_pressed_ > 1000) {
        sensing_state_.state_ = SensingStateT::STATE_LONGPRESS;
        sensing_state_.direction_ *= -1;
      }
      break;

    case SensingStateT::STATE_LONGPRESS:
      if (btn_pressed) {
        periodic_long_press_();
      } else
        sensing_state_.state_ = SensingStateT::STATE_RELEASED;
      break;

    default:
      ESP_LOGE(TAG, "should never get here");
  }
}

void OxtDimmerChannel::dump_config() {
  ESP_LOGCONFIG(TAG, "OXT channel: '%s'", light_state_ ? light_state_->get_name().c_str() : "");
  ESP_LOGCONFIG(TAG, "  Minimal brightness: %d", min_value_);
  ESP_LOGCONFIG(TAG, "  Maximal brightness: %d", max_value_);
  ESP_LOGCONFIG(TAG, "  Sensing pin: %s",
                sensing_state_.sensing_pin_ ? sensing_state_.sensing_pin_->dump_summary().c_str() : "none");
}

void OxtController::dump_config() { ESP_LOGCONFIG(TAG, "Oxt dimmer"); }

}  // namespace oxt_dimmer
}  // namespace esphome
