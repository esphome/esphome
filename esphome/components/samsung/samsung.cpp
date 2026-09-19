#include "samsung.h"
#include <cinttypes>
#include "esphome/core/helpers.h"

namespace esphome::samsung {

static const char *const TAG = "samsung.climate";

void SamsungClimate::transmit_state() {
  const bool was_on = this->current_climate_mode_ != climate::ClimateMode::CLIMATE_MODE_OFF;
  const bool power_on = this->mode != climate::ClimateMode::CLIMATE_MODE_OFF;

  this->current_climate_mode_ = this->mode;

  if (power_on) {
    std::memcpy(this->protocol_.raw, K_RESET, K_SAMSUNG_AC_EXTENDED_STATE_LENGTH);
    this->set_climate_mode_(this->mode);
  }  // else: keep the last transmitted mode so the power off frame still carries a valid state

  this->set_temp_(static_cast<uint8_t>(this->target_temperature));
  this->set_swing_mode_(this->swing_mode);
  this->set_fan_(this->fan_mode.has_value() ? this->fan_mode.value() : climate::CLIMATE_FAN_AUTO);
  this->set_power_(power_on);

  if (power_on != was_on) {
    this->send_power_state_(power_on);
  } else {
    this->send_();
  }
}

bool SamsungClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(SAMSUNG_AIRCON1_HDR_MARK, SAMSUNG_AIRCON1_HDR_SPACE)) {
    return false;
  }

  ESP_LOGD(TAG, "Received Samsung A/C message size %" PRId32, data.size());
  uint8_t received_length = K_SAMSUNG_AC_EXTENDED_STATE_LENGTH;
  for (uint8_t i = 0; i < K_SAMSUNG_AC_EXTENDED_STATE_LENGTH; i++) {
    if (i != 0 && i % K_SAMSUNG_AC_SECTION_LENGTH == 0) {
      // Each section (7 bytes) is separated by a MSG_SPACE followed by a new header.
      const bool have_separator = data.expect_item(SAMSUNG_AIRCON1_BIT_MARK, SAMSUNG_AIRCON1_MSG_SPACE) &&
                                  data.expect_item(SAMSUNG_AIRCON1_HDR_MARK, SAMSUNG_AIRCON1_HDR_SPACE);
      if (!have_separator) {
        // Sections beyond the standard frame are optional; a missing separator marks the end.
        if (i >= K_SAMSUNG_AC_STATE_LENGTH) {
          ESP_LOGV(TAG, "End of frame after %" PRIu8 " bytes", i);
          received_length = i;
          break;
        }
        ESP_LOGW(TAG, "Failed to receive section separator before byte %" PRIu8, i);
        return false;
      }
      ESP_LOGV(TAG, "Received section separator before byte %" PRIu8, i);
    }

    for (uint8_t y = 0; y < 8; y++) {
      if (data.expect_item(SAMSUNG_AIRCON1_BIT_MARK, SAMSUNG_AIRCON1_ONE_SPACE)) {
        this->protocol_.raw[i] |= 1 << y;
      } else if (data.expect_item(SAMSUNG_AIRCON1_BIT_MARK, SAMSUNG_AIRCON1_ZERO_SPACE)) {
        this->protocol_.raw[i] &= ~(1 << y);
      } else {
        ESP_LOGW(TAG, "Failed to receive bit %" PRIu8 " in byte %" PRIu8, y, i);
        return false;
      }
    }
  }

  if (!data.expect_mark(SAMSUNG_AIRCON1_BIT_MARK)) {
    ESP_LOGVV(TAG, "Footer fail");
    return false;
  }

  if (received_length == K_SAMSUNG_AC_EXTENDED_STATE_LENGTH) {
    // In an extended frame the 2nd section carries timer data and the 3rd section holds bytes 7..13
    // of the standard state. Fold it back so the standard message map decodes the right bits.
    std::memcpy(this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH,
                this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH * 2, K_SAMSUNG_AC_SECTION_LENGTH);
  }

  this->update_swing_mode_();
  this->update_temp_();
  this->update_fan_();

  if (this->is_power_off_()) {
    this->mode = climate::ClimateMode::CLIMATE_MODE_OFF;
    this->current_climate_mode_ = this->mode;
  } else {
    this->update_climate_mode_();
  }

  ESP_LOGD(TAG, "Reception successful, power is %s, publishing new state.", ONOFF(!this->is_power_off_()));
  this->publish_state();
  return true;
}

