#if (defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF)

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "air_conditioner.h"
#include "ac_adapter.h"
#include <cmath>
#include <cstdint>

namespace esphome::midea::ac {

static void set_sensor(Sensor *sensor, float value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
    sensor->publish_state(value);
}

template<typename T> void update_property(T &property, const T &value, bool &flag) {
  if (property != value) {
    property = value;
    flag = true;
  }
}

void AirConditioner::on_status_change() {
  // Add frost protection custom preset once when autoconf completes
  if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_OK &&
      this->base_.getCapabilities().supportFrostProtectionPreset() && !this->frost_protection_set_) {
    // Read existing presets (set by codegen), append frost protection, write back
    auto traits = this->get_traits();
    const auto &existing = traits.get_supported_custom_presets();
    bool found = false;
    for (const char *p : existing) {
      if (strcmp(p, Constants::FREEZE_PROTECTION) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      std::vector<const char *> merged(existing.begin(), existing.end());
      merged.push_back(Constants::FREEZE_PROTECTION);
      this->set_supported_custom_presets(merged);
    }
    this->frost_protection_set_ = true;
  }
  bool need_publish = false;
  update_property(this->target_temperature, this->base_.getTargetTemp(), need_publish);
  update_property(this->current_temperature, this->base_.getIndoorTemp(), need_publish);
  auto mode = Converters::to_climate_mode(this->base_.getMode());
  update_property(this->mode, mode, need_publish);
  auto swing_mode = Converters::to_climate_swing_mode(this->base_.getSwingMode());
  update_property(this->swing_mode, swing_mode, need_publish);
  // Preset
  auto preset = this->base_.getPreset();
  if (Converters::is_custom_midea_preset(preset)) {
    if (this->set_custom_preset_(Converters::to_custom_climate_preset(preset)))
      need_publish = true;
  } else if (this->set_preset_(Converters::to_climate_preset(preset))) {
    need_publish = true;
  }
  // Fan mode
  auto fan_mode = this->base_.getFanMode();
  if (Converters::is_custom_midea_fan_mode(fan_mode)) {
    if (this->set_custom_fan_mode_(Converters::to_custom_climate_fan_mode(fan_mode)))
      need_publish = true;
  } else if (this->set_fan_mode_(Converters::to_climate_fan_mode(fan_mode))) {
    need_publish = true;
  }
  if (need_publish)
    this->publish_state();
  set_sensor(this->outdoor_temperature_sensor_, this->base_.getOutdoorTemp());
  set_sensor(this->power_sensor_, this->base_.getPowerUsage());
  set_sensor(this->humidity_setpoint_sensor_, this->base_.getIndoorHum());
  this->publish_appliance_state_();
}

/* ################ */
/* ### ENTITIES ### */
/* ################ */

void AirConditioner::setup() {
  this->request_appliance_data_();
  ApplianceBase::setup();
}

// Poll only what the configured entities need
void AirConditioner::request_appliance_data_() {
  auto &b = this->base_;
  if (this->compressor_frequency_sensor_ != nullptr || this->compressor_target_frequency_sensor_ != nullptr ||
      this->compressor_current_sensor_ != nullptr || this->compressor_voltage_sensor_ != nullptr ||
      this->indoor_coil_temperature_sensor_ != nullptr || this->outdoor_coil_temperature_sensor_ != nullptr ||
      this->discharge_temperature_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_1);
  bool group2 = this->indoor_fan_speed_sensor_ != nullptr || this->indoor_fan_target_speed_sensor_ != nullptr;
  bool group5 = this->indoor_humidity_sensor_ != nullptr || this->outdoor_fan_speed_sensor_ != nullptr;
#ifdef USE_BINARY_SENSOR
  group2 |= this->water_pump_binary_sensor_ != nullptr;
  group5 |= this->defrost_binary_sensor_ != nullptr;
#endif
  if (group2)
    b.request_group(MideaGroup::MIDEA_GROUP_2);
  if (group5)
    b.request_group(MideaGroup::MIDEA_GROUP_5);
  if (this->total_energy_sensor_ != nullptr || this->current_energy_sensor_ != nullptr ||
      this->real_time_power_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_4);
  if (this->outdoor_unit_power_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_7);
  if (this->horizontal_louver_angle_sensor_ != nullptr || this->vertical_louver_angle_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_11);
}

static void publish_known(Sensor *sensor, float value) {
  if (std::isfinite(value))
    set_sensor(sensor, value);
}

#ifdef USE_BINARY_SENSOR
static void publish_binary(binary_sensor::BinarySensor *sensor, bool value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->state != value))
    sensor->publish_state(value);
}
#endif

