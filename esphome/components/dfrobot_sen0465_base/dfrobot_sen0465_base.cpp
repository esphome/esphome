#include "dfrobot_sen0465_base.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::dfrobot_sen0465_base {

ESPHOME_LOG_TAG(TAG, "dfrobot_sen0465");

// Protocol from https://github.com/DFRobot/DFRobot_MultiGasSensor
// Request:  FF 01 command argument 00 00 00 00 checksum
// Response: FF command conc_high conc_low gas_type decimals temp_high temp_low checksum
static constexpr uint8_t FRAME_ADDRESS = 0x01;

static constexpr uint8_t NO_COMMAND = 0x00;
static constexpr uint8_t CMD_CHANGE_ACQUIRE_MODE = 0x78;
static constexpr uint8_t CMD_GET_ALL_DATA = 0x88;
static constexpr uint8_t CMD_SET_THRESHOLD_ALARM = 0x89;
static constexpr uint8_t ACQUIRE_MODE_PASSIVE = 0x04;
static constexpr uint8_t ALARM_SWITCH_OFF = 0x00;
static constexpr uint8_t ALARM_SWITCH_ON = 0x01;
static constexpr uint8_t ALARM_LOW_THRESHOLD = 0x00;
static constexpr uint8_t ALARM_HIGH_THRESHOLD = 0x01;
// Byte 2 of the answer to a setting command is 1 when the sensor accepted it
static constexpr uint8_t RESULT_OK = 0x01;

// The DFRobot examples wait this long after changing the acquire mode
static constexpr uint32_t MODE_CHANGE_DELAY_MS = 1000;
static constexpr uint32_t RETRY_DELAY_MS = 1000;
static constexpr uint8_t MAX_ATTEMPTS = 3;

// Like FucCheckSum() in the DFRobot library, the checksum covers bytes 1 to 6 only, not byte 7
static uint8_t frame_checksum(const uint8_t *frame) {
  uint8_t sum = 0;
  for (uint8_t i = 1; i < FRAME_SIZE - 2; i++)
    sum += frame[i];
  return static_cast<uint8_t>(~sum + 1);
}

static const LogString *step_name(SetupStep step) {
  return step == SetupStep::SETUP_STEP_ACQUIRE_MODE ? LOG_STR("Passive mode") : LOG_STR("Alarm setting");
}

void DFRobotSEN0465Component::setup() {
  // Answer only when asked, instead of sending a reading every second
  this->step_ = SetupStep::SETUP_STEP_ACQUIRE_MODE;
  if (!this->send_step_request_())
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
}

void DFRobotSEN0465Component::update() {
  // The first reading is requested when the setup steps are done
  if (this->step_ != SetupStep::SETUP_STEP_DONE)
    return;
  // A new request would replace the wait for the answer to the previous one
  if (this->pending_command_ != NO_COMMAND) {
    ESP_LOGV(TAG, "Previous request still waiting for an answer, skipping update");
    return;
  }
  if (!this->send_command_(CMD_GET_ALL_DATA)) {
    ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning();
  }
}

void DFRobotSEN0465Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "DFRobot SEN0465 series gas sensor:\n"
                "  Gas: %s (type 0x%02X)",
                this->gas_name_, this->gas_type_);
#ifdef USE_DFROBOT_SEN0465_TEMPERATURE_COMPENSATION
  ESP_LOGCONFIG(TAG, "  Temperature compensation: %s", YESNO(!this->compensation_.empty()));
