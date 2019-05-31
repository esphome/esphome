#include "coolix.h"
#include "esphome/core/log.h"

namespace esphome {
namespace coolix {

static const char *TAG = "coolix.climate";

const uint32_t COOLIX_OFF = 0xB27BE0;
// On, 25C, Mode: Auto, Fan: Auto, Zone Follow: Off, Sensor Temp: Ignore.
const uint32_t COOLIX_DEFAULT_STATE = 0xB2BFC8;
const uint32_t COOLIX_DEFAULT_STATE_AUTO_24_FAN = 0xB21F48;
const uint8_t COOLIX_COOL = 0b00;
const uint8_t COOLIX_DRY = 0b01;
const uint8_t COOLIX_AUTO = 0b10;
const uint8_t COOLIX_HEAT = 0b11;
const uint8_t COOLIX_FAN = 4;                                  // Synthetic.
const uint32_t COOLIX_MODE_MASK = 0b000000000000000000001100;  // 0xC

// Temperature
const uint8_t COOLIX_TEMP_MIN = 17;  // Celsius
const uint8_t COOLIX_TEMP_MAX = 30;  // Celsius
const uint8_t COOLIX_TEMP_RANGE = COOLIX_TEMP_MAX - COOLIX_TEMP_MIN + 1;
const uint8_t COOLIX_FAN_TEMP_CODE = 0b1110;  // Part of Fan Mode.
const uint32_t COOLIX_TEMP_MASK = 0b11110000;
const uint8_t COOLIX_TEMP_MAP[COOLIX_TEMP_RANGE] = {
    0b0000,  // 17C
    0b0001,  // 18c
    0b0011,  // 19C
    0b0010,  // 20C
    0b0110,  // 21C
    0b0111,  // 22C
    0b0101,  // 23C
    0b0100,  // 24C
    0b1100,  // 25C
    0b1101,  // 26C
    0b1001,  // 27C
    0b1000,  // 28C
    0b1010,  // 29C
    0b1011   // 30C
};

// Constants
// Pulse parms are *50-100 for the Mark and *50+100 for the space
// First MARK is the one after the long gap
// pulse parameters in usec
const uint16_t COOLIX_TICK = 560;  // Approximately 21 cycles at 38kHz
const uint16_t COOLIX_BIT_MARK_TICKS = 1;
const uint16_t COOLIX_BIT_MARK = COOLIX_BIT_MARK_TICKS * COOLIX_TICK;
const uint16_t COOLIX_ONE_SPACE_TICKS = 3;
const uint16_t COOLIX_ONE_SPACE = COOLIX_ONE_SPACE_TICKS * COOLIX_TICK;
const uint16_t COOLIX_ZERO_SPACE_TICKS = 1;
const uint16_t COOLIX_ZERO_SPACE = COOLIX_ZERO_SPACE_TICKS * COOLIX_TICK;
const uint16_t COOLIX_HEADER_MARK_TICKS = 8;
const uint16_t COOLIX_HEADER_MARK = COOLIX_HEADER_MARK_TICKS * COOLIX_TICK;
const uint16_t COOLIX_HEADER_SPACE_TICKS = 8;
const uint16_t COOLIX_HEADER_SPACE = COOLIX_HEADER_SPACE_TICKS * COOLIX_TICK;
const uint16_t COOLIX_MIN_GAP_TICKS = COOLIX_HEADER_MARK_TICKS + COOLIX_ZERO_SPACE_TICKS;
const uint16_t COOLIX_MIN_GAP = COOLIX_MIN_GAP_TICKS * COOLIX_TICK;

const uint16_t COOLIX_BITS = 24;

climate::ClimateTraits CoolixClimate::traits() {
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

void CoolixClimate::setup() {
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

void CoolixClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->transmit_state_();
  this->publish_state();
}

void CoolixClimate::transmit_state_() {
  uint32_t remote_state;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      remote_state = (COOLIX_DEFAULT_STATE & ~COOLIX_MODE_MASK) | (COOLIX_COOL << 2);
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state = (COOLIX_DEFAULT_STATE & ~COOLIX_MODE_MASK) | (COOLIX_HEAT << 2);
      break;
    case climate::CLIMATE_MODE_AUTO:
      remote_state = COOLIX_DEFAULT_STATE_AUTO_24_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state = COOLIX_OFF;
      break;
  }
  if (this->mode != climate::CLIMATE_MODE_OFF) {
    auto temp = (uint8_t) roundf(clamp(this->target_temperature, COOLIX_TEMP_MIN, COOLIX_TEMP_MAX));
    remote_state &= ~COOLIX_TEMP_MASK;  // Clear the old temp.
    remote_state |= (COOLIX_TEMP_MAP[temp - COOLIX_TEMP_MIN] << 4);
  }

  ESP_LOGV(TAG, "Sending coolix code: %u", remote_state);

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

}  // namespace coolix
}  // namespace esphome