void SamsungClimate::send_(const uint16_t length) {
  this->checksum_();

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  // Header(2) + (sections - 1) * MSG_HDR(2 * Item(2)) + length Bytes * 8 Bits * Bit(2) + Last Mark (1)
  const uint16_t sections = length / K_SAMSUNG_AC_SECTION_LENGTH;
  data->reserve(2 + (sections - 1) * 2 * 2 + length * 8 * 2 + 1);
  data->set_carrier_frequency(SAMSUNG_IR_FREQUENCY_HZ);

  // Header
  data->item(SAMSUNG_AIRCON1_HDR_MARK, SAMSUNG_AIRCON1_HDR_SPACE);

  for (uint16_t i = 0; i < length; i++) {
    if (i != 0 && i % K_SAMSUNG_AC_SECTION_LENGTH == 0) {
      data->item(SAMSUNG_AIRCON1_BIT_MARK, SAMSUNG_AIRCON1_MSG_SPACE);
      data->item(SAMSUNG_AIRCON1_HDR_MARK, SAMSUNG_AIRCON1_HDR_SPACE);
    }

    uint8_t send_byte = this->protocol_.raw[i];

    for (uint8_t y = 0; y < 8; y++) {
      if (send_byte & 0x01) {
        data->item(SAMSUNG_AIRCON1_BIT_MARK, SAMSUNG_AIRCON1_ONE_SPACE);
      } else {
        data->item(SAMSUNG_AIRCON1_BIT_MARK, SAMSUNG_AIRCON1_ZERO_SPACE);
      }

      send_byte >>= 1;
    }
  }

  data->mark(SAMSUNG_AIRCON1_BIT_MARK);

  transmit.perform();
}

void SamsungClimate::set_swing_mode_(const climate::ClimateSwingMode swing_mode) {
  switch (swing_mode) {
    case climate::ClimateSwingMode::CLIMATE_SWING_BOTH:
      this->protocol_.swing = K_SAMSUNG_AC_SWING_BOTH;
      break;
    case climate::ClimateSwingMode::CLIMATE_SWING_HORIZONTAL:
      this->protocol_.swing = K_SAMSUNG_AC_SWING_H;
      break;
    case climate::ClimateSwingMode::CLIMATE_SWING_VERTICAL:
      this->protocol_.swing = K_SAMSUNG_AC_SWING_V;
      break;
    case climate::ClimateSwingMode::CLIMATE_SWING_OFF:
    default:
      this->protocol_.swing = K_SAMSUNG_AC_SWING_OFF;
      break;
  }
}

void SamsungClimate::update_swing_mode_() {
  switch (this->protocol_.swing) {
    case K_SAMSUNG_AC_SWING_BOTH:
      this->swing_mode = climate::ClimateSwingMode::CLIMATE_SWING_BOTH;
      break;
    case K_SAMSUNG_AC_SWING_H:
      this->swing_mode = climate::ClimateSwingMode::CLIMATE_SWING_HORIZONTAL;
      break;
    case K_SAMSUNG_AC_SWING_V:
      this->swing_mode = climate::ClimateSwingMode::CLIMATE_SWING_VERTICAL;
      break;
    case K_SAMSUNG_AC_SWING_OFF:
    default:
      this->swing_mode = climate::ClimateSwingMode::CLIMATE_SWING_OFF;
      break;
  }
}

