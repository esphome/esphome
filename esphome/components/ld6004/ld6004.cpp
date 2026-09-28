#include "ld6004.h"
#include "esphome/components/ld600x/ld600x_frame.h"
#include "esphome/core/log.h"
#include <array>
#include <cmath>

namespace esphome::ld6004 {

static const char *const TAG = "ld6004";

static constexpr uint16_t TYPE_SET_DWELL_LIFETIME = 0x0206;
static constexpr uint16_t TYPE_SET_OUTPUT_INTERVAL = 0x0207;
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

static constexpr std::array<uint32_t, 5> WORK_MODE_COMMANDS = {CMD_WORK_NORMAL, CMD_WORK_LOW_POWER,
                                                               CMD_WORK_RADAR_OFF_P20_HIGH, CMD_WORK_RADAR_OFF_P20_LOW,
                                                               CMD_WORK_HIGH_REFLECTIVITY};
#ifdef USE_TEXT_SENSOR
// Same order as the work_mode select options and the 0x0A12 report values.
static const char *const WORK_MODE_NAMES[] = {"normal", "low_power", "radar_off_p20_high", "radar_off_p20_low",
                                              "high_reflectivity"};
#endif
static constexpr std::array<uint32_t, 6> P20_MODE_COMMANDS = {CMD_P20_PRESENCE_HIGH, CMD_P20_PRESENCE_LOW,
                                                              CMD_P20_CONSTANT_LOW,  CMD_P20_CONSTANT_HIGH,
                                                              CMD_P20_PULSE_LOW,     CMD_P20_PULSE_HIGH};

bool LD6004Component::handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) {
  switch (type) {
    case TYPE_REPORT_WORK_MODE: {
      if (len != 1 && len != 4)
        return true;
      const uint32_t mode = len == 4 ? ld600x::read_u32_le(data) : data[0];
      if (mode >= WORK_MODE_COMMANDS.size())
        return true;
#ifdef USE_SELECT
      if (this->work_mode_select_ != nullptr)
        this->work_mode_select_->publish_state(mode);
#endif
#ifdef USE_TEXT_SENSOR
      // The LD6004 has five modes, so the text sensor follows this report instead of the base's
      // two-state fallback, which is disarmed by marking the mode as reported.
      if (this->work_mode_text_sensor_ != nullptr) {
        this->work_mode_reported_ = true;
        this->work_mode_text_sensor_->publish_state(WORK_MODE_NAMES[mode]);
      }
#endif
      this->low_power_enabled_ = mode == 1;
      this->low_power_reported_ = true;
      return true;
    }
    case TYPE_REPORT_WORK_STATUS:
      // 0x0A14 marks the unattended low-power transition. The base would also publish it as a
      // two-state work mode; here only its presence side effect is kept.
      if (len >= 1 && data[0] == 0 && !this->target_presence_any_)
        this->clear_area_presence_();
      return true;
    case TYPE_REPORT_P20_MODE:
#ifdef USE_SELECT
      if (len == 1 && data[0] < P20_MODE_COMMANDS.size() && this->p20_mode_select_ != nullptr)
        this->p20_mode_select_->publish_state(data[0]);
#endif
      return true;
    case TYPE_REPORT_DWELL_LIFETIME:
#ifdef USE_NUMBER
      if (len == 4)
        this->publish_number_clamped_(this->dwell_lifetime_number_, ld600x::read_u32_le(data));
#endif
      return true;
    case TYPE_REPORT_OUTPUT_INTERVAL:
#ifdef USE_NUMBER
      if (len == 4)
        this->publish_number_clamped_(this->output_interval_number_, ld600x::read_u32_le(data));
#endif
      return true;
    default:
      return false;
  }
}

void LD6004Component::set_number_value(uint8_t kind, float value) {
  if (kind != NUMBER_DWELL_LIFETIME && kind != NUMBER_OUTPUT_INTERVAL) {
    this->LD600XComponent::set_number_value(kind, value);
    return;
  }
  const float minimum = kind == NUMBER_OUTPUT_INTERVAL ? 1.0f : 0.0f;
  // UINT32_MAX rounds up to 2^32 as a float. Reject that value before converting it.
  if (!std::isfinite(value) || value < minimum || value >= 4294967296.0f || std::floor(value) != value) {
    ESP_LOGW(TAG, "Invalid number value: %f", value);
    return;
  }
  uint8_t data[4];
  ld600x::write_u32_le(data, static_cast<uint32_t>(value));
  this->queue_command_(kind == NUMBER_DWELL_LIFETIME ? TYPE_SET_DWELL_LIFETIME : TYPE_SET_OUTPUT_INTERVAL, data,
                       sizeof(data));
}

void LD6004Component::set_select_value(uint8_t kind, size_t index) {
  switch (kind) {
    case SELECT_WORK_MODE:
      if (index < WORK_MODE_COMMANDS.size())
        this->send_control_command_(WORK_MODE_COMMANDS[index]);
      break;
    case SELECT_P20_MODE:
      if (index < P20_MODE_COMMANDS.size())
        this->send_control_command_(P20_MODE_COMMANDS[index]);
      break;
    default:
      this->LD600XComponent::set_select_value(kind, index);
      break;
  }
}

void LD6004Component::press_button(uint8_t kind) {
  if (kind == BUTTON_CLEAR_DWELL) {
    this->send_control_command_(CMD_CLEAR_DWELL);
    this->LD600XComponent::press_button(ld600x::BUTTON_GET_AREAS);
  } else {
    this->LD600XComponent::press_button(kind);
  }
}

void LD6004Component::setup_model() {
#ifdef USE_SELECT
  if (this->work_mode_select_ != nullptr)
    this->send_control_command_(CMD_GET_WORK_MODE);
  if (this->p20_mode_select_ != nullptr)
    this->send_control_command_(CMD_GET_P20_MODE);
#endif
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
