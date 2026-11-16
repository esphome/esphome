#include "zigbee_light_esp32.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE
#ifdef USE_LIGHT
#include "esphome/core/log.h"
#include "zigbee_converters.h"

namespace esphome::zigbee {

static constexpr uint32_t RECOVER_LEVEL_RETRY_TIMEOUT_ID = 0;
ESPHOME_LOG_TAG(TAG, "zigbee.light");

void ZigbeeLight::on_light_remote_values_update() {
  if (this->synchronizing_from_zigbee_)
    return;
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
  bool ct = traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE) ||
            (traits.supports_color_capability(light::ColorCapability::COLD_WARM_WHITE) && traits.get_min_mireds() > 0 &&
             traits.get_max_mireds() > 0);
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
    uint8_t level_value = brightness_to_level(values.get_brightness());
    this->zb_->add_attr(this->level_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, 0, level_value);

    // static attributes for level cluster
    uint16_t transition_time = (uint16_t) (this->light_state_->get_default_transition_length() / 100);  // in 1/10s
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_ON_OFF_TRANSITION_TIME_ID, 0, transition_time);

    uint8_t options = 0b00000001;  // execute if off
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_OPTIONS_ID, 0, options);
    uint16_t time = 0;
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_LEVEL_REMAINING_TIME_ID, 0, time);
  }
  // setup color cluster
  if (color || ct) {
    this->zb_->add_cluster(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER);
    this->zb_->add_cluster(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_CLIENT);
    uint8_t options = 0b00000001;  // execute if off
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_COLOR_CONTROL_OPTIONS_ID, 0, options);
    uint16_t time = 0;
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_COLOR_CONTROL_REMAINING_TIME_ID, 0, time);
    uint8_t mode = get_color_mode(values.get_color_mode());
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, 0, mode);
    uint8_t capa = color << 3 | ct << 4;
    this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                        EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_CAPABILITIES_ID, 0, capa);

    if (color) {
      this->x_forwarder_ = new ZigbeeOnValueForwarder<uint16_t, ZigbeeLight>(this, EZB_ZCL_ATTR_TYPE_UINT16,
                                                                             &ZigbeeLight::apply_x_, false);
      this->y_forwarder_ = new ZigbeeOnValueForwarder<uint16_t, ZigbeeLight>(this, EZB_ZCL_ATTR_TYPE_UINT16,
                                                                             &ZigbeeLight::apply_y_, false);
      rgb_to_xy(values.get_red(), values.get_green(), values.get_blue(), this->x_, this->y_);
      this->zb_->add_attr(this->x_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                          EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, 0, this->x_);
      this->zb_->add_attr(this->y_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                          EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, 0, this->y_);
    }

    if (ct) {
      this->ct_forwarder_ = new ZigbeeOnValueForwarder<uint16_t, ZigbeeLight>(this, EZB_ZCL_ATTR_TYPE_UINT16,
                                                                              &ZigbeeLight::apply_ct_, false);
      uint16_t colortemp;
      if (traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE)) {
        colortemp = convert_color_temp(values.get_color_temperature());
      } else {
        colortemp = get_color_temp(values.get_warm_white(), values.get_cold_white(), traits.get_min_mireds(),
                                   traits.get_max_mireds());
      }
      this->zb_->add_attr(this->ct_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                          EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_MIREDS_ID, 0, colortemp);
      uint16_t min_ct = convert_color_temp(traits.get_min_mireds());
      uint16_t max_ct = convert_color_temp(traits.get_max_mireds());
      this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                          EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, 0, min_ct);
      this->zb_->add_attr(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                          EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, 0, max_ct);
    }
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
  this->next_call_.set_transition_length(100);  // zigbee sends a new state every 100 ms
  this->synchronizing_from_zigbee_ = true;
  this->next_call_.perform();
  this->synchronizing_from_zigbee_ = false;
  this->next_call_ = this->light_state_->make_call();
}

