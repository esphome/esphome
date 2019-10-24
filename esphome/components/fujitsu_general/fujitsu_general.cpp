#include "fujitsu_general.h"

namespace esphome {
namespace fujitsu_general {

static const char *TAG = "fujitsu_general.climate";

// Control packet
const uint16_t FUJITSU_GENERAL_STATE_LENGTH = 16;

const uint8_t FUJITSU_GENERAL_BASE_BYTE0 = 0x14;
const uint8_t FUJITSU_GENERAL_BASE_BYTE1 = 0x63;
const uint8_t FUJITSU_GENERAL_BASE_BYTE2 = 0x00;
const uint8_t FUJITSU_GENERAL_BASE_BYTE3 = 0x10;
const uint8_t FUJITSU_GENERAL_BASE_BYTE4 = 0x10;
const uint8_t FUJITSU_GENERAL_BASE_BYTE5 = 0xFE;
const uint8_t FUJITSU_GENERAL_BASE_BYTE6 = 0x09;
const uint8_t FUJITSU_GENERAL_BASE_BYTE7 = 0x30;

// Temperature and POWER ON
const uint8_t FUJITSU_GENERAL_POWER_ON_MASK_BYTE8 = 0b00000001;
const uint8_t FUJITSU_GENERAL_BASE_BYTE8 = 0x40;

// Mode
const uint8_t FUJITSU_GENERAL_MODE_AUTO_BYTE9 = 0x00;
const uint8_t FUJITSU_GENERAL_MODE_HEAT_BYTE9 = 0x04;
const uint8_t FUJITSU_GENERAL_MODE_COOL_BYTE9 = 0x01;
const uint8_t FUJITSU_GENERAL_MODE_DRY_BYTE9 = 0x02;
const uint8_t FUJITSU_GENERAL_MODE_FAN_BYTE9 = 0x03;
const uint8_t FUJITSU_GENERAL_MODE_10C_BYTE9 = 0x0B;
const uint8_t FUJITSU_GENERAL_BASE_BYTE9 = 0x01;

// Fan speed and swing
const uint8_t FUJITSU_GENERAL_FAN_AUTO_BYTE10 = 0x00;
const uint8_t FUJITSU_GENERAL_FAN_HIGH_BYTE10 = 0x01;
const uint8_t FUJITSU_GENERAL_FAN_MEDIUM_BYTE10 = 0x02;
const uint8_t FUJITSU_GENERAL_FAN_LOW_BYTE10 = 0x03;
const uint8_t FUJITSU_GENERAL_FAN_SILENT_BYTE10 = 0x04;
const uint8_t FUJITSU_GENERAL_SWING_MASK_BYTE10 = 0b00010000;
const uint8_t FUJITSU_GENERAL_BASE_BYTE10 = 0x00;

const uint8_t FUJITSU_GENERAL_BASE_BYTE11 = 0x00;
const uint8_t FUJITSU_GENERAL_BASE_BYTE12 = 0x00;
const uint8_t FUJITSU_GENERAL_BASE_BYTE13 = 0x00;

// Outdoor Unit Low Noise
const uint8_t FUJITSU_GENERAL_OUTDOOR_UNIT_LOW_NOISE_BYTE14 = 0xA0;
const uint8_t FUJITSU_GENERAL_BASE_BYTE14 = 0x20;

// CRC
const uint8_t FUJITSU_GENERAL_BASE_BYTE15 = 0x6F;

// Power off packet is specific
const uint16_t FUJITSU_GENERAL_OFF_LENGTH = 7;

const uint8_t FUJITSU_GENERAL_OFF_BYTE0 = FUJITSU_GENERAL_BASE_BYTE0;
const uint8_t FUJITSU_GENERAL_OFF_BYTE1 = FUJITSU_GENERAL_BASE_BYTE1;
const uint8_t FUJITSU_GENERAL_OFF_BYTE2 = FUJITSU_GENERAL_BASE_BYTE2;
const uint8_t FUJITSU_GENERAL_OFF_BYTE3 = FUJITSU_GENERAL_BASE_BYTE3;
const uint8_t FUJITSU_GENERAL_OFF_BYTE4 = FUJITSU_GENERAL_BASE_BYTE4;
const uint8_t FUJITSU_GENERAL_OFF_BYTE5 = 0x02;
const uint8_t FUJITSU_GENERAL_OFF_BYTE6 = 0xFD;

const uint8_t FUJITSU_GENERAL_TEMP_MAX = 30;  // Celsius
const uint8_t FUJITSU_GENERAL_TEMP_MIN = 16;  // Celsius

const uint16_t FUJITSU_GENERAL_HEADER_MARK = 3300;
const uint16_t FUJITSU_GENERAL_HEADER_SPACE = 1600;
const uint16_t FUJITSU_GENERAL_BIT_MARK = 420;
const uint16_t FUJITSU_GENERAL_ONE_SPACE = 1200;
const uint16_t FUJITSU_GENERAL_ZERO_SPACE = 420;
const uint16_t FUJITSU_GENERAL_TRL_MARK = 420;
const uint16_t FUJITSU_GENERAL_TRL_SPACE = 8000;

const uint32_t FUJITSU_GENERAL_CARRIER_FREQUENCY = 38000;

climate::ClimateTraits FujitsuGeneralClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  traits.set_visual_min_temperature(FUJITSU_GENERAL_TEMP_MIN);
  traits.set_visual_max_temperature(FUJITSU_GENERAL_TEMP_MAX);
  traits.set_visual_temperature_step(1);
  return traits;
}

void FujitsuGeneralClimate::setup() {
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else
    this->current_temperature = NAN;
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 24;
  }
  // initial state is powered off
  this->power_ = false;
}

void FujitsuGeneralClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_off_();
  } else {
    this->transmit_state_();
  }
  this->publish_state();
}

void FujitsuGeneralClimate::transmit_state_() {
  uint8_t remote_state[FUJITSU_GENERAL_STATE_LENGTH] = {0};

  remote_state[0] = FUJITSU_GENERAL_BASE_BYTE0;
  remote_state[1] = FUJITSU_GENERAL_BASE_BYTE1;
  remote_state[2] = FUJITSU_GENERAL_BASE_BYTE2;
  remote_state[3] = FUJITSU_GENERAL_BASE_BYTE3;
  remote_state[4] = FUJITSU_GENERAL_BASE_BYTE4;
  remote_state[5] = FUJITSU_GENERAL_BASE_BYTE5;
  remote_state[6] = FUJITSU_GENERAL_BASE_BYTE6;
  remote_state[7] = FUJITSU_GENERAL_BASE_BYTE7;
  remote_state[8] = FUJITSU_GENERAL_BASE_BYTE8;
  remote_state[9] = FUJITSU_GENERAL_BASE_BYTE9;
  remote_state[10] = FUJITSU_GENERAL_BASE_BYTE10;
  remote_state[11] = FUJITSU_GENERAL_BASE_BYTE11;
  remote_state[12] = FUJITSU_GENERAL_BASE_BYTE12;
  remote_state[13] = FUJITSU_GENERAL_BASE_BYTE13;
  remote_state[14] = FUJITSU_GENERAL_BASE_BYTE14;
  remote_state[15] = FUJITSU_GENERAL_BASE_BYTE15;

  // Set temperature
  uint8_t safecelsius = std::max((uint8_t) this->target_temperature, FUJITSU_GENERAL_TEMP_MIN);
  safecelsius = std::min(safecelsius, FUJITSU_GENERAL_TEMP_MAX);
  remote_state[8] = (byte) safecelsius - 16;
  remote_state[8] = remote_state[8] << 4;

  // If not powered - set power on flag
  if (!this->power_) {
    this->power_ = true;
    remote_state[8] = (byte) remote_state[8] | FUJITSU_GENERAL_POWER_ON_MASK_BYTE8;
  }

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      remote_state[9] = FUJITSU_GENERAL_MODE_COOL_BYTE9;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[9] = FUJITSU_GENERAL_MODE_HEAT_BYTE9;
      break;
    case climate::CLIMATE_MODE_AUTO:
    default:
      remote_state[9] = FUJITSU_GENERAL_MODE_AUTO_BYTE9;
      break;
      // TODO: CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_DRY, CLIMATE_MODE_10C are missing in esphome
  }

  // TODO: missing support for fan speed
  remote_state[10] = FUJITSU_GENERAL_FAN_AUTO_BYTE10;

  // TODO: missing support for swing
  // remote_state[10] = (byte) remote_state[10] | FUJITSU_GENERAL_SWING_MASK_BYTE10;

  // TODO: missing support for outdoor unit low noise
  // remote_state[14] = (byte) remote_state[14] | FUJITSU_GENERAL_OUTDOOR_UNIT_LOW_NOISE_BYTE14;

  // CRC
  remote_state[15] = 0;
  for (int i = 7; i < 15; i++) {
    remote_state[15] += (byte) remote_state[i];  // Addiction
  }
  remote_state[15] = 0x100 - remote_state[15];  // mod 256

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(FUJITSU_GENERAL_CARRIER_FREQUENCY);

  // Header
  data->mark(FUJITSU_GENERAL_HEADER_MARK);
  data->space(FUJITSU_GENERAL_HEADER_SPACE);
  // Data
  for (uint8_t i : remote_state) {
    // Send all Bits from Byte Data in Reverse Order
    for (uint8_t mask = 00000001; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(FUJITSU_GENERAL_BIT_MARK);
      bool bit = i & mask;
      data->space(bit ? FUJITSU_GENERAL_ONE_SPACE : FUJITSU_GENERAL_ZERO_SPACE);
      // Next bits
    }
  }
  // Footer
  data->mark(FUJITSU_GENERAL_TRL_MARK);
  data->space(FUJITSU_GENERAL_TRL_SPACE);

  transmit.perform();

  // If was not powered - set powered
  if (!this->power_) {
    this->power_ = true;
  }
}

