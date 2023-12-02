#include "hitachi_ac344.h"

namespace esphome {
namespace hitachi_ac344 {

static const char *const TAG = "climate.hitachi_ac344";

void set_bits(uint8_t *const dst, const uint8_t offset, const uint8_t nbits, const uint8_t data) {
  if (offset >= 8 || !nbits)
    return;  // Short circuit as it won't change.
  // Calculate the mask for the supplied value.
  uint8_t mask = UINT8_MAX >> (8 - ((nbits > 8) ? 8 : nbits));
  // Calculate the mask & clear the space for the data.
  // Clear the destination bits.
  *dst &= ~(uint8_t) (mask << offset);
  // Merge in the data.
  *dst |= ((data & mask) << offset);
}

void set_bit(uint8_t *const data, const uint8_t position, const bool on) {
  uint8_t mask = 1 << position;
  if (on) {
    *data |= mask;
  } else {
    *data &= ~mask;
  }
}

uint8_t *invert_byte_pairs(uint8_t *ptr, const uint16_t length) {
  for (uint16_t i = 1; i < length; i += 2) {
    // Code done this way to avoid a compiler warning bug.
    uint8_t inv = ~*(ptr + i - 1);
    *(ptr + i) = inv;
  }
  return ptr;
}

bool HitachiClimate::get_power_() { return remote_state_[HITACHI_AC344_POWER_BYTE] == HITACHI_AC344_POWER_ON; }

void HitachiClimate::set_power_(bool on) {
  set_button_(HITACHI_AC344_BUTTON_POWER);
  remote_state_[HITACHI_AC344_POWER_BYTE] = on ? HITACHI_AC344_POWER_ON : HITACHI_AC344_POWER_OFF;
}

uint8_t HitachiClimate::get_mode_() { return remote_state_[HITACHI_AC344_MODE_BYTE] & 0xF; }

void HitachiClimate::set_mode_(uint8_t mode) {
  uint8_t new_mode = mode;
  switch (mode) {
    // Fan mode sets a special temp.
    case HITACHI_AC344_MODE_FAN:
      set_temp_(HITACHI_AC344_TEMP_FAN, false);
      break;
    case HITACHI_AC344_MODE_HEAT:
    case HITACHI_AC344_MODE_COOL:
    case HITACHI_AC344_MODE_DRY:
      break;
    default:
      new_mode = HITACHI_AC344_MODE_COOL;
  }
  set_bits(&remote_state_[HITACHI_AC344_MODE_BYTE], 0, 4, new_mode);
  if (new_mode != HITACHI_AC344_MODE_FAN)
    set_temp_(previous_temp_);
  set_fan_(get_fan_());  // Reset the fan speed after the mode change.
  set_power_(true);
}

void HitachiClimate::set_temp_(uint8_t celsius, bool set_previous) {
  uint8_t temp;
  temp = std::min(celsius, HITACHI_AC344_TEMP_MAX);
  temp = std::max(temp, HITACHI_AC344_TEMP_MIN);
  set_bits(&remote_state_[HITACHI_AC344_TEMP_BYTE], HITACHI_AC344_TEMP_OFFSET, HITACHI_AC344_TEMP_SIZE, temp);
  if (previous_temp_ > temp) {
    set_button_(HITACHI_AC344_BUTTON_TEMP_DOWN);
  } else if (previous_temp_ < temp) {
    set_button_(HITACHI_AC344_BUTTON_TEMP_UP);
  }
  if (set_previous)
    previous_temp_ = temp;
}

uint8_t HitachiClimate::get_fan_() { return remote_state_[HITACHI_AC344_FAN_BYTE] >> 4 & 0xF; }

void HitachiClimate::set_fan_(uint8_t speed) {
  uint8_t new_speed = std::max(speed, HITACHI_AC344_FAN_MIN);
  uint8_t fan_max = HITACHI_AC344_FAN_MAX;

  // Only 2 x low speeds in Dry mode or Auto
  if (get_mode_() == HITACHI_AC344_MODE_DRY && speed == HITACHI_AC344_FAN_AUTO) {
    fan_max = HITACHI_AC344_FAN_AUTO;
  } else if (get_mode_() == HITACHI_AC344_MODE_DRY) {
    fan_max = HITACHI_AC344_FAN_MAX_DRY;
  } else if (get_mode_() == HITACHI_AC344_MODE_FAN && speed == HITACHI_AC344_FAN_AUTO) {
    // Fan Mode does not have auto. Set to safe low
    new_speed = HITACHI_AC344_FAN_MIN;
  }

  new_speed = std::min(new_speed, fan_max);
  // Handle the setting the button value if we are going to change the value.
  if (new_speed != get_fan_())
    set_button_(HITACHI_AC344_BUTTON_FAN);
  // Set the values

  set_bits(&remote_state_[HITACHI_AC344_FAN_BYTE], 4, 4, new_speed);
  remote_state_[9] = 0x92;

  // When fan is at min/max, additional bytes seem to be set
  if (new_speed == HITACHI_AC344_FAN_MIN)
    remote_state_[9] = 0x98;
  remote_state_[29] = 0x01;
}

void HitachiClimate::set_swing_v_toggle_(bool on) {
  uint8_t button = get_button_();  // Get the current button value.
  if (on) {
    button = HITACHI_AC344_BUTTON_SWINGV;              // Set the button to SwingV.
  } else if (button == HITACHI_AC344_BUTTON_SWINGV) {  // Asked to unset it
    // It was set previous, so use Power as a default
    button = HITACHI_AC344_BUTTON_POWER;
  }
  set_button_(button);
}

bool HitachiClimate::get_swing_v_toggle_() { return get_button_() == HITACHI_AC344_BUTTON_SWINGV; }

void HitachiClimate::set_swing_v_(bool on) {
  set_swing_v_toggle_(on);  // Set the button value.
  set_bit(&remote_state_[HITACHI_AC344_SWINGV_BYTE], HITACHI_AC344_SWINGV_OFFSET, on);
}

bool HitachiClimate::get_swing_v_() {
  return GETBIT8(remote_state_[HITACHI_AC344_SWINGV_BYTE], HITACHI_AC344_SWINGV_OFFSET);
}

void HitachiClimate::set_swing_h_(uint8_t position) {
  if (position > HITACHI_AC344_SWINGH_LEFT_MAX)
    return set_swing_h_(HITACHI_AC344_SWINGH_MIDDLE);
  set_bits(&remote_state_[HITACHI_AC344_SWINGH_BYTE], HITACHI_AC344_SWINGH_OFFSET, HITACHI_AC344_SWINGH_SIZE, position);
  set_button_(HITACHI_AC344_BUTTON_SWINGH);
}

uint8_t HitachiClimate::get_swing_h_() {
  return GETBITS8(remote_state_[HITACHI_AC344_SWINGH_BYTE], HITACHI_AC344_SWINGH_OFFSET, HITACHI_AC344_SWINGH_SIZE);
}

uint8_t HitachiClimate::get_button_() { return remote_state_[HITACHI_AC344_BUTTON_BYTE]; }

void HitachiClimate::set_button_(uint8_t button) { remote_state_[HITACHI_AC344_BUTTON_BYTE] = button; }

void HitachiClimate::transmit_state() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      set_mode_(HITACHI_AC344_MODE_COOL);
      break;
    case climate::CLIMATE_MODE_DRY:
      set_mode_(HITACHI_AC344_MODE_DRY);
      break;
    case climate::CLIMATE_MODE_HEAT:
      set_mode_(HITACHI_AC344_MODE_HEAT);
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      set_mode_(HITACHI_AC344_MODE_AUTO);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      set_mode_(HITACHI_AC344_MODE_FAN);
      break;
    case climate::CLIMATE_MODE_OFF:
      set_power_(false);
      break;
    default:
      ESP_LOGW(TAG, "Unsupported mode: %s", LOG_STR_ARG(climate_mode_to_string(this->mode)));
  }