void AirConditioner::publish_appliance_state_() {
  const auto &e = this->base_.get_appliance_state();
  if (e.status_valid)
    set_sensor(this->error_code_sensor_, e.error_code);
  publish_known(this->compressor_frequency_sensor_, e.compressor_frequency);
  publish_known(this->compressor_target_frequency_sensor_, e.compressor_target_frequency);
  publish_known(this->compressor_current_sensor_, e.compressor_current);
  publish_known(this->compressor_voltage_sensor_, e.compressor_voltage);
  publish_known(this->indoor_coil_temperature_sensor_, e.indoor_coil_temperature);
  publish_known(this->outdoor_coil_temperature_sensor_, e.outdoor_coil_temperature);
  publish_known(this->discharge_temperature_sensor_, e.discharge_temperature);
  publish_known(this->indoor_fan_speed_sensor_, e.indoor_fan_speed);
  publish_known(this->indoor_fan_target_speed_sensor_, e.indoor_fan_target_speed);
  publish_known(this->total_energy_sensor_, e.total_energy);
  publish_known(this->current_energy_sensor_, e.current_energy);
  publish_known(this->real_time_power_sensor_, e.real_time_power);
  publish_known(this->indoor_humidity_sensor_, e.indoor_humidity);
  publish_known(this->outdoor_fan_speed_sensor_, e.outdoor_fan_speed);
  publish_known(this->outdoor_unit_power_sensor_, e.outdoor_unit_power);
  publish_known(this->horizontal_louver_angle_sensor_, e.horizontal_louver_angle);
  publish_known(this->vertical_louver_angle_sensor_, e.vertical_louver_angle);
#ifdef USE_BINARY_SENSOR
  if (e.defrost >= 0)
    publish_binary(this->defrost_binary_sensor_, e.defrost);
  if (e.water_pump >= 0)
    publish_binary(this->water_pump_binary_sensor_, e.water_pump);
  if (e.status_valid) {
    publish_binary(this->filter_alert_binary_sensor_, e.filter_alert);
    publish_binary(this->display_binary_sensor_, e.display_on);
    publish_binary(this->follow_me_binary_sensor_, e.follow_me);
  }
#endif
}

void AirConditioner::control(const ClimateCall &call) {
  dudanov::midea::ac::Control ctrl{};
  auto target_temp_val = call.get_target_temperature();
  if (target_temp_val.has_value())
    ctrl.targetTemp = *target_temp_val;
  auto swing_mode_val = call.get_swing_mode();
  if (swing_mode_val.has_value())
    ctrl.swingMode = Converters::to_midea_swing_mode(*swing_mode_val);
  auto mode_val = call.get_mode();
  if (mode_val.has_value())
    ctrl.mode = Converters::to_midea_mode(*mode_val);
  auto preset_val = call.get_preset();
  if (preset_val.has_value()) {
    ctrl.preset = Converters::to_midea_preset(*preset_val);
  } else if (call.has_custom_preset()) {
    // get_custom_preset() returns StringRef pointing to null-terminated string literals from codegen
    ctrl.preset = Converters::to_midea_preset(call.get_custom_preset().c_str());
  }
  auto fan_mode_val = call.get_fan_mode();
  if (fan_mode_val.has_value()) {
    ctrl.fanMode = Converters::to_midea_fan_mode(*fan_mode_val);
  } else if (call.has_custom_fan_mode()) {
    // get_custom_fan_mode() returns StringRef pointing to null-terminated string literals from codegen
    ctrl.fanMode = Converters::to_midea_fan_mode(call.get_custom_fan_mode().c_str());
  }
  this->base_.control(ctrl);
}

