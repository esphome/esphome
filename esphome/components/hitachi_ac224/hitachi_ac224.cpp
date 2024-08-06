#include "hitachi_ac224.h"

namespace esphome {
namespace hitachi_ac224 {

static const char *const TAG = "climate.hitachi_ac224";

uint8_t reverse_bits(uint8_t n) {
  uint8_t reversed = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    reversed <<= 1;
    reversed |= (n & 1);
    n >>= 1;
  }
  return reversed;
}

void set_bit(uint8_t *const data, const uint8_t position, const bool on) {
  uint8_t mask = 1 << position;
  if (on) {
    *data |= mask;
  } else {
    *data &= ~mask;
  }
}

bool get_bit(const uint8_t data, const uint8_t position) { return (data & (1 << position)) != 0; }

void HitachiClimate::set_checksum_() {
  uint8_t sum = 62;
  for (uint16_t i = 0; i < HITACHI_AC224_STATE_LENGTH - 1; i++) {
    sum -= reverse_bits(remote_state_[i]);
  }
  remote_state_[HITACHI_AC224_CHECKSUM_BYTE] = reverse_bits(sum);
}

bool HitachiClimate::get_power_() {
  return get_bit(remote_state_[HITACHI_AC224_POWER_BYTE], HITACHI_AC224_POWER_OFFSET);
}

void HitachiClimate::set_power_(bool on) {
  set_bit(&remote_state_[HITACHI_AC224_POWER_BYTE], HITACHI_AC224_POWER_OFFSET, on);
}

uint8_t HitachiClimate::get_mode_() { return reverse_bits(remote_state_[HITACHI_AC224_MODE_BYTE]); }

void HitachiClimate::set_mode_(uint8_t mode) {
  uint8_t new_mode = mode;
  switch (mode) {
    // Fan mode sets a special temp.
    case HITACHI_AC224_MODE_FAN:
      set_temp_(HITACHI_AC224_TEMP_FAN);
      break;
    case HITACHI_AC224_MODE_HEAT:
    case HITACHI_AC224_MODE_COOL:
    case HITACHI_AC224_MODE_DRY:
      break;
    default:
      new_mode = HITACHI_AC224_MODE_AUTO;
  }
  remote_state_[HITACHI_AC224_MODE_BYTE] = reverse_bits(new_mode);
  if (new_mode != HITACHI_AC224_MODE_FAN)
    set_temp_(previous_temp_);
  set_fan_(get_fan_());  // Reset the fan speed after the mode change.
  set_power_(true);
}

void HitachiClimate::set_temp_(uint8_t celsius) {
  uint8_t temp;
  if (celsius == HITACHI_AC224_TEMP_FAN) {
    temp = HITACHI_AC224_TEMP_FAN;
  } else {
    temp = std::min(celsius, HITACHI_AC224_TEMP_MAX);
    temp = std::max(temp, HITACHI_AC224_TEMP_MIN);
    previous_temp_ = temp;
  }
  remote_state_[HITACHI_AC224_TEMP_BYTE] = reverse_bits(temp << 1);
  if (temp == HITACHI_AC224_TEMP_MIN) {
    remote_state_[HITACHI_AC224_TEMP_BOOST_BYTE] = HITACHI_AC224_TEMP_BOOST_ON;
  } else {
    remote_state_[HITACHI_AC224_TEMP_BOOST_BYTE] = HITACHI_AC224_TEMP_BOOST_OFF;
  }
}

uint8_t HitachiClimate::get_fan_() { return reverse_bits(remote_state_[HITACHI_AC224_FAN_BYTE]); }

void HitachiClimate::set_fan_(uint8_t speed) {
  uint8_t fan_min = HITACHI_AC224_FAN_AUTO;
  uint8_t fan_max = HITACHI_AC224_FAN_HIGH;
  switch (get_mode_()) {
    case HITACHI_AC224_MODE_DRY:  // Only 2 x low speeds in Dry mode.
      fan_min = HITACHI_AC224_FAN_QUIET;
      fan_max = HITACHI_AC224_FAN_LOW;
      break;
    case HITACHI_AC224_MODE_FAN:
      fan_min = HITACHI_AC224_FAN_QUIET;  // No Auto in Fan mode.
      break;
  }
  uint8_t new_speed = std::max(speed, fan_min);
  new_speed = std::min(new_speed, fan_max);
  remote_state_[HITACHI_AC224_FAN_BYTE] = reverse_bits(new_speed);
}

void HitachiClimate::set_swing_v_(bool on) {
  set_bit(&remote_state_[HITACHI_AC224_SWINGV_BYTE], HITACHI_AC224_SWINGV_OFFSET, on);
}

bool HitachiClimate::get_swing_v_() {
  return get_bit(remote_state_[HITACHI_AC224_SWINGV_BYTE], HITACHI_AC224_SWINGV_OFFSET);
}

void HitachiClimate::set_swing_h_(bool on) {
  set_bit(&remote_state_[HITACHI_AC224_SWINGH_BYTE], HITACHI_AC224_SWINGH_OFFSET, on);
}

bool HitachiClimate::get_swing_h_() {
  return get_bit(remote_state_[HITACHI_AC224_SWINGH_BYTE], HITACHI_AC224_SWINGH_OFFSET);
}

void HitachiClimate::transmit_state() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      set_mode_(HITACHI_AC224_MODE_COOL);
      break;
    case climate::CLIMATE_MODE_DRY:
      set_mode_(HITACHI_AC224_MODE_DRY);
      break;
    case climate::CLIMATE_MODE_HEAT:
      set_mode_(HITACHI_AC224_MODE_HEAT);
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      set_mode_(HITACHI_AC224_MODE_AUTO);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      set_mode_(HITACHI_AC224_MODE_FAN);
      break;
    case climate::CLIMATE_MODE_OFF:
      set_power_(false);
      break;
    default:
      ESP_LOGW(TAG, "Unsupported mode: %s", LOG_STR_ARG(climate_mode_to_string(this->mode)));
  }

  set_temp_(static_cast<uint8_t>(this->target_temperature));

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_QUIET:
      set_fan_(HITACHI_AC224_FAN_QUIET);
      break;
    case climate::CLIMATE_FAN_LOW:
      set_fan_(HITACHI_AC224_FAN_LOW);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      set_fan_(HITACHI_AC224_FAN_MEDIUM);
      break;
    case climate::CLIMATE_FAN_HIGH:
      set_fan_(HITACHI_AC224_FAN_HIGH);
      break;
    case climate::CLIMATE_FAN_ON:
    case climate::CLIMATE_FAN_AUTO:
    default:
      set_fan_(HITACHI_AC224_FAN_AUTO);
  }

  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      set_swing_v_(true);
      set_swing_h_(true);
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      set_swing_v_(true);
      set_swing_h_(false);
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      set_swing_v_(false);
      set_swing_h_(true);
      break;
    case climate::CLIMATE_SWING_OFF:
      set_swing_v_(false);
      set_swing_h_(false);
      break;
  }

  set_checksum_();

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(HITACHI_AC224_FREQ);

  uint8_t repeat = 0;
  for (uint8_t r = 0; r <= repeat; r++) {
    // Header
    data->item(HITACHI_AC224_HDR_MARK, HITACHI_AC224_HDR_SPACE);
    // Data
    for (uint8_t i : remote_state_) {
      for (int8_t j = 7; j >= 0; j--) {
        data->mark(HITACHI_AC224_BIT_MARK);
        bool bit = i & (1 << j);
        data->space(bit ? HITACHI_AC224_ONE_SPACE : HITACHI_AC224_ZERO_SPACE);
      }
    }
    // Footer
    data->item(HITACHI_AC224_BIT_MARK, HITACHI_AC224_MIN_GAP);
  }
  transmit.perform();

  dump_state_("Sent", remote_state_);
}

void HitachiClimate::dump_state_(const char action[], uint8_t state[]) {
  for (uint16_t i = 0; i < HITACHI_AC224_STATE_LENGTH - 10; i += 10) {
    ESP_LOGV(TAG, "%s: %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X", action, state[i + 0], state[i + 1],
             state[i + 2], state[i + 3], state[i + 4], state[i + 5], state[i + 6], state[i + 7], state[i + 8],
             state[i + 9]);
  }
  ESP_LOGV(TAG, "%s: %02X %02X %02X %02X %02X  %02X %02X %02X", action, state[20], state[21], state[22], state[23],
           state[24], state[25], state[26], state[27]);
}

}  // namespace hitachi_ac224
}  // namespace esphome