void SamsungClimate::set_climate_mode_(const climate::ClimateMode climate_mode) {
  switch (climate_mode) {
    case climate::ClimateMode::CLIMATE_MODE_HEAT:
      this->protocol_.mode = K_SAMSUNG_AC_HEAT;
      break;
    case climate::ClimateMode::CLIMATE_MODE_DRY:
      this->protocol_.mode = K_SAMSUNG_AC_DRY;
      break;
    case climate::ClimateMode::CLIMATE_MODE_COOL:
      this->protocol_.mode = K_SAMSUNG_AC_COOL;
      break;
    case climate::ClimateMode::CLIMATE_MODE_FAN_ONLY:
      this->protocol_.mode = K_SAMSUNG_AC_FAN;
      break;
    case climate::ClimateMode::CLIMATE_MODE_HEAT_COOL:
    case climate::ClimateMode::CLIMATE_MODE_AUTO:
    default:
      this->protocol_.mode = K_SAMSUNG_AC_AUTO;
      break;
  }
}

void SamsungClimate::update_climate_mode_() {
  switch (this->protocol_.mode) {
    case K_SAMSUNG_AC_HEAT:
      this->mode = climate::ClimateMode::CLIMATE_MODE_HEAT;
      break;
    case K_SAMSUNG_AC_DRY:
      this->mode = climate::ClimateMode::CLIMATE_MODE_DRY;
      break;
    case K_SAMSUNG_AC_COOL:
      this->mode = climate::ClimateMode::CLIMATE_MODE_COOL;
      break;
    case K_SAMSUNG_AC_FAN:
      this->mode = climate::ClimateMode::CLIMATE_MODE_FAN_ONLY;
      break;
    case K_SAMSUNG_AC_AUTO:
    default:
      this->mode = climate::ClimateMode::CLIMATE_MODE_AUTO;
      break;
  }

  this->current_climate_mode_ = this->mode;
}

void SamsungClimate::set_temp_(const uint8_t temp) {
  this->protocol_.temp =
      esphome::clamp<uint8_t>(temp, K_SAMSUNG_AC_MIN_TEMP, K_SAMSUNG_AC_MAX_TEMP) - K_SAMSUNG_AC_MIN_TEMP;
}

void SamsungClimate::update_temp_() { this->target_temperature = this->protocol_.temp + K_SAMSUNG_AC_MIN_TEMP; }

void SamsungClimate::send_power_state_(const bool on) {
  static const uint8_t K_TIMER_SECTION[K_SAMSUNG_AC_SECTION_LENGTH] = {0x01, 0xD2, 0x0F, 0x00, 0x00, 0x00, 0x00};

  this->set_power_(on);

  // Expand the standard state into an extended frame: section 3 takes bytes 7..13, section 2 the timer block.
  std::memcpy(this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH * 2, this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH,
              K_SAMSUNG_AC_SECTION_LENGTH);
  std::memcpy(this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH, K_TIMER_SECTION, K_SAMSUNG_AC_SECTION_LENGTH);

  this->send_(K_SAMSUNG_AC_EXTENDED_STATE_LENGTH);

  // Collapse back to the standard layout so following frames are built from a valid state.
  std::memcpy(this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH, this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH * 2,
              K_SAMSUNG_AC_SECTION_LENGTH);
}

void SamsungClimate::set_power_(const bool on) { this->protocol_.power_1 = this->protocol_.power_2 = on ? 0b11 : 0b00; }

bool SamsungClimate::is_power_off_() { return this->protocol_.power_1 != 0b11 || this->protocol_.power_2 != 0b11; }

void SamsungClimate::set_fan_(const climate::ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case climate::ClimateFanMode::CLIMATE_FAN_LOW:
      this->protocol_.fan = K_SAMSUNG_AC_FAN_LOW;
      break;
    case climate::ClimateFanMode::CLIMATE_FAN_MEDIUM:
      this->protocol_.fan = K_SAMSUNG_AC_FAN_MED;
      break;
    case climate::ClimateFanMode::CLIMATE_FAN_HIGH:
      this->protocol_.fan = K_SAMSUNG_AC_FAN_HIGH;
      break;
    case climate::ClimateFanMode::CLIMATE_FAN_AUTO:
    default:
      this->protocol_.fan = K_SAMSUNG_AC_FAN_AUTO;
      break;
  }
}