void ZigbeeLight::apply_on_off_(bool on) {
  if (this->block_zigbee_) {
    this->set_attr_requested_ = true;
    ESP_LOGD(TAG, "discard: %s", on ? "ON" : "OFF");
    return;
  }
  this->next_call_.set_state(on);
  ESP_LOGD(TAG, "set state");
  if (!on) {
    // the zigbee sdk always sets brightness to the minimum while turning off (fade out).
    // Afterwards the attribute restores the previous level, but the application is not notified.
    if (!esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
      // in case lock fails (unlikely), retry in 50 ms
      this->set_timeout(RECOVER_LEVEL_RETRY_TIMEOUT_ID, 50, [on, this]() { this->apply_on_off_(on); });
      return;
    }
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
  if (this->block_zigbee_) {
    this->set_attr_requested_ = true;
    return;
  }
  float brightness = level_to_brightness(level);
  this->next_call_.set_brightness(brightness);
  ESP_LOGD(TAG, "set brightness: %f", brightness);
  this->schedule_call_();
}

void ZigbeeLight::apply_x_(uint16_t x) {
  if (this->block_zigbee_) {
    this->set_attr_requested_ = true;
    return;
  }
  this->x_ = x;
  this->apply_color_();
}

void ZigbeeLight::apply_y_(uint16_t y) {
  if (this->block_zigbee_) {
    this->set_attr_requested_ = true;
    return;
  }
  this->y_ = y;
  this->apply_color_();
}

void ZigbeeLight::apply_color_() {
  if (this->x_ < 0xFFFF && this->y_ < 0xFFFF) {
    float r, g, b;
    if (xy_to_rgb(this->x_, this->y_, r, g, b)) {
      this->next_call_.set_rgb(r, g, b);
      this->next_call_.set_color_brightness_if_supported(1);
      this->next_call_.set_white_if_supported(0);
      this->next_call_.set_warm_white_if_supported(0);
      this->next_call_.set_cold_white_if_supported(0);
      this->next_call_.set_color_mode_if_supported(light::ColorMode::RGB);
      ESP_LOGD(TAG, "set color: %f, %f, %f", r, g, b);
      this->schedule_call_();
    }
  }
}

void ZigbeeLight::apply_ct_(uint16_t ct) {
  if (this->block_zigbee_) {
    this->set_attr_requested_ = true;
    return;
  }
  this->next_call_.set_color_temperature(convert_color_temp(ct));
  this->next_call_.set_color_brightness_if_supported(0);
  this->next_call_.set_white_if_supported(1);
  this->next_call_.set_color_mode_if_supported(light::ColorMode::COLD_WARM_WHITE);
  this->next_call_.set_color_mode_if_supported(light::ColorMode::COLOR_TEMPERATURE);
  ESP_LOGD(TAG, "set color temp: %f", convert_color_temp(ct));
  this->schedule_call_();
}

void ZigbeeLight::set_attr_internal() {
  this->set_attr_requested_ = true;
  this->block_zigbee_ = true;  // prevent race conditions
  this->enable_loop();
  auto values = light_state_->remote_values;
  const auto color_mode = values.get_color_mode();
  bool on = values.is_on();
  if (this->zb_->is_started() && esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    // the sdk internal state machine can only be stopped by sending local zigbee commands. We need to check
    // if a transition is active and send the appropiate commands. Those are applied before set_attr_value.
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
          // on with timed off in progress, send off then on
          ezb_zcl_on_off_cmd_t cmd_req = {};
          cmd_req.cmd_ctrl.src_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
          cmd_req.cmd_ctrl.dst_addr.u.short_addr = ezb_get_short_address();
          cmd_req.cmd_ctrl.dis_default_rsp = true;
          if (ontime > 0) {
            ezb_err_t ret = ezb_zcl_on_off_off_cmd_req(&cmd_req);  // send off command
            esp_zigbee_lock_release();
            if (ret != EZB_ERR_NONE) {
              ESP_LOGW(TAG, "Could not send off command: Error %u", ret);
            }
            return;  // proceed in next loop iteration so the off command is processed.
          }
          ezb_err_t ret = ezb_zcl_on_off_on_cmd_req(&cmd_req);  // send on command
          esp_zigbee_lock_release();
          if (ret != EZB_ERR_NONE) {
            ESP_LOGW(TAG, "Could not send on command: Error %u", ret);
          }
          return;  // proceed in next loop iteration so the on command is processed.
        }
      }
    }
    if (this->level_forwarder_ &&
        ezb_zcl_get_cluster_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER) != NULL) {
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
          if (ret != EZB_ERR_NONE) {
            ESP_LOGW(TAG, "Could not send stop command: Error %u", ret);
          }
        }
      }
    }
    if (this->x_forwarder_ && ezb_zcl_get_cluster_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                                       EZB_ZCL_CLUSTER_SERVER) != NULL) {
      ezb_zcl_attr_desc_t current_time =
          ezb_zcl_get_attr_desc(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                                EZB_ZCL_ATTR_COLOR_CONTROL_REMAINING_TIME_ID, EZB_ZCL_STD_MANUF_CODE);
      if (current_time) {
        uint16_t time = 0;
        ezb_zcl_attr_desc_get_value(current_time, &time);
        if (time > 0) {
          // level transition in progress, send stop
          ezb_zcl_color_control_stop_move_step_cmd_t cmd_req = {};
          cmd_req.cmd_ctrl.src_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_ep = this->endpoint_id_;
          cmd_req.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
          cmd_req.cmd_ctrl.dst_addr.u.short_addr = ezb_get_short_address();
          cmd_req.cmd_ctrl.dis_default_rsp = true;
          ezb_err_t ret = ezb_zcl_color_control_stop_move_step_cmd_req(&cmd_req);  // send stop command
          if (ret != EZB_ERR_NONE) {
            ESP_LOGW(TAG, "Could not send stop_move_step command: Error %u", ret);
          }
        }
      }
    }
    ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
                           EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &on, false);
    ESP_LOGI(TAG, "Set %s", values.is_on() ? "ON" : "OFF");
    if (this->level_forwarder_ && color_mode & light::ColorCapability::BRIGHTNESS) {
      uint8_t level = brightness_to_level(values.get_brightness());
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE, &level, false);
      ESP_LOGI(TAG, "Set brightness %u", level);
    }
    if (this->x_forwarder_ && color_mode & light::ColorCapability::RGB) {
      rgb_to_xy(values.get_red(), values.get_green(), values.get_blue(), this->x_, this->y_);
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, EZB_ZCL_STD_MANUF_CODE, &this->x_, false);
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, EZB_ZCL_STD_MANUF_CODE, &this->y_, false);
      uint8_t mode = 0x01;
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, EZB_ZCL_STD_MANUF_CODE, &mode, false);
      ESP_LOGI(TAG, "Set color");
    }
    if (this->ct_forwarder_ && (color_mode & light::ColorCapability::COLOR_TEMPERATURE ||
                                color_mode & light::ColorCapability::COLD_WARM_WHITE)) {
      uint16_t ct;
      if (color_mode & light::ColorCapability::COLOR_TEMPERATURE) {
        ct = convert_color_temp(values.get_color_temperature());
      } else {
        light::LightTraits traits = this->light_state_->get_traits();
        ct = get_color_temp(values.get_warm_white(), values.get_cold_white(), traits.get_min_mireds(),
                            traits.get_max_mireds());
      }
      if (ct > 0) {
        ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                               EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_MIREDS_ID, EZB_ZCL_STD_MANUF_CODE, &ct,
                               false);
        ESP_LOGI(TAG, "Set color temp: %u", ct);
      }
      uint8_t mode = 0x02;
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, EZB_ZCL_STD_MANUF_CODE, &mode, false);
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
  if (!this->zb_->is_joined()) {
    this->report_requested_ = true;
    return;
  }
  if (has_lock or esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    ezb_zcl_report_attr_cmd_t cmd = {};
    cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
    cmd.cmd_ctrl.fc.dis_default_rsp = 1;
    cmd.cmd_ctrl.dst_addr.addr_mode = EZB_ADDR_MODE_SHORT;
    cmd.cmd_ctrl.dst_addr.u.short_addr = 0x0000;
    cmd.cmd_ctrl.dst_ep = 1;
    cmd.cmd_ctrl.src_ep = this->endpoint_id_;
    cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF;
    cmd.payload.attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    ezb_zcl_report_attr_cmd_req(&cmd);
    if (this->level_forwarder_) {
      cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_LEVEL;
      cmd.payload.attr_id = EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID;
      ezb_zcl_report_attr_cmd_req(&cmd);
    }
    if (this->x_forwarder_) {
      cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_COLOR_CONTROL;
      cmd.payload.attr_id = EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID;
      ezb_zcl_report_attr_cmd_req(&cmd);
      cmd.payload.attr_id = EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID;
      ezb_zcl_report_attr_cmd_req(&cmd);
    }
    if (this->ct_forwarder_) {
      cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_COLOR_CONTROL;
      cmd.payload.attr_id = EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_MIREDS_ID;
      ezb_zcl_report_attr_cmd_req(&cmd);
    }
    this->report_requested_ = false;
    if (!has_lock) {
      esp_zigbee_lock_release();
    }
  }
}

}  // namespace esphome::zigbee

#endif
#endif
#endif
