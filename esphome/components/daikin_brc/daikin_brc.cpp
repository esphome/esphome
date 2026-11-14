#include "daikin_brc.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/helpers.h"

namespace esphome::daikin_brc {

ESPHOME_LOG_TAG(TAG, "daikin_brc.climate");

static constexpr uint8_t DAIKIN_BRC_HEADER_BYTE0 = 0x11;
static constexpr uint8_t DAIKIN_BRC_HEADER_BYTE1 = 0xDA;
static constexpr uint8_t DAIKIN_BRC_HEADER_BYTE2 = 0x17;
static constexpr uint8_t DAIKIN_BRC_HEADER_BYTE3 = 0x18;
static constexpr uint8_t DAIKIN_BRC_FRAME_TYPE_STATE = 0x00;

using climate_ir::find_ir_code;
using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// Codes include the ON bit so the OFF entry never matches a running unit; an unlisted received code is ignored
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_OFF, DAIKIN_BRC_MODE_OFF},
    {climate::CLIMATE_MODE_COOL, DAIKIN_BRC_MODE_ON | DAIKIN_BRC_MODE_COOL},
    {climate::CLIMATE_MODE_DRY, DAIKIN_BRC_MODE_ON | DAIKIN_BRC_MODE_DRY},
    {climate::CLIMATE_MODE_HEAT, DAIKIN_BRC_MODE_ON | DAIKIN_BRC_MODE_HEAT},
    {climate::CLIMATE_MODE_HEAT_COOL, DAIKIN_BRC_MODE_ON | DAIKIN_BRC_MODE_AUTO},
    {climate::CLIMATE_MODE_FAN_ONLY, DAIKIN_BRC_MODE_ON | DAIKIN_BRC_MODE_FAN},
};
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_LOW, DAIKIN_BRC_FAN_1},
    {climate::CLIMATE_FAN_MEDIUM, DAIKIN_BRC_FAN_2},
    {climate::CLIMATE_FAN_HIGH, DAIKIN_BRC_FAN_3},
};

void DaikinBrcClimate::control(const climate::ClimateCall &call) {
  this->mode_button_ = 0x00;
  if (call.get_mode().has_value()) {
    // Need to determine if this is call due to Mode button pressed so that we can set the Mode button byte
    this->mode_button_ = DAIKIN_BRC_IR_MODE_BUTTON;
  }
  ClimateIR::control(call);
}

void DaikinBrcClimate::transmit_state() {
  uint8_t remote_state[DAIKIN_BRC_TRANSMIT_FRAME_SIZE] = {0x11, 0xDA, 0x17, 0x18, 0x04, 0x00, 0x1E, 0x11,
                                                          0xDA, 0x17, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                          0x00, 0x00, 0x00, 0x00, 0x20, 0x00};

  // The low 3 bits of the frame header byte address the indoor unit, in the preamble and the state frame
  remote_state[3] |= this->sub_unit_;
  remote_state[10] |= this->sub_unit_;
  remote_state[6] += this->sub_unit_;  // preamble checksum covers byte 3

  remote_state[12] = this->alt_mode_();
  remote_state[13] = this->mode_button_;
  remote_state[14] = to_ir_code(MODES, this->mode, DAIKIN_BRC_MODE_OFF);
  remote_state[17] = this->temperature_();
  remote_state[18] = this->fan_speed_swing_();

  // Calculate state frame checksum
  for (int i = DAIKIN_BRC_PREAMBLE_SIZE; i < DAIKIN_BRC_TRANSMIT_FRAME_SIZE - 1; i++) {
    remote_state[DAIKIN_BRC_TRANSMIT_FRAME_SIZE - 1] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_BRC_IR_FREQUENCY);

  data->mark(DAIKIN_BRC_HEADER_MARK);
  data->space(DAIKIN_BRC_HEADER_SPACE);
  data->item_bytes<DAIKIN_BRC_BIT_TIMINGS>(remote_state, DAIKIN_BRC_PREAMBLE_SIZE);
  data->mark(DAIKIN_BRC_BIT_MARK);
  data->space(DAIKIN_BRC_MESSAGE_SPACE);
  data->mark(DAIKIN_BRC_HEADER_MARK);
  data->space(DAIKIN_BRC_HEADER_SPACE);

  data->item_bytes<DAIKIN_BRC_BIT_TIMINGS>(remote_state + DAIKIN_BRC_PREAMBLE_SIZE,
                                           DAIKIN_BRC_TRANSMIT_FRAME_SIZE - DAIKIN_BRC_PREAMBLE_SIZE);

  data->mark(DAIKIN_BRC_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t DaikinBrcClimate::alt_mode_() {
  uint8_t alt_mode = 0x00;
  switch (this->mode) {
    case climate::CLIMATE_MODE_DRY:
      alt_mode = 0x23;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      alt_mode = 0x63;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_HEAT:
    default:
      alt_mode = 0x73;
      break;
  }
  return alt_mode;
}

uint8_t DaikinBrcClimate::fan_speed_swing_() {
  uint16_t fan_speed = to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_LOW), DAIKIN_BRC_FAN_1);

  // If swing is enabled switch first 4 bits to 1111
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      fan_speed |= DAIKIN_BRC_IR_SWING_ON;
      break;
    default:
      fan_speed |= DAIKIN_BRC_IR_SWING_OFF;
      break;
  }
  return fan_speed;
}

