#include "coolix.h"
#include "esphome/core/log.h"

namespace esphome {
namespace coolix {

static const char *const TAG = "coolix.climate";

const uint32_t COOLIX_OFF = 0xB27BE0;
const uint32_t COOLIX_SWING = 0xB26BE0;
const uint32_t COOLIX_LED = 0xB5F5A5;
const uint32_t COOLIX_SILENCE_FP = 0xB5F5B6;

// On, 25C, Mode: Auto, Fan: Auto, Zone Follow: Off, Sensor Temp: Ignore.
const uint8_t COOLIX_COOL = 0b0000;
const uint8_t COOLIX_DRY_FAN = 0b0100;
const uint8_t COOLIX_AUTO = 0b1000;
const uint8_t COOLIX_HEAT = 0b1100;
const uint32_t COOLIX_MODE_MASK = 0b1100;
const uint32_t COOLIX_FAN_MASK = 0xF000;
const uint32_t COOLIX_FAN_MODE_AUTO_DRY = 0x1000;
const uint32_t COOLIX_FAN_AUTO = 0xB000;
const uint32_t COOLIX_FAN_MIN = 0x9000;
const uint32_t COOLIX_FAN_MED = 0x5000;
const uint32_t COOLIX_FAN_MAX = 0x3000;

// Temperature
const uint8_t COOLIX_TEMP_RANGE = COOLIX_TEMP_MAX - COOLIX_TEMP_MIN + 1;
const uint8_t COOLIX_FAN_TEMP_CODE = 0b11100000;  // Part of Fan Mode.
const uint32_t COOLIX_TEMP_MASK = 0b11110000;
const uint8_t COOLIX_TEMP_MAP[COOLIX_TEMP_RANGE] = {
    0b00000000,  // 17C
    0b00010000,  // 18c
    0b00110000,  // 19C
    0b00100000,  // 20C
    0b01100000,  // 21C
    0b01110000,  // 22C
    0b01010000,  // 23C
    0b01000000,  // 24C
    0b11000000,  // 25C
    0b11010000,  // 26C
    0b10010000,  // 27C
    0b10000000,  // 28C
    0b10100000,  // 29C
    0b10110000   // 30C
};

// Constants
static const uint32_t BIT_MARK_US = 660;
static const uint32_t HEADER_MARK_US = 560 * 8;
static const uint32_t HEADER_SPACE_US = 560 * 8;
static const uint32_t BIT_ONE_SPACE_US = 1500;
static const uint32_t BIT_ZERO_SPACE_US = 450;
static const uint32_t FOOTER_MARK_US = BIT_MARK_US;
static const uint32_t FOOTER_SPACE_US = HEADER_SPACE_US;

const uint16_t COOLIX_BITS = 24;

void CoolixClimate::transmit_state() {
  uint32_t remote_state = 0xB20F00;

  if (send_swing_cmd_) {
    send_swing_cmd_ = false;
    remote_state = COOLIX_SWING;
  } else {
    switch (this->mode) {
      case climate::CLIMATE_MODE_COOL:
        remote_state |= COOLIX_COOL;
        break;
      case climate::CLIMATE_MODE_HEAT:
        remote_state |= COOLIX_HEAT;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        remote_state |= COOLIX_AUTO;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
      case climate::CLIMATE_MODE_DRY:
        remote_state |= COOLIX_DRY_FAN;
        break;
      case climate::CLIMATE_MODE_OFF:
      default:
        remote_state = COOLIX_OFF;
        break;
    }
    if (this->mode != climate::CLIMATE_MODE_OFF) {
      if (this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
        auto temp = (uint8_t) roundf(clamp<float>(this->target_temperature, COOLIX_TEMP_MIN, COOLIX_TEMP_MAX));
        remote_state |= COOLIX_TEMP_MAP[temp - COOLIX_TEMP_MIN];
      } else {
        remote_state |= COOLIX_FAN_TEMP_CODE;
      }
      if (this->mode == climate::CLIMATE_MODE_HEAT_COOL || this->mode == climate::CLIMATE_MODE_DRY) {
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        remote_state |= COOLIX_FAN_MODE_AUTO_DRY;
      } else {
        switch (this->fan_mode.value()) {
          case climate::CLIMATE_FAN_HIGH:
            remote_state |= COOLIX_FAN_MAX;
            break;
          case climate::CLIMATE_FAN_MEDIUM:
            remote_state |= COOLIX_FAN_MED;
            break;
          case climate::CLIMATE_FAN_LOW:
            remote_state |= COOLIX_FAN_MIN;
            break;
          case climate::CLIMATE_FAN_AUTO:
          default:
            remote_state |= COOLIX_FAN_AUTO;
            break;
        }
      }
    }
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
    //   Break data into bytes, starting at the Most Significant
    //   Byte. Each byte then being sent normal, then followed inverted.
    for (uint16_t i = 8; i <= COOLIX_BITS; i += 8) {
      // Grab a bytes worth of data.
      uint8_t byte = (remote_state >> (COOLIX_BITS - i)) & 0xFF;
      // Normal
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(BIT_MARK_US);
        data->space((byte & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
      }
      // Inverted
      for (uint64_t mask = 1ULL << 7; mask; mask >>= 1) {
        data->mark(BIT_MARK_US);
        data->space(!(byte & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
      }
    }
    // Footer
    data->mark(BIT_MARK_US);
    data->space(FOOTER_SPACE_US);  // Pause before repeating
  }

  transmit.perform();
}

bool CoolixClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Decoded remote state y 3 bytes long code.
  uint32_t remote_state = 0;
  // The protocol sends the data twice, read here
  uint32_t loop_read;
  for (uint16_t loop = 1; loop <= 2; loop++) {
    if (!data.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
      return false;
    loop_read = 0;
    for (uint8_t a_byte = 0; a_byte < 3; a_byte++) {
      uint8_t byte = 0;
      for (int8_t a_bit = 7; a_bit >= 0; a_bit--) {
        if (data.expect_item(BIT_MARK_US, BIT_ONE_SPACE_US))
          byte |= 1 << a_bit;
        else if (!data.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US))
          return false;
      }
      // Need to see this segment inverted
      for (int8_t a_bit = 7; a_bit >= 0; a_bit--) {
        bool bit = byte & (1 << a_bit);
        if (!data.expect_item(BIT_MARK_US, bit ? BIT_ZERO_SPACE_US : BIT_ONE_SPACE_US))
          return false;
      }
      // Receiving MSB first: reorder bytes
      loop_read |= byte << ((2 - a_byte) * 8);
    }
    // Footer Mark
    if (!data.expect_mark(BIT_MARK_US))
      return false;
    if (loop == 1) {
      // Back up state on first loop
      remote_state = loop_read;
      if (!data.expect_space(FOOTER_SPACE_US))
        return false;
    }
  }

  ESP_LOGV(TAG, "Decoded 0x%02X", remote_state);
  if (remote_state != loop_read || (remote_state & 0xFF0000) != 0xB20000)
    return false;

  if (remote_state == COOLIX_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else if (remote_state == COOLIX_SWING) {
    this->swing_mode =
        this->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  } else {
    if ((remote_state & COOLIX_MODE_MASK) == COOLIX_HEAT)
      this->mode = climate::CLIMATE_MODE_HEAT;
    else if ((remote_state & COOLIX_MODE_MASK) == COOLIX_AUTO)
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
    else if ((remote_state & COOLIX_MODE_MASK) == COOLIX_DRY_FAN) {
      if ((remote_state & COOLIX_FAN_MASK) == COOLIX_FAN_MODE_AUTO_DRY)
        this->mode = climate::CLIMATE_MODE_DRY;
      else
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
    } else
      this->mode = climate::CLIMATE_MODE_COOL;

    // Fan Speed
    if ((remote_state & COOLIX_FAN_AUTO) == COOLIX_FAN_AUTO || this->mode == climate::CLIMATE_MODE_HEAT_COOL ||
        this->mode == climate::CLIMATE_MODE_DRY)
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
    else if ((remote_state & COOLIX_FAN_MIN) == COOLIX_FAN_MIN)
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    else if ((remote_state & COOLIX_FAN_MED) == COOLIX_FAN_MED)
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    else if ((remote_state & COOLIX_FAN_MAX) == COOLIX_FAN_MAX)
      this->fan_mode = climate::CLIMATE_FAN_HIGH;

    // Temperature
    uint8_t temperature_code = remote_state & COOLIX_TEMP_MASK;
    for (uint8_t i = 0; i < COOLIX_TEMP_RANGE; i++)
      if (COOLIX_TEMP_MAP[i] == temperature_code)
        this->target_temperature = i + COOLIX_TEMP_MIN;
  }
  this->publish_state();

  return true;
}

}  // namespace coolix
}  // namespace esphome
