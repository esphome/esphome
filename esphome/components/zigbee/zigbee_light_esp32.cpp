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

uint8_t get_color_mode(light::ColorMode mode) {
  if (!(mode & light::ColorCapability::RGB) && (mode & light::ColorCapability::COLOR_TEMPERATURE)) {
    return 0x02;
  }
  // default to XY
  return 0x01;
}

// This follows Home Assistant's color_RGB_to_xy() conversion. ESPHome uses
// normalized RGB channel values instead of Home Assistant's 0-255 values, and
// Zigbee stores each resulting CIE xy component as a 16-bit value.
bool rgb_to_xy(float red, float green, float blue, uint16_t &x, uint16_t &y) {
  float r = std::clamp(red, 0.0f, 1.0f);
  float g = std::clamp(green, 0.0f, 1.0f);
  float b = std::clamp(blue, 0.0f, 1.0f);
  if (r + g + b == 0.0f) {
    x = 0;
    y = 0;
    return true;
  }

  auto gamma_decode = [](float channel) {
    return channel > 0.04045f ? std::pow((channel + 0.055f) / 1.055f, 2.4f) : channel / 12.92f;
  };
  r = gamma_decode(r);
  g = gamma_decode(g);
  b = gamma_decode(b);

  float cie_x = r * 0.664511f + g * 0.154324f + b * 0.162028f;
  float cie_y = r * 0.283881f + g * 0.668433f + b * 0.047685f;
  float cie_z = r * 0.000088f + g * 0.072310f + b * 0.986039f;
  float sum = cie_x + cie_y + cie_z;

  x = static_cast<uint16_t>(std::lroundf(std::clamp(cie_x / sum * 65536.0f, 0.0f, 65279.0f)));
  y = static_cast<uint16_t>(std::lroundf(std::clamp(cie_y / sum * 65536.0f, 0.0f, 65279.0f)));
  return true;
}

// This follows Home Assistant's color_xy_to_RGB() conversion. Zigbee provides
// CIE xy as 16-bit values while ESPHome expects normalized RGB channels.
bool xy_to_rgb(uint16_t zb_x, uint16_t zb_y, float &red, float &green, float &blue) {
  float x = zb_x / 65536.0f;
  float y = zb_y / 65536.0f;
  if (y == 0.0f)
    return false;

  float cie_x = x / y;
  float cie_z = (1.0f - x - y) / y;

  float r = cie_x * 1.656492f - 0.354851f - cie_z * 0.255038f;
  float g = -cie_x * 0.707196f + 1.655397f + cie_z * 0.036152f;
  float b = cie_x * 0.051713f - 0.121364f + cie_z * 1.011530f;

  auto gamma_encode = [](float channel) {
    return channel <= 0.0031308f ? 12.92f * channel : 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
  };
  r = std::max(0.0f, gamma_encode(r));
  g = std::max(0.0f, gamma_encode(g));
  b = std::max(0.0f, gamma_encode(b));

  float maximum = std::max({r, g, b});
  if (maximum > 1.0f) {
    r /= maximum;
    g /= maximum;
    b /= maximum;
  }

  red = r;
  green = g;
  blue = b;
  return true;
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
      uint16_t x = 0;
      uint16_t y = 0;
      rgb_to_xy(values.get_red(), values.get_green(), values.get_blue(), x, y);
      this->zb_->add_attr(this->x_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                          EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, 0, x);
      this->zb_->add_attr(this->y_forwarder_, this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                          EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, 0, y);
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

void ZigbeeLight::apply_x_(uint16_t x) {
  if (this->block_zigbee_)
    return;
  this->x_ = x;
  this->apply_color_();
}

void ZigbeeLight::apply_y_(uint16_t y) {
  if (this->block_zigbee_)
    return;
  this->y_ = y;
  this->apply_color_();
}

void ZigbeeLight::apply_color_() {
  if (this->x_ < 0xFFFF && this->y_ < 0xFFFF) {
    float r, g, b;
    if (xy_to_rgb(this->x_, this->y_, r, g, b)) {
      this->next_call_.set_rgb(r, g, b);
      ESP_LOGD(TAG, "set color: %f, %f, %f", r, g, b);
      this->schedule_call_();
    }
    this->x_ = this->y_ = 0xFFFF;
  }
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
          ESP_LOGI(TAG, "Err %u", ret);
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
    if (this->x_forwarder_ && color_mode & light::ColorCapability::RGB) {
      uint16_t x, y;
      rgb_to_xy(values.get_red(), values.get_green(), values.get_blue(), x, y);
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, EZB_ZCL_STD_MANUF_CODE, &x, false);
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID, EZB_ZCL_STD_MANUF_CODE, &y, false);
      uint8_t mode = 0x01;
      ezb_zcl_set_attr_value(this->endpoint_id_, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
                             EZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, EZB_ZCL_STD_MANUF_CODE, &mode, false);
      ESP_LOGI(TAG, "Set color");
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
