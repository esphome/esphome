#include "coolix.h"
#include "esphome/core/log.h"

namespace esphome {
namespace coolix {

static const char *TAG = "coolix.climate";

const uint32_t COOLIX_OFF = 0xB27BE0;
const uint32_t COOLIX_SWING = 0xB26BE0;
const uint32_t COOLIX_LED = 0xB5F5A5;
const uint32_t COOLIX_SILENCE_FP = 0xB5F5B6;

// On, 25C, Mode: Auto, Fan: Auto, Zone Follow: Off, Sensor Temp: Ignore.
const uint32_t COOLIX_DEFAULT_STATE = 0xB2BFC8;
const uint32_t COOLIX_DEFAULT_STATE_AUTO_24_FAN = 0xB21F48;
const uint8_t COOLIX_COOL = 0b0000;
const uint8_t COOLIX_DRY_FAN = 0b0100;
const uint8_t COOLIX_AUTO = 0b1000;
const uint8_t COOLIX_HEAT = 0b1100;
const uint32_t COOLIX_MODE_MASK = 0b1100;
const uint32_t COOLIX_FAN_MASK = 0xF000;
const uint32_t COOLIX_FAN_DRY = 0x1000;
const uint32_t COOLIX_FAN_AUTO = 0xB000;
const uint32_t COOLIX_FAN_MIN = 0x9000;
const uint32_t COOLIX_FAN_MED = 0x5000;
const uint32_t COOLIX_FAN_MAX = 0x3000;

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
static const uint32_t HEADER_MARK_US = 4500;
static const uint32_t HEADER_SPACE_US = 4500;
static const uint32_t BIT_MARK_US = 560;
static const uint32_t BIT_ONE_SPACE_US = 1600;
static const uint32_t BIT_ZERO_SPACE_US = 530;
static const uint32_t FOOTER_MARK_US = BIT_MARK_US;
static const uint32_t FOOTER_SPACE_US = HEADER_SPACE_US;

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
      remote_state = (COOLIX_DEFAULT_STATE & ~COOLIX_MODE_MASK) | COOLIX_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state = (COOLIX_DEFAULT_STATE & ~COOLIX_MODE_MASK) | COOLIX_HEAT;
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

  ESP_LOGV(TAG, "Sending coolix code: 0x%02X", remote_state);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);
  uint16_t repeat = 1;
  for (uint16_t r = 0; r <= repeat; r++) {
    // Header
    data->mark(HEADER_MARK_US);
    data->space(HEADER_SPACE_US);
    // Data
    //   Break data into byte segments, starting at the Most Significant
    //   Byte. Each byte then being sent normal, then followed inverted.
    for (uint16_t i = 8; i <= COOLIX_BITS; i += 8) {
      // Grab a bytes worth of data.
      uint8_t segment = (remote_state >> (COOLIX_BITS - i)) & 0xFF;
      // Normal
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(BIT_MARK_US);
        data->space((segment & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
      }
      // Inverted
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(BIT_MARK_US);
        data->space(!(segment & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
      }
    }
    // Footer
    data->mark(BIT_MARK_US);
    data->space(FOOTER_SPACE_US);  // Pause before repeating
  }

  transmit.perform();
}

bool CoolixClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (data.size() != 200)
    return false;
  data.reset();
  uint32_t state = 0;
  uint32_t state_test;
  for (uint16_t r = 0; r <= 1; r++) {
    if (!data.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
      return false;
    state_test = 0;
    for (uint8_t a_segment = 0; a_segment < 3; a_segment++) {
      uint8_t segment = 0;
      for (int8_t a_bit = 7; a_bit >= 0; a_bit--) {
        if (data.peek_item(BIT_MARK_US, BIT_ONE_SPACE_US))
          segment |= 1 << a_bit;
        else if (!data.peek_item(BIT_MARK_US, BIT_ZERO_SPACE_US))
          return false;
        data.advance(2);
      }
      // Need to see this segment inverted
      for (int8_t a_bit = 7; a_bit >= 0; a_bit--)
        if (!data.expect_item(BIT_MARK_US,
          (segment & (1 << a_bit)) ? BIT_ZERO_SPACE_US : BIT_ONE_SPACE_US))
          return false;
      state_test |= segment << ((2-a_segment) * 8);
    }
    if (!data.expect_mark(BIT_MARK_US))
      return false;
    if (r == 0 && !data.expect_space(FOOTER_SPACE_US))
      return false;
    if (state == 0) state = state_test;
  }

  ESP_LOGV(TAG, "Decoded 0x%02X", state);
  if (state != state_test || (state & 0xFF0000) != 0xB20000) return false;

  if (state == COOLIX_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    if ((state & COOLIX_MODE_MASK) == COOLIX_HEAT)
      this->mode = climate::CLIMATE_MODE_HEAT;
    else if ((state & COOLIX_MODE_MASK) == COOLIX_AUTO)
      this->mode = climate::CLIMATE_MODE_AUTO;
    else if ((state & COOLIX_MODE_MASK) == COOLIX_DRY_FAN)  {
      // climate::CLIMATE_MODE_DRY;
      if ((state & COOLIX_FAN_MASK) == COOLIX_FAN_DRY)
        ESP_LOGV("Not supported DRY mode. Reporting AUTO");
      else
        ESP_LOGV("Not supported FAN Auto mode. Reporting AUTO");
      this->mode = climate::CLIMATE_MODE_AUTO;
    }
    else this->mode = climate::CLIMATE_MODE_COOL;

    // Fan Speed
    if ((state & COOLIX_FAN_AUTO) == COOLIX_FAN_AUTO) // || this->mode == climate::CLIMATE_MODE_DRY
      ESP_LOGV("Not supported FAN speed AUTO");
    else if ((state & COOLIX_FAN_MIN) == COOLIX_FAN_MIN)
      ESP_LOGV("Not supported FAN speed MIN");
    else if ((state & COOLIX_FAN_MED) == COOLIX_FAN_MED)
      ESP_LOGV("Not supported FAN speed MED");
    else if ((state & COOLIX_FAN_MAX) == COOLIX_FAN_MAX)
      ESP_LOGV("Not supported FAN speed MAX");

    // Temperature
    uint8_t temperature_code = (state & COOLIX_TEMP_MASK) >> 4;
    for (uint8_t i = 0; i < COOLIX_TEMP_RANGE; i++)
      if (COOLIX_TEMP_MAP[i] == temperature_code)
        this->target_temperature = i + COOLIX_TEMP_MIN;
  }
  this->publish_state();

  return true;
}

}  // namespace coolix
}  // namespace esphome
