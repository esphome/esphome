#include "zigbee_light_esp32.h"
#include "esphome/core/log.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#ifdef USE_LIGHT

namespace esphome::zigbee {

ESPHOME_LOG_TAG(TAG, "zigbee.light");

// convertion between float 0.0 - 1.0 scale and zigbee uint8_t scale 1 - 254 with 255 = NaN.
// theoretically 1 should map to the minimum brightness the light can still show, so should
// set this to 0+eps.
float level_to_brightness(uint8_t level) {
  if (level == 255)
    return std::numeric_limits<float>::quiet_NaN();
  if (level == 0)
    return std::numeric_limits<float>::min();
  return static_cast<float>(level - 1) / 253;
}

uint8_t brightness_to_level(float brightness) {
  if (std::isnan(brightness))
    return 255;
  return static_cast<uint8_t>(roundf(253.0f * brightness) + 1);  // 1 to 254
}

void ZigbeeLight::on_light_remote_values_update() {
  if (this->synchronizing_from_zigbee_)
    return;  // TODO: Fix this race-condition
  this->set_attr_internal();
}

void ZigbeeLight::connect(light::LightState *light_state) { this->light_state_ = light_state; }

void ZigbeeLight::setup() {
  this->next_call_ = this->light_state_->make_call();
  auto values = this->light_state_->remote_values;
  light::LightTraits traits = this->light_state_->get_traits();
  bool onoff = traits.supports_color_capability(light::ColorCapability::ON_OFF);
  bool level = traits.supports_color_capability(light::ColorCapability::BRIGHTNESS);
  bool color = traits.supports_color_capability(light::ColorCapability::RGB);
  bool ct = traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE);
  if (!onoff) {
    ESP_LOGE(TAG, "Light must at least support ON_OFF capability to be added as Zigbee light");
    this->mark_failed();
    return;
  }
  // on_off endpoint is already set up, update device id for others
  if (level && (color || ct)) {
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
    this->zb_->add_cluster(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_CLIENT);
    uint8_t value = brightness_to_level(values.get_brightness());
    this->zb_->add_attr(this->level_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, 0, value);

    // static attributes for level cluster
    uint16_t transition_time = (uint16_t) (this->light_state_->get_default_transition_length() / 100);  // in 1/10s
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID, 0, transition_time);

    uint8_t options = 0b00000001;
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_OPTIONS_ID, 0, options);
    uint16_t time = 0;
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_REMAINING_TIME_ID, 0, time);
  }
}

bool ZigbeeLight::loop_internal() {
  if (this->call_pending_) {
    this->make_call_();
  }
  if (!this->set_attr_requested_ && this->block_zigbee_) {
    this->block_zigbee_ = false;
  }
  return !this->block_zigbee_;
}

void ZigbeeLight::schedule_call_() {
  this->call_pending_ = true;
  this->enable_loop();
}

void ZigbeeLight::make_call_() {
  this->call_pending_ = false;
  this->next_call_.set_transition_length(0.1);
  this->synchronizing_from_zigbee_ = true;
  this->next_call_.perform();
  this->synchronizing_from_zigbee_ = false;
  this->next_call_ = this->light_state_->make_call();
}

void ZigbeeLight::apply_on_off_(bool on) {
  if (this->block_zigbee_)
    return;
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
        float brightness = level_to_brightness(lvl);
        this->next_call_.set_brightness(brightness);
        ESP_LOGD(TAG, "set brightness: %f", brightness);
      }
    }
    esp_zigbee_lock_release();
  }
  this->schedule_call_();
}

void ZigbeeLight::apply_level_(uint8_t level) {
  if (this->block_zigbee_)
    return;
  float brightness = level_to_brightness(level);
  this->next_call_.set_brightness(brightness);
  ESP_LOGD(TAG, "set brightness: %f", brightness);
  this->schedule_call_();
}

void ZigbeeLight::set_attr_internal() {
  this->set_attr_requested_ = true;
  this->block_zigbee_ = true;  // prevent race conditions
  this->enable_loop();
  if (this->zb_->is_started() && esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    if (ezb_zcl_get_cluster_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER) != NULL) {
      ezb_zcl_attr_desc_t on_time =
          ezb_zcl_get_attr_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                                EZB_ZCL_ATTR_ON_OFF_ON_TIME_ID, EZB_ZCL_STD_MANUF_CODE);
      ezb_zcl_attr_desc_t off_time =
          ezb_zcl_get_attr_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                                EZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME_ID, EZB_ZCL_STD_MANUF_CODE);
      if (on_time && off_time) {
        uint16_t ontime = 0;
        ezb_zcl_attr_desc_get_value(on_time, &ontime);
        uint16_t offtime = 0;
        ezb_zcl_attr_desc_get_value(off_time, &offtime);
        if (ontime > 0 || offtime > 0) {
          // on with timed off in progress, send on
          ezb_zcl_on_off_cmd_t cmd_req = {};
          cmd_req.cmd_ctrl.src_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
          cmd_req.cmd_ctrl.dst_addr.u.short_addr = ezb_get_short_address();
          cmd_req.cmd_ctrl.dis_default_rsp = true;
          ezb_err_t ret = ezb_zcl_on_off_on_cmd_req(&cmd_req);  // send on_off cluster command
          ESP_LOGI(TAG, "Err %u", ret);
        }
      }
    }
    if (ezb_zcl_get_cluster_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER) != NULL) {
      ezb_zcl_attr_desc_t current_time =
          ezb_zcl_get_attr_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                                EZB_ZCL_ATTR_LEVEL_REMAINING_TIME_ID, EZB_ZCL_STD_MANUF_CODE);
      if (current_time) {
        uint16_t time = 0;
        ezb_zcl_attr_desc_get_value(current_time, &time);
        if (time > 0) {
          // level transition in progress, send stop
          ezb_zcl_level_stop_cmd_t cmd_req = {};
          cmd_req.cmd_ctrl.src_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
          cmd_req.cmd_ctrl.dst_addr.u.short_addr = ezb_get_short_address();
          cmd_req.cmd_ctrl.dis_default_rsp = true;
          ezb_err_t ret = ezb_zcl_level_stop_cmd_req(&cmd_req);  // send stop command
          ESP_LOGI(TAG, "Err %u", ret);
        }
      }
    }
    auto values = light_state_->remote_values;
    const auto color_mode = values.get_color_mode();
    bool on = values.is_on();
    ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                           EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
    ESP_LOGI(TAG, "Set %s", values.is_on() ? "ON" : "OFF");
    if (this->level_forwarder_ && color_mode & light::ColorCapability::BRIGHTNESS) {
      uint8_t level = brightness_to_level(values.get_brightness());
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE, &level, false);
      ESP_LOGI(TAG, "Set brightness %u", level);
    }
    // cleared before report_() so it can disable the loop
    // when the report has to wait for join
    this->set_attr_requested_ = false;
    if (this->force_report_) {
      this->report_internal(true);
    }
    esp_zigbee_lock_release();
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
