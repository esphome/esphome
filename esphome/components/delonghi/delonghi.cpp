#include "delonghi.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/helpers.h"

namespace esphome::delonghi {

ESPHOME_LOG_TAG(TAG, "delonghi.climate");

using climate_ir::find_ir_code;
using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// A received code that is not listed is ignored
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_OFF, DELONGHI_MODE_OFF},
    {climate::CLIMATE_MODE_COOL, DELONGHI_MODE_ON | DELONGHI_MODE_COOL},
    {climate::CLIMATE_MODE_DRY, DELONGHI_MODE_ON | DELONGHI_MODE_DRY},
    {climate::CLIMATE_MODE_HEAT, DELONGHI_MODE_ON | DELONGHI_MODE_HEAT},
    {climate::CLIMATE_MODE_HEAT_COOL, DELONGHI_MODE_ON | DELONGHI_MODE_AUTO},
    {climate::CLIMATE_MODE_FAN_ONLY, DELONGHI_MODE_ON | DELONGHI_MODE_FAN},
};
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, DELONGHI_FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, DELONGHI_FAN_LOW},
    {climate::CLIMATE_FAN_MEDIUM, DELONGHI_FAN_MEDIUM},
    {climate::CLIMATE_FAN_HIGH, DELONGHI_FAN_HIGH},
};

void DelonghiClimate::transmit_state() {
  uint8_t remote_state[DELONGHI_STATE_FRAME_SIZE] = {0};
  remote_state[0] = DELONGHI_ADDRESS;
  remote_state[1] = this->temperature_();
  remote_state[1] |= to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), DELONGHI_FAN_AUTO) << 5;
  remote_state[2] = to_ir_code(MODES, this->mode, DELONGHI_MODE_OFF);
  // Calculate checksum
  for (int i = 0; i < DELONGHI_STATE_FRAME_SIZE - 1; i++) {
    remote_state[DELONGHI_STATE_FRAME_SIZE - 1] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DELONGHI_IR_FREQUENCY);

  data->mark(DELONGHI_HEADER_MARK);
  data->space(DELONGHI_HEADER_SPACE);
  data->item_bytes<DELONGHI_BIT_TIMINGS>(remote_state, DELONGHI_STATE_FRAME_SIZE);
  data->mark(DELONGHI_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t DelonghiClimate::temperature_() {
  // Force special temperatures depending on the mode
  uint8_t temperature = 0b0001;
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
      temperature = (uint8_t) roundf(this->target_temperature) - DELONGHI_TEMP_OFFSET_HEAT;
      break;
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_DRY:
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_FAN_ONLY:
    case climate::CLIMATE_MODE_OFF:
    default:
      temperature = (uint8_t) roundf(this->target_temperature) - DELONGHI_TEMP_OFFSET_COOL;
  }
  if (temperature > 0x0F) {
    temperature = 0x0F;  // clamp maximum
  }
  return temperature;
}

bool DelonghiClimate::parse_state_frame_(const uint8_t frame[]) {
  const uint8_t checksum = sum_bytes(frame, DELONGHI_STATE_FRAME_SIZE - 1);
  if (frame[DELONGHI_STATE_FRAME_SIZE - 1] != checksum) {
    return false;
  }
  uint8_t mode = frame[2] & 0x0F;
  if (!(mode & DELONGHI_MODE_ON)) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    this->mode = from_ir_code(MODES, mode, this->mode);
  }
  uint8_t temperature = frame[1] & 0x0F;
  if (this->mode == climate::CLIMATE_MODE_HEAT) {
    this->target_temperature = temperature + DELONGHI_TEMP_OFFSET_HEAT;
  } else {
    this->target_temperature = temperature + DELONGHI_TEMP_OFFSET_COOL;
  }
  if (const auto entry = find_ir_code(FANS, frame[1] >> 5, true))
    this->fan_mode = static_cast<climate::ClimateFanMode>(entry->value);
  this->publish_state();
  return true;
}

bool DelonghiClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[DELONGHI_STATE_FRAME_SIZE];
  if (!data.expect_item(DELONGHI_HEADER_MARK, DELONGHI_HEADER_SPACE)) {
    return false;
  }
  if (!data.expect_bytes<DELONGHI_BIT_TIMINGS>(state_frame, DELONGHI_STATE_FRAME_SIZE) ||
      state_frame[0] != DELONGHI_ADDRESS)
    return false;
  return this->parse_state_frame_(state_frame);
}

}  // namespace esphome::delonghi
