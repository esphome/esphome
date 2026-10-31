#include "ld6004.h"
#include "esphome/components/ld600x/ld600x_frame.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cmath>

namespace esphome::ld6004 {

ESPHOME_LOG_TAG(TAG, "ld6004");

static constexpr uint16_t TYPE_SET_DWELL_LIFETIME = 0x0206;
static constexpr uint16_t TYPE_SET_OUTPUT_INTERVAL = 0x0207;
static constexpr uint16_t TYPE_REPORT_TARGET = 0x0A04;
static constexpr uint16_t TYPE_REPORT_AREA_PRESENCE = 0x0A0A;
static constexpr uint16_t TYPE_REPORT_WORK_MODE = 0x0A12;
static constexpr uint16_t TYPE_REPORT_WORK_STATUS = 0x0A14;
static constexpr uint16_t TYPE_REPORT_P20_MODE = 0x0A15;
static constexpr uint16_t TYPE_REPORT_DWELL_LIFETIME = 0x0A17;
static constexpr uint16_t TYPE_REPORT_OUTPUT_INTERVAL = 0x0A18;

static constexpr uint32_t CMD_WORK_LOW_POWER = 0x16;
static constexpr uint32_t CMD_WORK_NORMAL = 0x17;
static constexpr uint32_t CMD_GET_WORK_MODE = 0x18;
static constexpr uint32_t CMD_WORK_RADAR_OFF_P20_HIGH = 0x1B;
static constexpr uint32_t CMD_WORK_RADAR_OFF_P20_LOW = 0x1C;
static constexpr uint32_t CMD_P20_PRESENCE_HIGH = 0x1D;
static constexpr uint32_t CMD_P20_PRESENCE_LOW = 0x1E;
static constexpr uint32_t CMD_GET_P20_MODE = 0x1F;
static constexpr uint32_t CMD_P20_CONSTANT_LOW = 0x20;
static constexpr uint32_t CMD_P20_CONSTANT_HIGH = 0x21;
static constexpr uint32_t CMD_P20_PULSE_LOW = 0x22;
static constexpr uint32_t CMD_P20_PULSE_HIGH = 0x23;
static constexpr uint32_t CMD_WORK_HIGH_REFLECTIVITY = 0x24;
static constexpr uint32_t CMD_CLEAR_DWELL = 0x25;
static constexpr uint32_t CMD_GET_DWELL_LIFETIME = 0x26;
static constexpr uint32_t CMD_GET_OUTPUT_INTERVAL = 0x27;

// The work_mode select options, the 0x0A12 report values and the tables below share one order.
static constexpr uint8_t WORK_MODE_COUNT = 5;
static constexpr uint8_t WORK_MODE_RADAR_OFF_P20_HIGH = 2;
static constexpr uint8_t WORK_MODE_RADAR_OFF_P20_LOW = 3;
static constexpr uint8_t P20_MODE_COUNT = 6;
#ifdef USE_SELECT
// Every control code fits in a byte, so the tables are bytes in flash.
static constexpr uint8_t WORK_MODE_COMMANDS[WORK_MODE_COUNT] PROGMEM = {
    CMD_WORK_NORMAL, CMD_WORK_LOW_POWER, CMD_WORK_RADAR_OFF_P20_HIGH, CMD_WORK_RADAR_OFF_P20_LOW,
    CMD_WORK_HIGH_REFLECTIVITY};
static constexpr uint8_t P20_MODE_COMMANDS[P20_MODE_COUNT] PROGMEM = {CMD_P20_PRESENCE_HIGH, CMD_P20_PRESENCE_LOW,
                                                                      CMD_P20_CONSTANT_LOW,  CMD_P20_CONSTANT_HIGH,
                                                                      CMD_P20_PULSE_LOW,     CMD_P20_PULSE_HIGH};
#endif
#ifdef USE_TEXT_SENSOR
PROGMEM_STRING_TABLE(WorkModeStrings, "normal", "low_power", "radar_off_p20_high", "radar_off_p20_low",
                     "high_reflectivity");
static_assert(WorkModeStrings::COUNT == WORK_MODE_COUNT);
#endif

// The mode reports carry one byte or a little-endian uint32. Any other length returns UINT32_MAX, which the
// callers' range checks reject.
static uint32_t read_mode(const uint8_t *data, uint16_t len) {
  if (len == 1)
    return data[0];
  if (len == 4)
    return encode_value_le<uint32_t>(data);
  return UINT32_MAX;
}

bool LD6004Component::handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) {
  switch (type) {
    case TYPE_REPORT_WORK_MODE: {
      const uint32_t mode = read_mode(data, len);
      if (mode >= WORK_MODE_COUNT)
        break;
      this->radar_off_ = mode == WORK_MODE_RADAR_OFF_P20_HIGH || mode == WORK_MODE_RADAR_OFF_P20_LOW;
      if (this->radar_off_) {
        // Detection has stopped, so no later report clears what the last ones showed.
        this->clear_area_presence_();
        this->clear_target_state_();
      }
#ifdef USE_SELECT
      PUBLISH_STATE(this->work_mode_select_, mode);
#endif
#ifdef USE_TEXT_SENSOR
      if (this->work_mode_text_sensor_ != nullptr)
        this->work_mode_text_sensor_->publish_state(WorkModeStrings::get_progmem_str(static_cast<uint8_t>(mode), 0));
#endif
      return true;
    }
    case TYPE_REPORT_TARGET:
    case TYPE_REPORT_AREA_PRESENCE:
      // Claimed while the radar is off: frames still in flight must not restore the cleared presence.
      return this->radar_off_;
    case TYPE_REPORT_WORK_STATUS:
      // 0x0A14 marks the unattended low-power transition; a zero means nobody is in any area.
      if (len >= 1 && data[0] == 0 && !this->target_presence_any_)
        this->clear_area_presence_();
      return true;
    case TYPE_REPORT_P20_MODE: {
      const uint32_t mode = read_mode(data, len);
      if (mode >= P20_MODE_COUNT)
        break;
#ifdef USE_SELECT
      PUBLISH_STATE(this->p20_mode_select_, mode);
#endif
      return true;
    }
    case TYPE_REPORT_DWELL_LIFETIME:
      if (len != 4)
        break;
#ifdef USE_NUMBER
      this->publish_number_clamped_(this->dwell_lifetime_number_, encode_value_le<uint32_t>(data));
#endif
      return true;
    case TYPE_REPORT_OUTPUT_INTERVAL:
      if (len != 4)
        break;
#ifdef USE_NUMBER
      this->publish_number_clamped_(this->output_interval_number_, encode_value_le<uint32_t>(data));
#endif
      return true;
    default:
      return false;
  }
  // Still claimed, so the base does not treat it as an unknown frame.
  ESP_LOGD(TAG, "Unexpected report 0x%04X: %u bytes, first byte 0x%02X", type, len, len > 0 ? data[0] : 0);
  return true;
}