ClimateTraits AirConditioner::traits() {
  auto traits = ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(0.5);
  traits.set_supported_modes(this->supported_modes_);
  traits.set_supported_swing_modes(this->supported_swing_modes_);
  traits.set_supported_presets(this->supported_presets_);
  // Custom fan modes and presets are stored on Climate base class and wired via get_traits()
  /* + MINIMAL SET OF CAPABILITIES */
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_HIGH);
  if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_OK) {
    Converters::to_climate_traits(traits, this->base_.getCapabilities());
  }
  if (!traits.get_supported_modes().empty())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_OFF);
  if (!traits.get_supported_swing_modes().empty())
    traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_OFF);
  if (!traits.get_supported_presets().empty())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);
  return traits;
}

#ifdef USE_REMOTE_TRANSMITTER
static const LogString *display_control_str(DisplayControl control) {
  switch (control) {
    case DisplayControl::DISPLAY_CONTROL_UART:
      return LOG_STR("UART");
    case DisplayControl::DISPLAY_CONTROL_IR:
      return LOG_STR("IR");
    default:
      return LOG_STR("AUTO");
  }
}
#endif

void AirConditioner::dump_config() {
  ESP_LOGCONFIG(Constants::TAG,
                "MideaDongle:\n"
                "  [x] Period: %" PRIu32 "ms\n"
                "  [x] Response timeout: %" PRIu32 "ms\n"
                "  [x] Request attempts: %d",
                this->base_.getPeriod(), this->base_.getTimeout(), this->base_.getNumAttempts());
#ifdef USE_REMOTE_TRANSMITTER
  ESP_LOGCONFIG(Constants::TAG,
                "  [x] Using RemoteTransmitter\n"
                "  [x] Display control: %s",
                LOG_STR_ARG(display_control_str(this->display_control_)));
#endif
  if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_OK) {
    this->base_.getCapabilities().dump();
  } else if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_ERROR) {
    ESP_LOGW(Constants::TAG,
             "Failed to get 0xB5 capabilities report. Suggest to disable it in config and manually set your "
             "appliance options.");
  }
  this->dump_traits_(Constants::TAG);
}

/* ACTIONS */

void AirConditioner::do_follow_me(float temperature, bool use_fahrenheit, bool beeper) {
#ifdef USE_REMOTE_TRANSMITTER
  // Check if temperature is finite (not NaN or infinite)
  if (!std::isfinite(temperature)) {
    ESP_LOGW(Constants::TAG, "Follow me action requires a finite temperature, got: %f", temperature);
    return;
  }

  // Clamp to the uint8_t range before rounding so lroundf never sees an out-of-range value
  uint8_t temp_uint8 = static_cast<uint8_t>(std::lroundf(esphome::clamp(temperature, 0.0f, 255.0f)));

  char temp_symbol = use_fahrenheit ? 'F' : 'C';
  ESP_LOGD(Constants::TAG, "Follow me action called with temperature: %.5f °%c, rounded to: %u °%c", temperature,
           temp_symbol, temp_uint8, temp_symbol);

  // Create and transmit the data
  IrFollowMeData data(temp_uint8, use_fahrenheit, beeper);
  this->transmitter_.transmit(data);
#else
  ESP_LOGW(Constants::TAG, "Action needs remote_transmitter component");
#endif
}

void AirConditioner::do_swing_step() {
#ifdef USE_REMOTE_TRANSMITTER
  IrSpecialData data(0x01);
  this->transmitter_.transmit(data);
#else
  ESP_LOGW(Constants::TAG, "Action needs remote_transmitter component");
#endif
}

void AirConditioner::do_display_toggle() {
#ifdef USE_REMOTE_TRANSMITTER
  // AUTO uses IR unless the appliance reports light control over UART. Without a transmitter
  // built in, every setting sends the UART request: Mundoclima, Rotenso and others accept it
  // without advertising it in the 0xB5 capabilities report.
  const bool use_ir = this->display_control_ == DisplayControl::DISPLAY_CONTROL_IR ||
                      (this->display_control_ == DisplayControl::DISPLAY_CONTROL_AUTO &&
                       !this->base_.getCapabilities().supportLightControl());
  if (use_ir) {
    IrSpecialData data(0x08);
    this->transmitter_.transmit(data);
    return;
  }
#endif
  this->base_.displayToggle();
}

