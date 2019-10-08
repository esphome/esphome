#include "esphome/core/log.h"
#include "tuya_light.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.light";

void TuyaLight::setup() {
  if (parent_ == nullptr)
    return;
  if (dimmer_id_ != -1) {
    parent_->register_listener(dimmer_id_, this);
    dimmer_value_ = parent_->get_dp_value(dimmer_id_);
  }
  if (switch_id_ != -1) {
    parent_->register_listener(switch_id_, this);
    switch_value_ = parent_->get_dp_value(switch_id_);
  }
}

void TuyaLight::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Dimmer:");
  if (dimmer_id_ != -1)
    ESP_LOGCONFIG(TAG, "   dimmer is dpid %d", dimmer_id_);
  if (switch_id_ != -1)
    ESP_LOGCONFIG(TAG, "   switch is dpid %d", switch_id_);
}

void TuyaLight::set_dimmer(int dpid) { dimmer_id_ = dpid; }

void TuyaLight::set_switch(int dpid) { switch_id_ = dpid; }

void TuyaLight::set_tuya_parent(Tuya *parent) { parent_ = parent; }

void TuyaLight::set_min_value(uint32_t value) { min_value_ = value; }

void TuyaLight::set_max_value(uint32_t value) { max_value_ = value; }

void TuyaLight::dp_update(int dpid, uint32_t value) {
  ESP_LOGV(TAG, "dp_update(%d, %d)", dpid, value);
  if (dpid == dimmer_id_)
    update_dimmer_(value);
  if (dpid == switch_id_)
    update_switch_(value);
}

void TuyaLight::update_dimmer_(uint32_t value) {
  ESP_LOGV(TAG, "dimmer update to %d", value);
  state_->make_call().set_brightness(float(value) / max_value_).perform();
}

void TuyaLight::update_switch_(uint32_t value) {
  ESP_LOGV(TAG, "switch update to %d", value);
  if (value > 0)
    state_->turn_on().perform();
  else
    state_->turn_off().perform();
}

light::LightTraits TuyaLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supports_brightness(true);
  return traits;
}

void TuyaLight::setup_state(light::LightState *state) { state_ = state; }

void TuyaLight::write_state(light::LightState *state) {
  auto val = state->current_values;
  uint32_t value = (uint32_t)(val.get_brightness() * max_value_);
  if (!val.is_on()) {
    if (switch_id_ != -1) {
      parent_->set_dp_value(switch_id_, 0);
      switch_value_ = 0;
    } else if (dimmer_id_ != -1) {
      parent_->set_dp_value(dimmer_id_, 0);
      dimmer_value_ = 0;
    }
    return;
  }
  if (value < min_value_)
    value = min_value_;
  if (dimmer_id_ != -1) {
    parent_->set_dp_value(dimmer_id_, value);
    dimmer_value_ = value;
  }
  if ((switch_id_ != -1) && (switch_value_ == 0)) {
    parent_->set_dp_value(switch_id_, 1);
    switch_value_ = 1;
  }
}

}  // namespace tuya
}  // namespace esphome