  set_temp_(static_cast<uint8_t>(this->target_temperature));

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      set_fan_(HITACHI_AC344_FAN_LOW);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      set_fan_(HITACHI_AC344_FAN_MEDIUM);
      break;
    case climate::CLIMATE_FAN_HIGH:
      set_fan_(HITACHI_AC344_FAN_HIGH);
      break;
    case climate::CLIMATE_FAN_ON:
    case climate::CLIMATE_FAN_AUTO:
    default:
      set_fan_(HITACHI_AC344_FAN_AUTO);
  }

  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      set_swing_v_(true);
      set_swing_h_(HITACHI_AC344_SWINGH_AUTO);
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      set_swing_v_(true);
      set_swing_h_(HITACHI_AC344_SWINGH_MIDDLE);
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      set_swing_v_(false);
      set_swing_h_(HITACHI_AC344_SWINGH_AUTO);
      break;
    case climate::CLIMATE_SWING_OFF:
      set_swing_v_(false);
      set_swing_h_(HITACHI_AC344_SWINGH_MIDDLE);
      break;
  }

  // TODO: find change value to set button, now always set to power button
  set_button_(HITACHI_AC344_BUTTON_POWER);

  invert_byte_pairs(remote_state_ + 3, HITACHI_AC344_STATE_LENGTH - 3);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(HITACHI_AC344_FREQ);

  uint8_t repeat = 0;
  for (uint8_t r = 0; r <= repeat; r++) {
    // Header
    data->item(HITACHI_AC344_HDR_MARK, HITACHI_AC344_HDR_SPACE);
    // Data
    for (uint8_t i : remote_state_) {
      for (uint8_t j = 0; j < 8; j++) {
        data->mark(HITACHI_AC344_BIT_MARK);
        bool bit = i & (1 << j);
        data->space(bit ? HITACHI_AC344_ONE_SPACE : HITACHI_AC344_ZERO_SPACE);
      }
    }
    // Footer
    data->item(HITACHI_AC344_BIT_MARK, HITACHI_AC344_MIN_GAP);
  }
  transmit.perform();

  dump_state_("Sent", remote_state_);
}