using dudanov::midea::FrameData;
using dudanov::midea::FrameType;
using dudanov::midea::ResponseStatus;

static constexpr uint8_t GROUP_IDS[] = {1, 2, 4, 5, 7, 11};
static_assert(sizeof(GROUP_IDS) == static_cast<uint8_t>(MideaGroup::MIDEA_GROUP_COUNT));

/* ################## */
/* ### FRAME DATA ### */
/* ################## */

// 0x41 group data query: {0x41, 0x21, 0x01, 0x40 | group, 0...} as sent by midea-msmart
class GroupQueryData : public FrameData {
 public:
  explicit GroupQueryData(uint8_t group) : FrameData(20) {
    this->m_data[0] = 0x41;
    this->m_data[1] = 0x21;
    this->m_data[2] = 0x01;
    this->m_data[3] = 0x40 | group;
    this->m_data.push_back(FrameData::m_getID());
    this->appendCRC();
  }
};

/* ############### */
/* ### POLLING ### */
/* ############### */

void Appliance::m_onIdle() {
  // Group queries take turns with the status query, so the climate state keeps its refresh rate
  if (!this->status_turn_ && this->queue_next_data_query_()) {
    this->status_turn_ = true;
    return;
  }
  this->status_turn_ = false;
  this->queue_status_query_();
}

bool Appliance::queue_next_data_query_() {
  constexpr uint8_t group_count = static_cast<uint8_t>(MideaGroup::MIDEA_GROUP_COUNT);
  for (uint8_t tries = 0; tries < group_count; ++tries) {
    const uint8_t idx = this->next_query_;
    this->next_query_ = (idx + 1) % group_count;
    if (this->polled_groups_ & (1U << idx)) {
      this->queue_group_query_(static_cast<MideaGroup>(idx));
      return true;
    }
  }
  return false;
}

void Appliance::queue_status_query_() {
  dudanov::midea::ac::QueryStateData data{};
  ESP_LOGV(Constants::TAG, "Enqueuing a GET_STATUS(0x41) request");
  this->m_queueRequest(FrameType::DEVICE_QUERY, std::move(data), [this](FrameData data) -> ResponseStatus {
    if (!data.hasStatus())
      return ResponseStatus::RESPONSE_WRONG;
    const bool changed = this->read_status_fields_(data);
    const ResponseStatus status = this->m_readStatus(std::move(data));
    if (changed)
      this->sendUpdate();
    return status;
  });
}

void Appliance::queue_group_query_(MideaGroup group) {
  const uint8_t id = GROUP_IDS[static_cast<uint8_t>(group)];
  ESP_LOGV(Constants::TAG, "Enqueuing a GET_GROUP_DATA(0x41) request for group %u", id);
  GroupQueryData data(id);
  this->m_queueRequest(FrameType::DEVICE_QUERY, std::move(data),
                       [this, group](FrameData data) -> ResponseStatus { return this->read_group_(group, data); });
}

/* ############### */
/* ### PARSING ### */
/* ############### */

template<typename T> static void update(T &property, T value, bool &changed) {
  // NAN never equals itself, so a first report always counts as a change
  if (!(property == value)) {
    property = value;
    changed = true;
  }
}

bool Appliance::read_status_fields_(const FrameData &data) {
  const uint8_t *d = data.data();
  const uint8_t size = data.size();
  if (size < 18)
    return false;
  bool changed = false;
  update(this->state_.status_valid, true, changed);
  update(this->state_.follow_me, static_cast<bool>(d[8] & 0x80), changed);
  update(this->state_.filter_alert, static_cast<bool>(d[13] & 0x20), changed);
  update(this->state_.display_on, d[14] != 0x70, changed);
  update(this->state_.error_code, d[16], changed);
  return changed;
}

static uint8_t bcd(uint8_t value) { return 10 * (value >> 4) + (value & 0x0F); }