#endif
  if (this->alarm_type_ == AlarmType::ALARM_TYPE_OFF) {
    ESP_LOGCONFIG(TAG, "  Alarm: off");
  } else {
    float scale = 1.0f;
    for (uint8_t i = 0; i < this->alarm_decimals_; i++)
      scale *= 10.0f;
    const float threshold = this->alarm_threshold_ / scale;
    ESP_LOGCONFIG(TAG, "  Alarm: %s %.*f",
                  this->alarm_type_ == AlarmType::ALARM_TYPE_HIGH ? LOG_STR_LITERAL("above") : LOG_STR_LITERAL("below"),
                  this->alarm_decimals_, threshold);
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Concentration", this->concentration_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

bool DFRobotSEN0465Component::send_command_(uint8_t command, uint8_t arg0, uint8_t arg1, uint8_t arg2, uint8_t arg3) {
  uint8_t frame[FRAME_SIZE] = {FRAME_HEAD, FRAME_ADDRESS, command, arg0, arg1, arg2, arg3};
  frame[FRAME_SIZE - 1] = frame_checksum(frame);
  if (!this->write_frame(frame))
    return false;
  this->pending_command_ = command;
  this->read_response();
  return true;
}

bool DFRobotSEN0465Component::send_step_request_() {
  if (this->step_ == SetupStep::SETUP_STEP_ACQUIRE_MODE)
    return this->send_command_(CMD_CHANGE_ACQUIRE_MODE, ACQUIRE_MODE_PASSIVE);
  if (this->alarm_type_ == AlarmType::ALARM_TYPE_OFF) {
    // Switch the alarm off explicitly, the sensor may still have one from an earlier configuration
    return this->send_command_(CMD_SET_THRESHOLD_ALARM, ALARM_SWITCH_OFF);
  }
  return this->send_command_(
      CMD_SET_THRESHOLD_ALARM, ALARM_SWITCH_ON, static_cast<uint8_t>(this->alarm_threshold_ >> 8),
      static_cast<uint8_t>(this->alarm_threshold_),
      this->alarm_type_ == AlarmType::ALARM_TYPE_HIGH ? ALARM_HIGH_THRESHOLD : ALARM_LOW_THRESHOLD);
}

void DFRobotSEN0465Component::retry_step_request_() {
  if (!this->send_step_request_())
    this->step_failed_(LOG_STR("request not sent"));
}

void DFRobotSEN0465Component::step_failed_(const LogString *reason) {
  this->attempts_++;
  ESP_LOGW(TAG, "%s failed: %s (attempt %u of %u)", LOG_STR_ARG(step_name(this->step_)), LOG_STR_ARG(reason),
           this->attempts_, MAX_ATTEMPTS);
  if (this->attempts_ < MAX_ATTEMPTS) {
    this->set_timeout(STEP_TIMEOUT_ID, RETRY_DELAY_MS, [this]() { this->retry_step_request_(); });
    return;
  }
  // Carry on: readings may still work, and the warning stays until one does
  this->status_set_warning();
  this->next_step_();
}

void DFRobotSEN0465Component::next_step_() {
  this->attempts_ = 0;
  if (this->step_ == SetupStep::SETUP_STEP_ACQUIRE_MODE) {
    this->step_ = SetupStep::SETUP_STEP_ALARM;
    this->set_timeout(STEP_TIMEOUT_ID, MODE_CHANGE_DELAY_MS, [this]() { this->retry_step_request_(); });
    return;
  }
  this->step_ = SetupStep::SETUP_STEP_DONE;
  this->update();
}

void DFRobotSEN0465Component::handle_no_response_() {
  this->pending_command_ = NO_COMMAND;
  if (this->step_ != SetupStep::SETUP_STEP_DONE) {
    this->step_failed_(LOG_STR("no response"));
    return;
  }
  ESP_LOGW(TAG, "No response from the sensor");
  this->status_set_warning();
}

void DFRobotSEN0465Component::handle_response_(const uint8_t *frame) {
  // Reject a late answer to an earlier request or a reading sent before passive mode was set
  const bool expected = frame[0] == FRAME_HEAD && frame[1] == this->pending_command_;
  this->pending_command_ = NO_COMMAND;
  char hex[format_hex_pretty_size(FRAME_SIZE)];

  if (this->step_ == SetupStep::SETUP_STEP_DONE) {
    if (!expected) {
      ESP_LOGW(TAG, "Unexpected response: %s", format_hex_pretty_to(hex, frame, FRAME_SIZE));
      this->status_set_warning();
      return;
    }
    this->handle_data_(frame);
    return;
  }

  ESP_LOGD(TAG, "%s response: %s", LOG_STR_ARG(step_name(this->step_)), format_hex_pretty_to(hex, frame, FRAME_SIZE));
  if (!expected) {
    this->step_failed_(LOG_STR("unexpected response"));
    return;
  }
  // Like the DFRobot library, the answer to the mode change is accepted without a checksum check
  if (this->step_ == SetupStep::SETUP_STEP_ALARM && frame[FRAME_SIZE - 1] != frame_checksum(frame)) {
    this->step_failed_(LOG_STR("invalid checksum"));
    return;
  }
  if (frame[2] != RESULT_OK) {
    this->step_failed_(LOG_STR("not confirmed"));
    return;
  }
  ESP_LOGD(TAG, "%s confirmed", LOG_STR_ARG(step_name(this->step_)));
  this->next_step_();
}

void DFRobotSEN0465Component::handle_data_(const uint8_t *frame) {
  if (frame[FRAME_SIZE - 1] != frame_checksum(frame)) {
    char hex[format_hex_pretty_size(FRAME_SIZE)];
    ESP_LOGW(TAG, "Invalid checksum: %s", format_hex_pretty_to(hex, frame, FRAME_SIZE));
    this->status_set_warning();
    return;
  }

  // On-board 10 kOhm NTC (B = 3380) read by a 10-bit ADC at 3 V, as converted by the DFRobot library
  float temperature = NAN;
  const uint16_t temperature_raw = encode_uint16(frame[6], frame[7]);
  if (temperature_raw > 0 && temperature_raw < 1024) {
    const float voltage = 3.0f * temperature_raw / 1024.0f;
    const float resistance = voltage * 10000.0f / (3.0f - voltage);
    temperature = 1.0f / (1.0f / 298.15f + std::log(resistance / 10000.0f) / 3380.13f) - 273.15f;
  }
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);

  if (frame[4] != this->gas_type_) {
    ESP_LOGW(TAG, "Probe reports gas type 0x%02X, expected %s (0x%02X); check the configured model", frame[4],
             this->gas_name_, this->gas_type_);
    this->status_set_warning();
    return;
  }

  float concentration = encode_uint16(frame[2], frame[3]);
  switch (frame[5]) {
    case 0:
      break;
    case 1:
      concentration *= 0.1f;
      break;
    case 2:
      concentration *= 0.01f;
      break;
    default:
      ESP_LOGW(TAG, "Invalid number of decimal places: %u", frame[5]);
      this->status_set_warning();
      return;
  }

#ifdef USE_DFROBOT_SEN0465_TEMPERATURE_COMPENSATION
  if (!this->compensation_.empty())
    concentration = this->compensate_(concentration, temperature);
#endif
  if (concentration < 0.0f)
    concentration = 0.0f;

  if (this->concentration_sensor_ != nullptr)
    this->concentration_sensor_->publish_state(concentration);
  if (std::isnan(concentration)) {
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }
}

#ifdef USE_DFROBOT_SEN0465_TEMPERATURE_COMPENSATION
float DFRobotSEN0465Component::compensate_(float concentration, float temperature) const {
  for (const auto &range : this->compensation_) {
    if (temperature > range.low && temperature <= range.high)
      return concentration / (range.a * temperature + range.b) - (range.c * temperature + range.d);
  }
  ESP_LOGW(TAG, "Temperature %.1f C is outside the compensation range", temperature);
  return NAN;
}
#endif

}  // namespace esphome::dfrobot_sen0465_base