#ifdef USE_NUMBER
void LD6004Component::set_number_value(number::Number *entity, float value) {
  if (entity != this->dwell_lifetime_number_ && entity != this->output_interval_number_) {
    this->LD600XComponent::set_number_value(entity, value);
    return;
  }
  // NumberCall already keeps the value inside the traits' range; the module takes whole numbers only.
  if (std::floor(value) != value) {
    ESP_LOGW(TAG, "Invalid number value: %f", value);
    return;
  }
  uint8_t data[4];
  ld600x::write_u32_le(data, static_cast<uint32_t>(value));
  this->queue_command_(entity == this->dwell_lifetime_number_ ? TYPE_SET_DWELL_LIFETIME : TYPE_SET_OUTPUT_INTERVAL,
                       data, sizeof(data));
}
#endif  // USE_NUMBER

#ifdef USE_SELECT
bool LD6004Component::set_select_value(select::Select *entity, size_t index) {
  if (entity == this->work_mode_select_)
    return index < WORK_MODE_COUNT && this->send_control_command_(progmem_read_byte(&WORK_MODE_COMMANDS[index]));
  if (entity == this->p20_mode_select_)
    return index < P20_MODE_COUNT && this->send_control_command_(progmem_read_byte(&P20_MODE_COMMANDS[index]));
  return this->LD600XComponent::set_select_value(entity, index);
}
#endif  // USE_SELECT

#ifdef USE_BUTTON
void LD6004Component::press_button(uint8_t kind) {
  if (kind == BUTTON_CLEAR_DWELL) {
    if (this->send_control_command_(CMD_CLEAR_DWELL)) {
      this->LD600XComponent::press_button(ld600x::BUTTON_GET_AREAS);
    }
  } else {
    this->LD600XComponent::press_button(kind);
  }
}
#endif  // USE_BUTTON

void LD6004Component::setup_model() {
  bool want_work_mode = false;
#ifdef USE_SELECT
  want_work_mode = this->work_mode_select_ != nullptr;
  if (this->p20_mode_select_ != nullptr)
    this->send_control_command_(CMD_GET_P20_MODE);
#endif
#ifdef USE_TEXT_SENSOR
  // The text sensor follows the 0x0A12 report too, so it needs the same boot query as the select
  // to show a mode before the module next changes it.
  want_work_mode = want_work_mode || this->work_mode_text_sensor_ != nullptr;
#endif
  if (want_work_mode)
    this->send_control_command_(CMD_GET_WORK_MODE);
#ifdef USE_NUMBER
  if (this->dwell_lifetime_number_ != nullptr)
    this->send_control_command_(CMD_GET_DWELL_LIFETIME);
  if (this->output_interval_number_ != nullptr)
    this->send_control_command_(CMD_GET_OUTPUT_INTERVAL);
#endif
}

void LD6004Component::dump_model_config() {
#ifdef USE_NUMBER
  LOG_NUMBER("  ", "Dwell Lifetime", this->dwell_lifetime_number_);
  LOG_NUMBER("  ", "Output Interval", this->output_interval_number_);
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "Work Mode", this->work_mode_select_);
  LOG_SELECT("  ", "P20 Mode", this->p20_mode_select_);
#endif
}

}  // namespace esphome::ld6004