ResponseStatus Appliance::read_group_(MideaGroup group, const FrameData &data) {
  const uint8_t *d = data.data();
  const uint8_t size = data.size();
  if (!data.hasID(0xC1) || size < 4 || (d[3] & 0x0F) != (GROUP_IDS[static_cast<uint8_t>(group)] & 0x0F))
    return ResponseStatus::RESPONSE_WRONG;
  bool changed = false;
  auto &e = this->state_;
  switch (group) {
    case MideaGroup::MIDEA_GROUP_1:
      if (size < 17)
        break;
      update(e.compressor_frequency, static_cast<float>(d[4]), changed);
      update(e.compressor_target_frequency, static_cast<float>(d[5]), changed);
      update(e.compressor_current, static_cast<float>(d[7]), changed);
      update(e.compressor_voltage, static_cast<float>(d[8]), changed);
      update(e.indoor_coil_temperature, (static_cast<float>(d[11]) - 30.0f) * 0.5f, changed);
      update(e.outdoor_coil_temperature, (static_cast<float>(d[12]) - 50.0f) * 0.5f, changed);
      update(e.discharge_temperature, static_cast<float>(d[14]), changed);
      break;
    case MideaGroup::MIDEA_GROUP_2:
      if (size < 11)
        break;
      update(e.indoor_fan_target_speed, static_cast<float>(d[4] * 8), changed);
      update(e.indoor_fan_speed, static_cast<float>(d[5] * 8), changed);
      update(e.water_pump, static_cast<int8_t>((d[8] & 0x10) != 0), changed);
      break;
    case MideaGroup::MIDEA_GROUP_4: {
      if (size < 21)
        break;
      float total, current, power;
      if (this->energy_format_ == MideaEnergyFormat::MIDEA_ENERGY_FORMAT_BINARY) {
        auto u32 = [d](uint8_t i) {
          return (static_cast<uint32_t>(d[i]) << 24) | (static_cast<uint32_t>(d[i + 1]) << 16) |
                 (static_cast<uint32_t>(d[i + 2]) << 8) | d[i + 3];
        };
        total = static_cast<float>(u32(4)) * 0.1f;
        current = static_cast<float>(u32(12)) * 0.1f;
        power = static_cast<float>((static_cast<uint32_t>(d[16]) << 16) | (static_cast<uint32_t>(d[17]) << 8) | d[18]) *
                0.1f;
      } else {
        auto energy = [d](uint8_t i) {
          return 10000.0f * bcd(d[i]) + 100.0f * bcd(d[i + 1]) + bcd(d[i + 2]) + 0.01f * bcd(d[i + 3]);
        };
        total = energy(4);
        current = energy(12);
        power = 1000.0f * bcd(d[16]) + 10.0f * bcd(d[17]) + 0.1f * bcd(d[18]);
      }
      // All zero means the unit has no energy metering
      if (total == 0.0f && current == 0.0f && power == 0.0f)
        break;
      update(e.total_energy, total, changed);
      update(e.current_energy, current, changed);
      update(e.real_time_power, power, changed);
      break;
    }
    case MideaGroup::MIDEA_GROUP_5:
      if (size < 13)
        break;
      update(e.indoor_humidity, d[4] != 0 ? static_cast<float>(d[4]) : NAN, changed);
      update(e.outdoor_fan_speed, static_cast<float>(d[8] * 8), changed);
      update(e.defrost, static_cast<int8_t>(d[10] != 0), changed);
      break;
    case MideaGroup::MIDEA_GROUP_7:
      if (size < 14)
        break;
      update(e.outdoor_unit_power, static_cast<float>(d[10] | (d[11] << 8)), changed);
      break;
    case MideaGroup::MIDEA_GROUP_11:
      if (size < 15)
        break;
      update(e.horizontal_louver_angle, static_cast<float>(d[9]), changed);
      update(e.vertical_louver_angle, static_cast<float>(d[12]), changed);
      break;
    default:
      break;
  }
  if (changed)
    this->sendUpdate();
  return ResponseStatus::RESPONSE_OK;
}

}  // namespace esphome::midea::ac

#endif  // USE_ARDUINO || USE_ESP_IDF
