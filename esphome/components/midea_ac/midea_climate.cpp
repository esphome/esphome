#include "esphome/core/log.h"
#include "midea_climate.h"

namespace esphome {
namespace midea_ac {

static const char *const TAG = "midea_ac";

static void set_sensor(Sensor *sensor, float value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
    sensor->publish_state(value);
}

template<typename T> void set_property(T &property, T value, bool &flag) {
  if (property != value) {
    property = value;
    flag = true;
  }
}

void MideaAC::setup() {
  this->dongle_->set_appliance(this);
  this->get_capabilities_();
  if (this->power_sensor_ != nullptr) {
    this->set_interval(30*1000, [this](){
      this->get_power_usage_();
    });
  }
}

ResponseStatus MideaAC::read_status_(const Frame &frame) {
    const auto p = frame.as<PropertiesFrame>();
    if (!p.has_properties())
      return ResponseStatus::RESPONSE_WRONG;
    ESP_LOGD(TAG, "New status data received. Data parsing started...");
    this->cmd_frame_.set_properties(p);  // copy properties from response
    bool need_publish = false;
    set_property(this->mode, p.get_mode(), need_publish);
    set_property(this->target_temperature, p.get_target_temp(), need_publish);
    set_property(this->current_temperature, p.get_indoor_temp(), need_publish);
    if (p.is_custom_fan_mode()) {
      this->fan_mode.reset();
      optional<std::string> mode = p.get_custom_fan_mode();
      set_property(this->custom_fan_mode, mode, need_publish);
    } else {
      this->custom_fan_mode.reset();
      optional<ClimateFanMode> mode = p.get_fan_mode();
      set_property(this->fan_mode, mode, need_publish);
    }
    set_property(this->swing_mode, p.get_swing_mode(), need_publish);
    if (p.is_custom_preset()) {
      this->preset.reset();
      optional<std::string> preset = p.get_custom_preset();
      set_property(this->custom_preset, preset, need_publish);
    } else {
      this->custom_preset.reset();
      set_property(this->preset, p.get_preset(), need_publish);
    }
    if (need_publish)
      this->publish_state();
    set_sensor(this->outdoor_sensor_, p.get_outdoor_temp());
    set_sensor(this->humidity_sensor_, p.get_humidity_setpoint());
    return ResponseStatus::RESPONSE_OK;
}

void MideaAC::on_frame(const Frame &frame) {
  ESP_LOGW(TAG, "RX: frame has unknown type");
}

bool MideaAC::allow_preset(ClimatePreset preset) const {
  switch (preset) {
    case ClimatePreset::CLIMATE_PRESET_ECO:
      if (this->mode == ClimateMode::CLIMATE_MODE_COOL) {
        return true;
      } else {
        ESP_LOGD(TAG, "ECO preset is only available in COOL mode");
      }
      break;
    case ClimatePreset::CLIMATE_PRESET_SLEEP:
      if (this->mode == ClimateMode::CLIMATE_MODE_FAN_ONLY || this->mode == ClimateMode::CLIMATE_MODE_DRY) {
        ESP_LOGD(TAG, "SLEEP preset is not available in FAN_ONLY or DRY mode");
      } else {
        return true;
      }
      break;
    case ClimatePreset::CLIMATE_PRESET_BOOST:
      if ((this->mode == ClimateMode::CLIMATE_MODE_HEAT && this->capabilities_.turbo_heat()) ||
          (this->mode == ClimateMode::CLIMATE_MODE_COOL && this->capabilities_.turbo_cool())) {
        return true;
      } else {
        ESP_LOGD(TAG, "BOOST preset is only available in HEAT or COOL mode");
      }
      break;
    case ClimatePreset::CLIMATE_PRESET_NONE:
      return true;
    default:
      break;
  }
  return false;
}

bool MideaAC::allow_custom_preset(const std::string &custom_preset) const {
  if (custom_preset == Capabilities::FREEZE_PROTECTION) {
    if (this->mode == ClimateMode::CLIMATE_MODE_HEAT)
      return true;
    ESP_LOGD(TAG, "FREEZE_PROTECTION preset is only available in HEAT mode");
  }
  return false;
}

void MideaAC::control(const ClimateCall &call) {
  bool update = false;
  if (call.get_mode().has_value() && call.get_mode().value() != this->mode) {
    this->cmd_frame_.set_mode(call.get_mode().value());
    update = true;
  }
  if (call.get_target_temperature().has_value() && call.get_target_temperature().value() != this->target_temperature) {
    this->cmd_frame_.set_target_temp(call.get_target_temperature().value());
    update = true;
  }
  if (call.get_fan_mode().has_value() &&
      (!this->fan_mode.has_value() || this->fan_mode.value() != call.get_fan_mode().value())) {
    this->custom_fan_mode.reset();
    this->cmd_frame_.set_fan_mode(call.get_fan_mode().value());
    update = true;
  }
  if (call.get_custom_fan_mode().has_value() &&
      (!this->custom_fan_mode.has_value() || this->custom_fan_mode.value() != call.get_custom_fan_mode().value())) {
    this->fan_mode.reset();
    this->cmd_frame_.set_custom_fan_mode(call.get_custom_fan_mode().value());
    update = true;
  }
  if (call.get_swing_mode().has_value() && call.get_swing_mode().value() != this->swing_mode) {
    this->cmd_frame_.set_swing_mode(call.get_swing_mode().value());
    update = true;
  }
  if (call.get_preset().has_value() && this->allow_preset(call.get_preset().value()) &&
      (!this->preset.has_value() || this->preset.value() != call.get_preset().value())) {
    this->custom_preset.reset();
    this->cmd_frame_.set_preset(call.get_preset().value());
    update = true;
  }
  if (call.get_custom_preset().has_value() && this->allow_custom_preset(call.get_custom_preset().value()) &&
      (!this->custom_preset.has_value() || this->custom_preset.value() != call.get_custom_preset().value())) {
    this->preset.reset();
    this->cmd_frame_.set_custom_preset(call.get_custom_preset().value());
    update = true;
  }
  if (update) {
    this->cmd_frame_.set_beeper_feedback(this->beeper_feedback_);
    this->cmd_frame_.update_all();
    ESP_LOGD(TAG, "Enqueuing a priority SET_STATUS(0x40) request...");
    this->dongle_->queue_request_priority(this->cmd_frame_, std::bind(&MideaAC::read_status_, this, std::placeholders::_1));
  }
}

ClimateTraits MideaAC::traits() {
  auto traits = ClimateTraits();
  traits.set_supported_custom_presets(this->traits_custom_presets_);
  traits.set_supported_custom_fan_modes(this->traits_custom_fan_modes_);
  if (this->traits_swing_horizontal_)
    traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_HORIZONTAL);
  if (this->traits_swing_both_)
    traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_BOTH);
  this->capabilities_.to_climate_traits(traits);
  return traits;
}