void SamsungClimate::update_fan_() {
  switch (this->protocol_.fan) {
    case K_SAMSUNG_AC_FAN_LOW:
      this->fan_mode = climate::ClimateFanMode::CLIMATE_FAN_LOW;
      break;
    case K_SAMSUNG_AC_FAN_MED:
      this->fan_mode = climate::ClimateFanMode::CLIMATE_FAN_MEDIUM;
      break;
    case K_SAMSUNG_AC_FAN_HIGH:
      this->fan_mode = climate::ClimateFanMode::CLIMATE_FAN_HIGH;
      break;
    case K_SAMSUNG_AC_FAN_AUTO:
    default:
      this->fan_mode = climate::ClimateFanMode::CLIMATE_FAN_AUTO;
      break;
  }
}

uint8_t SamsungClimate::calc_section_checksum(const uint8_t *section) {
  uint8_t sum = 0;

  sum += count_bits(*section, 8);  // Include the entire first byte
  // The lower half of the second byte.
  sum += count_bits(get_bits8(*(section + 1), K_LOW_NIBBLE, K_NIBBLE_SIZE), 8);
  // The upper half of the third byte.
  sum += count_bits(get_bits8(*(section + 2), K_HIGH_NIBBLE, K_NIBBLE_SIZE), 8);
  // The next 4 bytes.
  sum += count_bits(section + 3, 4);
  // Bitwise invert the result.
  return sum ^ UINT8_MAX;
}

void SamsungClimate::checksum_() {
  uint8_t sectionsum = calc_section_checksum(this->protocol_.raw);
  this->protocol_.sum_1_upper = get_bits8(sectionsum, K_HIGH_NIBBLE, K_NIBBLE_SIZE);
  this->protocol_.sum_1_lower = get_bits8(sectionsum, K_LOW_NIBBLE, K_NIBBLE_SIZE);
  sectionsum = calc_section_checksum(this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH);
  this->protocol_.sum_2_upper = get_bits8(sectionsum, K_HIGH_NIBBLE, K_NIBBLE_SIZE);
  this->protocol_.sum_2_lower = get_bits8(sectionsum, K_LOW_NIBBLE, K_NIBBLE_SIZE);
  sectionsum = calc_section_checksum(this->protocol_.raw + K_SAMSUNG_AC_SECTION_LENGTH * 2);
  this->protocol_.sum_3_upper = get_bits8(sectionsum, K_HIGH_NIBBLE, K_NIBBLE_SIZE);
  this->protocol_.sum_3_lower = get_bits8(sectionsum, K_LOW_NIBBLE, K_NIBBLE_SIZE);
}

uint16_t SamsungClimate::count_bits(const uint8_t *const start, const uint16_t length, const bool ones,
                                    const uint16_t init) {
  uint16_t count = init;

  for (uint16_t offset = 0; offset < length; offset++) {
    for (uint8_t currentbyte = *(start + offset); currentbyte; currentbyte >>= 1) {
      if (currentbyte & 1)
        count++;
    }
  }

  if (ones || length == 0) {
    return count;
  } else {
    return (length * 8) - count;
  }
}

uint16_t SamsungClimate::count_bits(const uint64_t data, const uint8_t length, const bool ones, const uint16_t init) {
  uint16_t count = init;
  uint8_t bits_so_far = length;

  for (uint64_t remainder = data; remainder && bits_so_far; remainder >>= 1, bits_so_far--) {
    if (remainder & 1)
      count++;
  }

  if (ones || length == 0) {
    return count;
  } else {
    return length - count;
  }
}
}  // namespace esphome::samsung