bool HitachiClimate::parse_mode_(const uint8_t remote_state[]) {
  uint8_t power = remote_state[HITACHI_AC344_POWER_BYTE];
  ESP_LOGV(TAG, "Power: %02X %02X", remote_state[HITACHI_AC344_POWER_BYTE], power);
  uint8_t mode = remote_state[HITACHI_AC344_MODE_BYTE] & 0xF;
  ESP_LOGV(TAG, "Mode: %02X %02X", remote_state[HITACHI_AC344_MODE_BYTE], mode);
  if (power == HITACHI_AC344_POWER_ON) {
    switch (mode) {
      case HITACHI_AC344_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case HITACHI_AC344_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case HITACHI_AC344_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case HITACHI_AC344_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case HITACHI_AC344_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  return true;
}

bool HitachiClimate::parse_temperature_(const uint8_t remote_state[]) {
  uint8_t temperature =
      GETBITS8(remote_state[HITACHI_AC344_TEMP_BYTE], HITACHI_AC344_TEMP_OFFSET, HITACHI_AC344_TEMP_SIZE);
  this->target_temperature = temperature;
  ESP_LOGV(TAG, "Temperature: %02X %02u %04f", remote_state[HITACHI_AC344_TEMP_BYTE], temperature,
           this->target_temperature);
  return true;
}

bool HitachiClimate::parse_fan_(const uint8_t remote_state[]) {
  uint8_t fan_mode = remote_state[HITACHI_AC344_FAN_BYTE] >> 4 & 0xF;
  ESP_LOGV(TAG, "Fan: %02X %02X", remote_state[HITACHI_AC344_FAN_BYTE], fan_mode);
  switch (fan_mode) {
    case HITACHI_AC344_FAN_MIN:
    case HITACHI_AC344_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case HITACHI_AC344_FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case HITACHI_AC344_FAN_HIGH:
    case HITACHI_AC344_FAN_MAX:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case HITACHI_AC344_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }
  return true;
}

bool HitachiClimate::parse_swing_(const uint8_t remote_state[]) {
  uint8_t swing_modeh =
      GETBITS8(remote_state[HITACHI_AC344_SWINGH_BYTE], HITACHI_AC344_SWINGH_OFFSET, HITACHI_AC344_SWINGH_SIZE);
  ESP_LOGV(TAG, "SwingH: %02X %02X", remote_state[HITACHI_AC344_SWINGH_BYTE], swing_modeh);

  if ((swing_modeh & 0x3) == 0x3) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  }

  return true;
}

bool HitachiClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Validate header
  if (!data.expect_item(HITACHI_AC344_HDR_MARK, HITACHI_AC344_HDR_SPACE)) {
    ESP_LOGVV(TAG, "Header fail");
    return false;
  }

  uint8_t recv_state[HITACHI_AC344_STATE_LENGTH] = {0};
  // Read all bytes.
  for (uint8_t pos = 0; pos < HITACHI_AC344_STATE_LENGTH; pos++) {
    // Read bit
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(HITACHI_AC344_BIT_MARK, HITACHI_AC344_ONE_SPACE)) {
        recv_state[pos] |= 1 << bit;
      } else if (!data.expect_item(HITACHI_AC344_BIT_MARK, HITACHI_AC344_ZERO_SPACE)) {
        ESP_LOGVV(TAG, "Byte %d bit %d fail", pos, bit);
        return false;
      }
    }
  }

  // Validate footer
  if (!data.expect_mark(HITACHI_AC344_BIT_MARK)) {
    ESP_LOGVV(TAG, "Footer fail");
    return false;
  }

  dump_state_("Recv", recv_state);

  // parse mode
  this->parse_mode_(recv_state);
  // parse temperature
  this->parse_temperature_(recv_state);
  // parse fan
  this->parse_fan_(recv_state);
  // parse swingv
  this->parse_swing_(recv_state);
  this->publish_state();
  for (uint8_t i = 0; i < HITACHI_AC344_STATE_LENGTH; i++)
    remote_state_[i] = recv_state[i];

  return true;
}

void HitachiClimate::dump_state_(const char action[], uint8_t state[]) {
  for (uint16_t i = 0; i < HITACHI_AC344_STATE_LENGTH - 10; i += 10) {
    ESP_LOGV(TAG, "%s: %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X", action, state[i + 0], state[i + 1],
             state[i + 2], state[i + 3], state[i + 4], state[i + 5], state[i + 6], state[i + 7], state[i + 8],
             state[i + 9]);
  }
  ESP_LOGV(TAG, "%s: %02X %02X %02X", action, state[40], state[41], state[42]);
}

}  // namespace hitachi_ac344
}  // namespace esphome
