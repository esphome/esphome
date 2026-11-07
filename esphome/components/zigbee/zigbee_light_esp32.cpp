#include "zigbee_light_esp32.h"
#include "esphome/core/log.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#ifdef USE_LIGHT

namespace esphome::zigbee {

ESPHOME_LOG_TAG(TAG, "zigbee.light");

void ZigbeeLight::on_light_remote_values_update() {
  if (this->synchronizing_from_zigbee_)
    return;  // TODO: Fix this race-condition
  this->set_attr_();
  // auto values = light_state_->remote_values;
  // const auto color_mode = values.get_color_mode();
  //  set attributes
  // if (color_mode & light::ColorCapability::ON_OFF)
  // onoff_attr_->set_attr(values.is_on());
  // ESP_LOGI(TAG, "Set %s", values.is_on() ? "ON" : "OFF");
  // if (color_mode & light::ColorCapability::BRIGHTNESS && brightness_attr_ != nullptr)
  // if (brightness_attr_ != nullptr)
  // brightness_attr_->set_attr(light::to_uint8_scale(values.get_brightness()));
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

void ZigbeeLight::connect(light::LightState *device) {
  this->light_state_ = device;
  auto values = device->remote_values;
  light::LightTraits traits = device->get_traits();
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
                      EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, 0, std::move(value));

  // setup level cluster
  if (level) {
    this->zb_->add_cluster(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER);
    uint8_t value = light::to_uint8_scale(values.get_brightness());
    this->zb_->add_attr(&this->level_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, 0, std::move(value));

    // static attributes for level cluster
    uint16_t transition_time = (uint16_t) (device->get_default_transition_length() / 100);  // in 1/10s
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID, 0, transition_time);

    uint8_t options = 0b00000001;
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_OPTIONS_ID, 0, options);
  }
}

void ZigbeeLight::apply_on_off_(bool on) {
  if (on && this->light_state_->remote_values.is_on() == on) {
    ESP_LOGD(TAG, "return");
    return;
  }
  auto call = this->light_state_->make_call();
  call.set_state(on);
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
        float brightness = static_cast<float>(lvl) / 254;  // integer level between 0 and 254
        call.set_brightness(brightness);
      }
    }
    esp_zigbee_lock_release();
  }
  call.set_transition_length(0);
  this->synchronizing_from_zigbee_ = true;
  call.perform();
  this->synchronizing_from_zigbee_ = false;
}

void ZigbeeLight::apply_level_(uint8_t level) {
  float brightness = static_cast<float>(level) / 254;  // integer level between 0 and 254
  if (std::fabs(this->light_state_->remote_values.get_brightness() - brightness) < (0.5f / 254.0f)) {
    return;
  }
  auto call = this->light_state_->make_call();
  call.set_brightness(brightness);
  call.set_transition_length(0);
  this->synchronizing_from_zigbee_ = true;
  call.perform();
  this->synchronizing_from_zigbee_ = false;
}

void ZigbeeLight::set_attr_() {
  if (!this->zb_->is_started()) {
    return;
  }
  if (esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    auto values = light_state_->remote_values;
    const auto color_mode = values.get_color_mode();
    // if (color_mode & light::ColorCapability::ON_OFF)
    bool on = values.is_on();
    ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                           EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
    ESP_LOGI(TAG, "Set %s", values.is_on() ? "ON" : "OFF");
    if (color_mode & light::ColorCapability::BRIGHTNESS) {
      uint8_t level = static_cast<uint8_t>(roundf(253.0f * values.get_brightness()) + 1);  // 1 to 254
      ezb_zcl_status_t state =
          ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                                 EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE, &level, false);
    }
    // cleared before report_() so it can disable the loop
    // when the report has to wait for join
    this->set_attr_requested_ = false;
    if (this->force_report_) {
      this->report_(true);
    }
    esp_zigbee_lock_release();
  } else {
    this->set_attr_requested_ = true;
    this->enable_loop();
  }
}

void ZigbeeLight::report_(bool has_lock) {
  if (!this->report_enabled) {
    return;
  }
}

}  // namespace esphome::zigbee

#endif
#endif
#endif