uint8_t DaikinBrcClimate::temperature_() {
  // Force special temperatures depending on the mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
    case climate::CLIMATE_MODE_DRY:
      if (this->fahrenheit_) {
        return DAIKIN_BRC_IR_DRY_FAN_TEMP_F;
      }
      return DAIKIN_BRC_IR_DRY_FAN_TEMP_C;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      uint8_t temperature;
      // Temperature in remote is in F
      if (this->fahrenheit_) {
        temperature = (uint8_t) roundf(clamp<float>(celsius_to_fahrenheit(this->target_temperature),
                                                    DAIKIN_BRC_TEMP_MIN_F, DAIKIN_BRC_TEMP_MAX_F));
      } else {
        temperature = ((uint8_t) roundf(this->target_temperature) - 9) << 1;
      }
      return temperature;
  }
}

bool DaikinBrcClimate::parse_state_frame_(const uint8_t frame[]) {
  const uint8_t checksum = sum_bytes(frame, DAIKIN_BRC_STATE_FRAME_SIZE - 1);
  if (frame[DAIKIN_BRC_STATE_FRAME_SIZE - 1] != checksum) {
    ESP_LOGCONFIG(TAG, "Bad CheckSum %x", checksum);
    return false;
  }

  uint8_t mode = frame[7];
  if (!(mode & DAIKIN_BRC_MODE_ON)) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    this->mode = from_ir_code(MODES, mode & (0xF0 | DAIKIN_BRC_MODE_ON), this->mode);
  }

  uint8_t temperature = frame[10];
  float temperature_c;
  if (this->fahrenheit_) {
    temperature_c = clamp<float>(((temperature - 32) / 1.8), DAIKIN_BRC_TEMP_MIN_C, DAIKIN_BRC_TEMP_MAX_C);
  } else {
    temperature_c = (temperature >> 1) + 9;
  }

  this->target_temperature = temperature_c;

  uint8_t fan_mode = frame[11];
  uint8_t swing_mode = frame[11];
  switch (swing_mode & 0xF) {
    case DAIKIN_BRC_IR_SWING_ON:
      this->swing_mode = climate::CLIMATE_SWING_BOTH;
      break;
    case DAIKIN_BRC_IR_SWING_OFF:
      this->swing_mode = climate::CLIMATE_SWING_OFF;
      break;
  }

  if (const auto entry = find_ir_code(FANS, fan_mode & 0xF0, true))
    this->fan_mode = static_cast<climate::ClimateFanMode>(entry->value);
  this->publish_state();
  return true;
}

bool DaikinBrcClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[DAIKIN_BRC_STATE_FRAME_SIZE];
  if (!data.expect_item(DAIKIN_BRC_HEADER_MARK, DAIKIN_BRC_HEADER_SPACE)) {
    return false;
  }
  // Frame header, with this unit's address in the low 3 bits of byte 3, then the frame type
  if (!data.expect_bytes<DAIKIN_BRC_BIT_TIMINGS>(state_frame, DAIKIN_BRC_STATE_FRAME_SIZE) ||
      state_frame[0] != DAIKIN_BRC_HEADER_BYTE0 || state_frame[1] != DAIKIN_BRC_HEADER_BYTE1 ||
      state_frame[2] != DAIKIN_BRC_HEADER_BYTE2 || state_frame[3] != (DAIKIN_BRC_HEADER_BYTE3 | this->sub_unit_) ||
      state_frame[4] != DAIKIN_BRC_FRAME_TYPE_STATE) {
    return false;
  }
  return this->parse_state_frame_(state_frame);
}

}  // namespace esphome::daikin_brc