void FujitsuGeneralClimate::transmit_off_() {
  uint8_t remote_state[FUJITSU_GENERAL_OFF_LENGTH] = {0};

  remote_state[0] = FUJITSU_GENERAL_OFF_BYTE0;
  remote_state[1] = FUJITSU_GENERAL_OFF_BYTE1;
  remote_state[2] = FUJITSU_GENERAL_OFF_BYTE2;
  remote_state[3] = FUJITSU_GENERAL_OFF_BYTE3;
  remote_state[4] = FUJITSU_GENERAL_OFF_BYTE4;
  remote_state[5] = FUJITSU_GENERAL_OFF_BYTE5;
  remote_state[6] = FUJITSU_GENERAL_OFF_BYTE6;

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(FUJITSU_GENERAL_CARRIER_FREQUENCY);

  // Header
  data->mark(FUJITSU_GENERAL_HEADER_MARK);
  data->space(FUJITSU_GENERAL_HEADER_SPACE);

  // Data
  for (uint8_t i : remote_state) {
    // Send all Bits from Byte Data in Reverse Order
    for (uint8_t mask = 00000001; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(FUJITSU_GENERAL_BIT_MARK);
      bool bit = i & mask;
      data->space(bit ? FUJITSU_GENERAL_ONE_SPACE : FUJITSU_GENERAL_ZERO_SPACE);
      // Next bits
    }
  }
  // Footer
  data->mark(FUJITSU_GENERAL_TRL_MARK);
  data->space(FUJITSU_GENERAL_TRL_SPACE);

  transmit.perform();

  this->power_ = false;
}

}  // namespace fujitsu_general
}  // namespace esphome
