#include "message.h"
#include "neewer_ct_light_output.h"
#include "neewer_state_output.h"
#include "utils.h"

#include "esphome/core/log.h"

#include <cstdint>
#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace neewerlight_ct {

using utils::TAG;

NeewerCTLightOutput::NeewerCTLightOutput() {
  set_color_temperature(new NeewerStateOutput());
  set_brightness(new NeewerStateOutput());
};

void NeewerCTLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Neewer CT Light:\n"
                "  MAC address: %s\n"
                "  Service UUID: %s\n"
                "  Characteristic UUID: %s\n"
                "  Require Response: %s\n"
                "  Colour Temperature Range: %d K - %d K",
                this->parent_ ? this->parent_->address_str() : "N/A", this->ble_.service_uuid_.to_string().c_str(),
                this->ble_.characteristic_uuid_.to_string().c_str(), this->ble_.require_response_ ? "True" : "False",
                utils::mireds_to_kelvin_int(warm_white_temperature_),
                utils::mireds_to_kelvin_int(cold_white_temperature_));
};

void NeewerCTLightOutput::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  this->ble_.gattc_event_handler(event, gattc_if, param);
}

void NeewerCTLightOutput::write_state(light::LightState *state) {
  // Copied from color_temperature::CTLightOutput::write_state().
  float color_temperature, brightness;
  state->current_values_as_ct(&color_temperature, &brightness);
  ESP_LOGD(TAG, "write_state called with color_temperature = %.2f, brightness = %.2f", color_temperature, brightness);

  // Calculate values in Kelvin and percent to be used both in logging and sending to the light.
  int color_temperature_k =
      utils::normalized_mireds_to_kelvin_int(color_temperature, cold_white_temperature_, warm_white_temperature_);
  int old_color_temperature_k = utils::normalized_mireds_to_kelvin_int(
      this->old_state_.color_temperature, cold_white_temperature_, warm_white_temperature_);
  int brightness_percent = utils::brightness_to_percent(brightness);
  int old_brightness_percent = utils::brightness_to_percent(this->old_state_.brightness);

  // For on/off state we have to use state->remote_values, since there is no current_values_as_on_off() method.
  OnOffState on_off_state = state->remote_values.is_on() ? OnOffState::ON : OnOffState::OFF;

  // Turn on *before* potentially setting color temperature and brightness.
  bool turned_on = this->old_state_.on_off != OnOffState::ON && on_off_state == OnOffState::ON;
  if (turned_on) {
    ESP_LOGD(TAG, "Light was turned off, turning it on.");
    this->turn_on_off_(OnOffState::ON);
  }

  if (brightness_percent != old_brightness_percent) {
    ESP_LOGD(TAG, "Brightness changed from %d%% to %d%%", old_brightness_percent, brightness_percent);
  }

  // There is a reasonable suspicion that when the light is turned on, the color temperature is set to warmest possible.
  // Therefore, we will send the message after turning the light on, even if the color temperature has not changed.
  if (color_temperature_k != old_color_temperature_k || turned_on) {
    if (color_temperature_k != old_color_temperature_k) {
      ESP_LOGD(TAG, "Color temperature changed from %d K to %d K", old_color_temperature_k, color_temperature_k);
    }
    this->change_color_temperature_and_brightness_(color_temperature_k, brightness_percent);
  } else if (brightness_percent != old_brightness_percent) {
    this->change_brightness_(brightness_percent);
  } else {
    ESP_LOGD(TAG, "No changes in brightness or color temperature detected.");
  }

  // Turn Off *after* potentially setting color temperature and brightness
  if (this->old_state_.on_off != OnOffState::OFF && on_off_state == OnOffState::OFF) {
    ESP_LOGD(TAG, "Light was turned on, turning it off.");
    this->turn_on_off_(OnOffState::OFF);
  }

  this->old_state_.color_temperature = color_temperature;
  this->old_state_.brightness = brightness;
  this->old_state_.on_off = on_off_state;

  // Copied from color_temperature::CTLightOutput::write_state().
  this->color_temperature_->set_level(color_temperature);
  this->brightness_->set_level(brightness);
};

void NeewerCTLightOutput::turn_on_off_(OnOffState on_off) {
  if (on_off == OnOffState::UNKNOWN) {
    ESP_LOGW(TAG, "Unknown On/Off state, not sending command.");
    return;
  }
  Message msg;
  msg.msg_type = MessageType::COMMAND;
  msg.cmd_type = CommandType::TURN_ON_OFF;
  msg.payload = {static_cast<uint8_t>((on_off == OnOffState::ON) ? 1 : 2)};  // Neewer expects 1 for ON and 2 for OFF
  this->ble_.send_message(this->parent_, neewer_msg(msg));
}

void NeewerCTLightOutput::change_color_temperature_and_brightness_(int color_temperature_k, int brightness_percent) {
  Message msg;
  msg.msg_type = MessageType::COMMAND;
  msg.cmd_type = CommandType::CT_BRIGHTNESS;
  msg.payload = {
      // expected value for brightness is 0 - 100 (0x00 - 0x64)
      static_cast<uint8_t>(brightness_percent),

      // neewer light expects value 0x1B (27) for 2700K or 0x41 (65) for 6500K
      // rounding is meant to avoid setting 27 for 2799K
      static_cast<uint8_t>(round(static_cast<float>(color_temperature_k) / 100.0f)),
  };
  this->ble_.send_message(this->parent_, neewer_msg(msg));
}

void NeewerCTLightOutput::change_brightness_(int brightness_percent) {
  Message msg;
  msg.msg_type = MessageType::COMMAND;
  msg.cmd_type = CommandType::CT_BRIGHTNESS;
  msg.payload = {
      // expected value for brightness is 0 - 100 (0x00 - 0x64)
      static_cast<uint8_t>(brightness_percent),
  };
  this->ble_.send_message(this->parent_, neewer_msg(msg));
}

std::vector<uint8_t> NeewerCTLightOutput::neewer_msg(const Message &msg) {
  std::vector<uint8_t> neewer_msg = {
      static_cast<uint8_t>(msg.msg_type),
      static_cast<uint8_t>(msg.cmd_type),
      static_cast<uint8_t>(msg.payload.size()),
  };
  neewer_msg.insert(neewer_msg.end(), msg.payload.begin(), msg.payload.end());
  neewer_msg.push_back(utils::checksum(neewer_msg));
  return neewer_msg;
}

}  // namespace neewerlight_ct
}  // namespace esphome

#endif  // USE_ESP32