void MideaAC::get_power_usage_() {
  uint8_t data[] = {0xAA, 0x22, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x41, 0x21,
                    0x01, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x17, 0x6A};
  ESP_LOGD(TAG, "Enqueuing a GET_POWERUSAGE(0x41) request...");
  this->dongle_->queue_request(data, [this](const Frame &frame) -> ResponseStatus{
    const auto p = frame.as<PropertiesFrame>();
    if (!p.has_power_info())
      return ResponseStatus::RESPONSE_WRONG;
    set_sensor(this->power_sensor_, p.get_power_usage());
    return ResponseStatus::RESPONSE_OK;
  });
}

void MideaAC::get_capabilities_() {
  uint8_t data[] = {
    0xAA, 0x0E, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03,
    0xB5, 0x01, 0x00, 0x4D, 0x3D
  };
  ESP_LOGD(TAG, "Enqueuing a priority GET_CAPABILITIES(0xB5) request...");
  this->dongle_->queue_request_priority(data, [this](const Frame &frame) -> ResponseStatus {
    if (!frame.has_id(0xB5))
      return ResponseStatus::RESPONSE_WRONG;
    if (this->capabilities_.read(frame)) {
      uint8_t data[] = {
        0xAA, 0x0F, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03,
        0xB5, 0x01, 0x01, 0x01, 0x21, 0x66
      };
      this->dongle_->send_frame(data);
      return ResponseStatus::RESPONSE_PARTIAL;
    }
    return ResponseStatus::RESPONSE_OK;
  });
}

void MideaAC::get_status_() {
  uint8_t data[] = {0xAA, 0x21, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
                    0x41, 0x81, 0x00, 0xFF, 0x03, 0xFF, 0x00, 0x02, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x03, 0x00, 0x37, 0x31};
  ESP_LOGD(TAG, "Enqueuing a GET_STATUS(0x41) request...");
  this->dongle_->queue_request(data, std::bind(&MideaAC::read_status_, this, std::placeholders::_1));
}

void MideaAC::display_toggle_() {
  uint8_t data[] = {0xAA, 0x21, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
                    0x41, 0x61, 0x00, 0xFF, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0xE3, 0xA8};
  ESP_LOGD(TAG, "Enqueuing a priority TOGGLE_LIGHT(0x41) request...");
  this->dongle_->queue_request_priority(data, std::bind(&MideaAC::read_status_, this, std::placeholders::_1));
}

void MideaAC::dump_config() {
  this->capabilities_.dump(TAG);
  this->dump_traits(TAG);
}

/* ACTIONS */

void MideaAC::do_follow_me(float temperature, bool beeper) {
#ifdef USE_REMOTE_TRANSMITTER
  IrFollowMeData data(static_cast<uint8_t>(lroundf(temperature)), beeper);
  this->transmit_ir_(data);
#else
  ESP_LOGW(TAG, "Action needs remote_transmitter component");
#endif
}

void MideaAC::do_swing_step() {
#ifdef USE_REMOTE_TRANSMITTER
  IrSpecialData data(0x01);
  this->transmit_ir_(data);
#else
  ESP_LOGW(TAG, "Action needs remote_transmitter component");
#endif
}

void MideaAC::do_display_toggle() {
  if (this->capabilities_.light_control()) {
    this->display_toggle_();
  } else {
#ifdef USE_REMOTE_TRANSMITTER
    IrSpecialData data(0x08);
    this->transmit_ir_(data);
#else
    ESP_LOGW(TAG, "Action needs remote_transmitter component");
#endif
  }
}

void MideaAC::do_beeper_on() {
  ESP_LOGD(TAG, "Beeper Feedback Turning ON...");
  this->set_beeper_feedback(true);
}

void MideaAC::do_beeper_off() {
  ESP_LOGD(TAG, "Beeper Feedback Turning OFF...");
  this->set_beeper_feedback(false);
}

}  // namespace midea_ac
}  // namespace esphome
