#include "yashima.h"
#include "esphome/core/log.h"

namespace esphome {
namespace yashima {

static const char *const TAG = "yashima.climate";

const uint16_t YASHIMA_STATE_LENGTH = 9;
const uint16_t YASHIMA_BITS = YASHIMA_STATE_LENGTH * 8;

/* the bit masks are intended to be sent from the MSB to the LSB */
const uint8_t YASHIMA_MODE_HEAT_BYTE0 = 0b00100000;
const uint8_t YASHIMA_MODE_DRY_BYTE0 = 0b01100000;
const uint8_t YASHIMA_MODE_COOL_BYTE0 = 0b11100000;
const uint8_t YASHIMA_MODE_FAN_BYTE0 = 0b10100000;
const uint8_t YASHIMA_MODE_AUTO_BYTE0 = 0b11100000;
const uint8_t YASHIMA_MODE_OFF_BYTE0 = 0b11110000;
const uint8_t YASHIMA_BASE_BYTE0 = 0b1110;

const uint8_t YASHIMA_TEMP_MAX = 30;  // Celsius
const uint8_t YASHIMA_TEMP_MIN = 16;  // Celsius
const uint8_t YASHIMA_TEMP_RANGE = YASHIMA_TEMP_MAX - YASHIMA_TEMP_MIN + 1;

const uint8_t YASHIMA_TEMP_MAP_BYTE1[YASHIMA_TEMP_RANGE] = {
    0b01100100,  // 16C
    0b10100100,  // 17C
    0b00100100,  // 18C
    0b11000100,  // 19C
    0b01000100,  // 20C
    0b10000100,  // 21C
    0b00000100,  // 22C
    0b11111000,  // 23C
    0b01111000,  // 24C
    0b10111000,  // 25C
    0b00111000,  // 26C
    0b11011000,  // 27C
    0b01011000,  // 28C
    0b10011000,  // 29C
    0b00011000,  // 30C
};
const uint8_t YASHIMA_BASE_BYTE1 = 0b11;

const uint8_t YASHIMA_FAN_AUTO_BYTE2 = 0b11000000;
const uint8_t YASHIMA_FAN_LOW_BYTE2 = 0b00000000;
const uint8_t YASHIMA_FAN_MEDIUM_BYTE2 = 0b10000000;
const uint8_t YASHIMA_FAN_HIGH_BYTE2 = 0b01000000;
const uint8_t YASHIMA_BASE_BYTE2 = 0b111111;

const uint8_t YASHIMA_BASE_BYTE3 = 0b11111111;
const uint8_t YASHIMA_BASE_BYTE4 = 0b11;

const uint8_t YASHIMA_MODE_HEAT_BYTE5 = 0b00000000;
const uint8_t YASHIMA_MODE_DRY_BYTE5 = 0b00000000;
const uint8_t YASHIMA_MODE_FAN_BYTE5 = 0b00000000;
const uint8_t YASHIMA_MODE_AUTO_BYTE5 = 0b00000000;
const uint8_t YASHIMA_MODE_COOL_BYTE5 = 0b10000000;
const uint8_t YASHIMA_MODE_OFF_BYTE5 = 0b10000000;
const uint8_t YASHIMA_BASE_BYTE5 = 0b11111;

const uint8_t YASHIMA_BASE_BYTE6 = 0b11111111;
const uint8_t YASHIMA_BASE_BYTE7 = 0b11111111;
const uint8_t YASHIMA_BASE_BYTE8 = 0b11001111;

/* values sampled using a Broadlink Mini 3: */
// const uint16_t YASHIMA_HEADER_MARK = 9600;
// const uint16_t YASHIMA_HEADER_SPACE = 4800;
// const uint16_t YASHIMA_BIT_MARK = 720;
// const uint16_t YASHIMA_ONE_SPACE = 550;
// const uint16_t YASHIMA_ZERO_SPACE = 1640;

/* scaled values to get correct timing on ESP8266/ESP32: */
const uint16_t YASHIMA_HEADER_MARK = 9035;
const uint16_t YASHIMA_HEADER_SPACE = 4517;
const uint16_t YASHIMA_BIT_MARK = 667;
const uint16_t YASHIMA_ONE_SPACE = 517;
const uint16_t YASHIMA_ZERO_SPACE = 1543;
const uint32_t YASHIMA_GAP = YASHIMA_HEADER_SPACE;

const uint32_t YASHIMA_CARRIER_FREQUENCY = 38000;

climate::ClimateTraits YashimaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);

  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL});
  if (supports_cool_)
    traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  if (supports_heat_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);

  traits.set_supports_two_point_target_temperature(false);
  traits.set_visual_min_temperature(YASHIMA_TEMP_MIN);
  traits.set_visual_max_temperature(YASHIMA_TEMP_MAX);
  traits.set_visual_temperature_step(1);
  return traits;
}

void YashimaClimate::setup() {
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
}

void YashimaClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->transmit_state_();
  this->publish_state();
}

void YashimaClimate::transmit_state_() {
  uint8_t remote_state[YASHIMA_STATE_LENGTH] = {0};

  remote_state[0] = YASHIMA_BASE_BYTE0;
  remote_state[1] = YASHIMA_BASE_BYTE1;
  remote_state[2] = YASHIMA_BASE_BYTE2;
  remote_state[3] = YASHIMA_BASE_BYTE3;
  remote_state[4] = YASHIMA_BASE_BYTE4;
  remote_state[5] = YASHIMA_BASE_BYTE5;
  remote_state[6] = YASHIMA_BASE_BYTE6;
  remote_state[7] = YASHIMA_BASE_BYTE7;
  remote_state[8] = YASHIMA_BASE_BYTE8;

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[0] |= YASHIMA_MODE_AUTO_BYTE0;
      remote_state[5] |= YASHIMA_MODE_AUTO_BYTE5;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[0] |= YASHIMA_MODE_COOL_BYTE0;
      remote_state[5] |= YASHIMA_MODE_COOL_BYTE5;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[0] |= YASHIMA_MODE_HEAT_BYTE0;
      remote_state[5] |= YASHIMA_MODE_HEAT_BYTE5;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[0] |= YASHIMA_MODE_OFF_BYTE0;
      remote_state[5] |= YASHIMA_MODE_OFF_BYTE5;
      break;
      // TODO: CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_DRY are missing in esphome
  }

  // TODO: missing support for fan speed
  remote_state[2] |= YASHIMA_FAN_AUTO_BYTE2;

  // Set temperature
  uint8_t safecelsius = std::max((uint8_t) this->target_temperature, YASHIMA_TEMP_MIN);
  safecelsius = std::min(safecelsius, YASHIMA_TEMP_MAX);
  remote_state[1] |= YASHIMA_TEMP_MAP_BYTE1[safecelsius - YASHIMA_TEMP_MIN];

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(YASHIMA_CARRIER_FREQUENCY);

  // Header
  data->mark(YASHIMA_HEADER_MARK);
  data->space(YASHIMA_HEADER_SPACE);
  // Data (sent from the MSB to the LSB)
  for (uint8_t i : remote_state) {
    for (int8_t j = 7; j >= 0; j--) {
      data->mark(YASHIMA_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? YASHIMA_ONE_SPACE : YASHIMA_ZERO_SPACE);
    }
  }
  // Footer
  data->mark(YASHIMA_BIT_MARK);
  data->space(YASHIMA_GAP);

  transmit.perform();
}

}  // namespace yashima
}  // namespace esphome
