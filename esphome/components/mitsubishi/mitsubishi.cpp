#include "mitsubishi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mitsubishi {

static const char *TAG = "mitsubishi.climate";

const uint32_t MITSUBISHI_OFF = 0x00;

const uint8_t MITSUBISHI_COOL = 0x18;
const uint8_t MITSUBISHI_DRY = 0x10;
const uint8_t MITSUBISHI_AUTO = 0x20;
const uint8_t MITSUBISHI_HEAT = 0x08;
const uint8_t MITSUBISHI_FAN_AUTO = 0x00;

// Temperature
const uint8_t MITSUBISHI_TEMP_MIN = 17;  // Celsius
const uint8_t MITSUBISHI_TEMP_MAX = 31;  // Celsius

// Pulse parameters in usec
const uint16_t MITSUBISHI_BIT_MARK = 430;
const uint16_t MITSUBISHI_ONE_SPACE = 1250;
const uint16_t MITSUBISHI_ZERO_SPACE = 390;
const uint16_t MITSUBISHI_HEADER_MARK = 3500;
const uint16_t MITSUBISHI_HEADER_SPACE = 1700;
const uint16_t MITSUBISHI_MIN_GAP = 17500;

climate::ClimateTraits MitsubishiClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  traits.set_visual_min_temperature(MITSUBISHI_TEMP_MIN);
  traits.set_visual_max_temperature(MITSUBISHI_TEMP_MAX);
  traits.set_visual_temperature_step(1);
  return traits;
}

void MitsubishiClimate::setup() {
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
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature = roundf(this->current_temperature);
  }
}

void MitsubishiClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->transmit_state_();
  this->publish_state();
}

void MitsubishiClimate::transmit_state_() {
  uint32_t remote_state = { 
    0x23, 0xCB, 0x26, 0x01,
    0x00, 0x20, 0x48, 0x00, 
    0x00, MITSUBISHI_FAN_AUTO, 0x61, 0x00, 
    0x00, 0x00, 0x10, 0x40,
    0x00, 0x00 };

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      remote_state[6] = MITSUBISHI_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] = MITSUBISHI_HEAT;
      break;
    case climate::CLIMATE_MODE_AUTO:
      remote_state[6] = MITSUBISHI_AUTO;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[5] = MITSUBISHI_OFF;
      break;
  }
  if (this->mode != climate::CLIMATE_MODE_OFF) {
    auto temp = (uint8_t) roundf(clamp(this->target_temperature, MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX));
    remote_state &= ~MITSUBISHI_TEMP_MASK;  // Clear the old temp.
    remote_state |= (MITSUBISHI_TEMP_MAP[temp - MITSUBISHI_TEMP_MIN] << 4);
  }

  ESP_LOGV(TAG, "Sending mitsubishi code: %u", remote_state);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);
  uint16_t repeat = 1;
  for (uint16_t r = 0; r <= repeat; r++) {
    // Header
    data->mark(MITSUBISHI_HEADER_MARK);
    data->space(MITSUBISHI_HEADER_SPACE);
    // Data
    //   Break data into byte segments, starting at the Most Significant
    //   Byte. Each byte then being sent normal, then followed inverted.
    for (uint16_t i = 8; i <= MITSUBISHI_BITS; i += 8) {
      // Grab a bytes worth of data.
      uint8_t segment = (remote_state >> (MITSUBISHI_BITS - i)) & 0xFF;
      // Normal
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(MITSUBISHI_BIT_MARK);
        data->space((segment & mask) ? MITSUBISHI_ONE_SPACE : MITSUBISHI_ZERO_SPACE);
      }
      // Inverted
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(MITSUBISHI_BIT_MARK);
        data->space(!(segment & mask) ? MITSUBISHI_ONE_SPACE : MITSUBISHI_ZERO_SPACE);
      }
    }
    // Footer
    data->mark(MITSUBISHI_BIT_MARK);
    data->space(MITSUBISHI_MIN_GAP);  // Pause before repeating
  }

  transmit.perform();
}

}  // namespace mitsubishi
}  // namespace esphome
