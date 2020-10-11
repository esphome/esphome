#include "esphome/core/log.h"
#include "midea_climate.h"

namespace esphome {
namespace midea_ac {

void MideaClimate::setup() {

  this->set_update_interval(1000);

  this->parent_->register_listener(midea_dongle::MideaAppliance::AIR_CONDITIONER, [this](midea_dongle::Frame &frame) {

    auto p = frame.as<PropertiesFrame>();

    switch (p.get_type()) {
      case midea_dongle::MideaMessageType::DEVICE_CONTROL:
        this->need_request_ &= ~MSG_CONTROL;
        break;
      case midea_dongle::MideaMessageType::DEVICE_NETWORK:
        this->need_request_ &= ~MSG_NETWORK;
        break;
    }

    if (!p.is<PropertiesFrame>()) {
      return;
    }

    this->mode = p.get_mode();
    this->target_temperature = p.get_target_temp();
    this->current_temperature = p.get_indoor_temp();
    this->fan_mode = p.get_fan_mode();
    this->swing_mode = p.get_swing_mode();
    this->publish_state();
  });
}

void MideaClimate::update() {
  if (this->need_request_ & MSG_CONTROL) {
    this->parent_->write_frame(this->cmd_frame_);
  } else if (this->need_request_ & MSG_NETWORK) {
    // TODO: network identification message
    ;
  } else {
    this->parent_->write_frame(this->query_frame_);
  }
}

void MideaClimate::control(const climate::ClimateCall &call) {

  this->cmd_frame_.set_mode(call.get_mode().value_or(this->mode));
  this->cmd_frame_.set_target_temp(call.get_target_temperature().value_or(this->target_temperature));
  this->cmd_frame_.set_fan_mode(call.get_fan_mode().value_or(this->fan_mode));
  this->cmd_frame_.set_swing_mode(call.get_swing_mode().value_or(this->swing_mode));
  this->cmd_frame_.disable_timer_on();
  this->cmd_frame_.disable_timer_off();
  this->cmd_frame_.set_beeper_feedback(this->beeper_feedback_);
  this->cmd_frame_.finalize();
  this->need_request_ |= MSG_CONTROL;
}

climate::ClimateTraits MideaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(0.5);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(true);
  traits.set_supports_dry_mode(true);
  traits.set_supports_heat_mode(true);
  traits.set_supports_fan_only_mode(true);
  traits.set_supports_fan_mode_auto(true);
  traits.set_supports_fan_mode_low(true);
  traits.set_supports_fan_mode_medium(true);
  traits.set_supports_fan_mode_high(true);
  traits.set_supports_swing_mode_off(true);
  traits.set_supports_swing_mode_vertical(true);
  traits.set_supports_current_temperature(true);
  return traits;
}

#if 0
void MideaClimate::dump_config() {
  LOG_CLIMATE("", "Tuya Climate", this);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  if (this->target_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Target Temperature has datapoint ID %u", *this->target_temperature_id_);
  if (this->current_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Current Temperature has datapoint ID %u", *this->current_temperature_id_);
}
#endif

}  // namespace midea_ac
}  // namespace esphome
