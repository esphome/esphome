#include "esphome/core/log.h"
#include "midea_climate.h"

namespace esphome {
namespace midea_ac {

static const char *TAG = "midea_ac";

void MideaAC::on_frame(const midea_dongle::Frame &frame) {
  const auto p = frame.as<PropertiesFrame>();
  if (!p.is<PropertiesFrame>()) {
    ESP_LOGW(TAG, "RX: not PropertiesFrame!");
    return;
  }
  if (p.get_type() == midea_dongle::MideaMessageType::DEVICE_CONTROL) {
    ESP_LOGD(TAG, "RX: control frame");
    this->ctrl_request_ = false;
  } else {
    ESP_LOGD(TAG, "RX: query frame");
  }
  if (this->ctrl_request_)
    return;
  this->cmd_frame_.set_properties(p);  // copy properties from response
  bool need_publish = false;
  if (this->mode != p.get_mode()) {
    this->mode = p.get_mode();
    need_publish = true;
  }
  if (this->target_temperature != p.get_target_temp()) {
    this->target_temperature = p.get_target_temp();
    need_publish = true;
  }
  if (this->current_temperature != p.get_indoor_temp()) {
    this->current_temperature = p.get_indoor_temp();
    need_publish = true;
  }
  if (this->fan_mode != p.get_fan_mode()) {
    this->fan_mode = p.get_fan_mode();
    need_publish = true;
  }
  if (this->swing_mode != p.get_swing_mode()) {
    this->swing_mode = p.get_swing_mode();
    need_publish = true;
  }
  if (need_publish)
    this->publish_state();
  if (this->outdoor_sensor_ != nullptr &&
      (!this->outdoor_sensor_->has_state() || this->outdoor_sensor_->get_raw_state() != p.get_outdoor_temp()))
    this->outdoor_sensor_->publish_state(p.get_outdoor_temp());
}

void MideaAC::on_update() {
  if (this->ctrl_request_) {
    ESP_LOGD(TAG, "TX: control frame");
    this->parent_->write_frame(this->cmd_frame_);
  } else {
    ESP_LOGD(TAG, "TX: query frame");
    this->parent_->write_frame(this->query_frame_);
  }
}

void MideaAC::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value() && call.get_mode().value() != this->mode) {
    this->cmd_frame_.set_mode(call.get_mode().value());
    this->ctrl_request_ = true;
  }
  if (call.get_target_temperature().has_value() && call.get_target_temperature().value() != this->target_temperature) {
    this->cmd_frame_.set_target_temp(call.get_target_temperature().value());
    this->ctrl_request_ = true;
  }
  if (call.get_fan_mode().has_value() && call.get_fan_mode().value() != this->fan_mode) {
    this->cmd_frame_.set_fan_mode(call.get_fan_mode().value());
    this->ctrl_request_ = true;
  }
  if (call.get_swing_mode().has_value() && call.get_swing_mode().value() != this->swing_mode) {
    this->cmd_frame_.set_swing_mode(call.get_swing_mode().value());
    this->ctrl_request_ = true;
  }
  if (this->ctrl_request_) {
    this->cmd_frame_.set_beeper_feedback(this->beeper_feedback_);
    this->cmd_frame_.finalize();
  }
}

climate::ClimateTraits MideaAC::traits() {
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

}  // namespace midea_ac
}  // namespace esphome
