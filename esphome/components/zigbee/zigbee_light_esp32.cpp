#include "zigbee_light_esp32.h"
#include "esphome/core/log.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#ifdef USE_LIGHT

namespace esphome::zigbee {

ESPHOME_LOG_TAG(TAG, "zigbee.light");

// convertion between float 0.0 - 1.0 scale and zigbee uint8_t scale 1 - 254 with 255 = NaN.
// theoretically 1 should map to the minimum brightness the light can still show, so should
// set this to 0+e.
float level_to_brightness_(uint8_t level) {
  if (level == 255)
    return std::numeric_limits<float>::quiet_NaN();
  if (level == 0)
    return 0.0;
  return static_cast<float>(level - 1) / 253;
}

uint8_t brightness_to_level_(float brightness) {
  if (std::isnan(brightness))
    return 255;
  return static_cast<uint8_t>(roundf(253.0f * brightness) + 1);  // 1 to 254
}

void ZigbeeLight::on_light_remote_values_update() {
  if (this->synchronizing_from_zigbee_)
    return;  // TODO: Fix this race-condition
  this->set_attr_internal();
  // auto values = light_state_->remote_values;
  // const auto color_mode = values.get_color_mode();
  /**
    if (color_mode & light::ColorCapability::RGB && color_attr_ != nullptr) {
      color_attr_->set_attr(to_uint8_scale(values.get_red()));
      color_attr_->set_attr(to_uint8_scale(values.get_green()));
      color_attr_->set_attr(to_uint8_scale(values.get_blue()));
    }
    if (color_mode & light::ColorCapability::WHITE) {
      uint8_t white_val = to_uint8_scale(values.get_white());
      color[ESPHOME_F("w")] = white_val;
      root[ESPHOME_F("white_value")] = white_val;  // legacy API
    }
    if (color_mode & light::ColorCapability::COLOR_TEMPERATURE) {
      // this one isn't under the color subkey for some reason
      root[ESPHOME_F("color_temp")] = uint32_t(values.get_color_temperature());
    }
    if (color_mode & light::ColorCapability::COLD_WARM_WHITE) {
      color[ESPHOME_F("c")] = to_uint8_scale(values.get_cold_white());
      color[ESPHOME_F("w")] = to_uint8_scale(values.get_warm_white());
    }*/
}

void ZigbeeLight::connect(light::LightState *light_state) { this->light_state_ = light_state; }

void ZigbeeLight::setup() {
  this->next_call_ = this->light_state_->make_call();
  auto values = this->light_state_->remote_values;
  light::LightTraits traits = this->light_state_->get_traits();
  bool onoff = traits.supports_color_capability(light::ColorCapability::ON_OFF);
  bool level = traits.supports_color_capability(light::ColorCapability::BRIGHTNESS);
  bool color = traits.supports_color_capability(light::ColorCapability::RGB) ||
               traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE);
  if (!onoff) {
    ESP_LOGE(TAG, "Light must at least support ON_OFF capability to be added as Zigbee light");
    this->mark_failed();
    return;
  }
  // on_off endpoint is already set up, update device id for others
  if (level && color) {
    this->zb_->update_device_id(this->endpoint_id_, EZB_ZHA_COLOR_DIMMABLE_LIGHT_DEVICE_ID);
  } else if (level) {
    this->zb_->update_device_id(this->endpoint_id_, EZB_ZHA_DIMMABLE_LIGHT_DEVICE_ID);
  }

  this->light_state_->add_remote_values_listener(this);

  bool value = values.is_on();
  this->zb_->add_attr(&this->on_off_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                      EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, 0, value);

  // setup level cluster
  if (level) {
    this->level_forwarder_ = new ZigbeeOnValueForwarder<uint8_t, ZigbeeLight>(this, EZB_ZCL_ATTR_TYPE_UINT8,
                                                                              &ZigbeeLight::apply_level_, false);
    this->zb_->add_cluster(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER);
    uint8_t value = brightness_to_level_(values.get_brightness());
    this->zb_->add_attr(this->level_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, 0, value);

    // static attributes for level cluster
    uint16_t transition_time = (uint16_t) (this->light_state_->get_default_transition_length() / 100);  // in 1/10s
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID, 0, transition_time);

    uint8_t options = 0b00000001;
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_OPTIONS_ID, 0, options);
  }
}

bool ZigbeeLight::loop_internal() {
  if (this->call_pending_) {
    this->make_call_();
  }
  return true;
}

void ZigbeeLight::schedule_call_() {
  this->call_pending_ = true;
  this->enable_loop();
}

void ZigbeeLight::make_call_() {
  this->call_pending_ = false;
  this->next_call_.set_transition_length(0);
  this->synchronizing_from_zigbee_ = true;
  this->next_call_.perform();
  this->synchronizing_from_zigbee_ = false;
  this->next_call_ = this->light_state_->make_call();
}

void ZigbeeLight::apply_on_off_(bool on) {
  this->next_call_.set_state(on);
  ESP_LOGD(TAG, "set state");
  if (!on) {                                 // recover level
    esp_zigbee_lock_acquire(portMAX_DELAY);  // Todo: limit time?
    if (ezb_zcl_get_cluster_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER) != NULL) {
      ezb_zcl_attr_desc_t current_level =
          ezb_zcl_get_attr_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                                EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE);
      if (current_level) {
        ESP_LOGD(TAG, "got level");
        uint8_t lvl;
        ezb_zcl_attr_desc_get_value(current_level, &lvl);
        float brightness = level_to_brightness_(lvl);  // integer level between 0 and 254
        this->next_call_.set_brightness(brightness);
        ESP_LOGD(TAG, "set brightness: %f", brightness);
      }
    }
    esp_zigbee_lock_release();
  }
  this->schedule_call_();
}

void ZigbeeLight::apply_level_(uint8_t level) {
  float brightness = level_to_brightness_(level);  // integer level between 0 and 254
  this->next_call_.set_brightness(brightness);
  ESP_LOGD(TAG, "set brightness: %f", brightness);
  this->schedule_call_();
}

void ZigbeeLight::set_attr_internal() {
  if (this->zb_->is_started() && esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    auto values = light_state_->remote_values;
    const auto color_mode = values.get_color_mode();
    // if (color_mode & light::ColorCapability::ON_OFF)
    bool on = values.is_on();
    ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                           EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
    ESP_LOGI(TAG, "Set %s", values.is_on() ? "ON" : "OFF");
    if (this->level_forwarder_ && color_mode & light::ColorCapability::BRIGHTNESS) {
      uint8_t level = brightness_to_level_(values.get_brightness());
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE, &level, false);
    }
    // cleared before report_() so it can disable the loop
    // when the report has to wait for join
    this->set_attr_requested_ = false;
    if (this->force_report_) {
      this->report_internal(true);
    }
    esp_zigbee_lock_release();
  } else {
    this->set_attr_requested_ = true;
    this->enable_loop();
  }
}

void ZigbeeLight::report_internal(bool has_lock) {
  if (!this->report_enabled) {
    return;
  }
  this->report_enabled = false;
}

}  // namespace esphome::zigbee

#endif
#endif
#endif
