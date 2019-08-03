#include "whirlpool.h"
#include "esphome/core/log.h"

namespace esphome {
namespace whirlpool {

static const char *TAG = "whirlpool.climate";


const uint16_t WHIRLPOOL_HEADER_MARK = 8950;
const uint16_t WHIRLPOOL_HEADER_SPACE = 4484;
const uint16_t WHIRLPOOL_BIT_MARK = 597;
const uint16_t WHIRLPOOL_ONE_SPACE = 1649;
const uint16_t WHIRLPOOL_ZERO_SPACE = 533;
const uint32_t WHIRLPOOL_GAP = WHIRLPOOL_HEADER_SPACE;

const uint32_t WHIRLPOOL_CARRIER_FREQUENCY = 38000;

const uint8_t WHIRLPOOL_TEMP_MAX = 30;  // Celsius
const uint8_t WHIRLPOOL_TEMP_MIN = 16;  // Celsius

const uint8_t WHIRLPOOL_STATE_LENGTH = 21;

climate::ClimateTraits WhirlpoolClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(1);
  return traits;
}

void WhirlpoolClimate::setup() {
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
    this->mode = climate::CLIMATE_MODE_AUTO;
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature = roundf(this->current_temperature);
  }
}

void WhirlpoolClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->transmit_state_();
  this->publish_state();
}

void WhirlpoolClimate::transmit_state_() {
  uint8_t remote_state[WHIRLPOOL_STATE_LENGTH] = {0};
  remote_state[0] = 0x83;
  remote_state[1] = 0x06;
  remote_state[6] = 0x80;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      break;
    case climate::CLIMATE_MODE_HEAT:
      break;
    case climate::CLIMATE_MODE_AUTO:
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      break;
  }

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    auto temp = (uint8_t) roundf(clamp(this->target_temperature, COOLIX_TEMP_MIN, COOLIX_TEMP_MAX));
    remote_state &= ~COOLIX_TEMP_MASK;  // Clear the old temp.
    remote_state |= (COOLIX_TEMP_MAP[temp - COOLIX_TEMP_MIN] << 4);
  }

  remote_state[13] = 13;
  for (uint8_t i = 2; i < 13; i++) remote_state[13] ^= remote_state[i];
  for (uint8_t i = 14; i < 20; i++) remote_state[20] ^= remote_state[i];

  ESP_LOGV(TAG, "Sending whirlpool code: %u", remote_state[0]);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);
  uint16_t repeat = 1;
  for (uint16_t r = 0; r <= repeat; r++) {
    // Header
    data->mark(COOLIX_HEADER_MARK);
    data->space(COOLIX_HEADER_SPACE);
    // Data
    //   Break data into byte segments, starting at the Most Significant
    //   Byte. Each byte then being sent normal, then followed inverted.
    for (uint16_t i = 8; i <= COOLIX_BITS; i += 8) {
      // Grab a bytes worth of data.
      uint8_t segment = (remote_state >> (COOLIX_BITS - i)) & 0xFF;
      // Normal
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(COOLIX_BIT_MARK);
        data->space((segment & mask) ? COOLIX_ONE_SPACE : COOLIX_ZERO_SPACE);
      }
      // Inverted
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(COOLIX_BIT_MARK);
        data->space(!(segment & mask) ? COOLIX_ONE_SPACE : COOLIX_ZERO_SPACE);
      }
    }
    // Footer
    data->mark(COOLIX_BIT_MARK);
    data->space(COOLIX_MIN_GAP);  // Pause before repeating
  }

  transmit.perform();
}

}  // namespace whirlpool
}  // namespace esphome
