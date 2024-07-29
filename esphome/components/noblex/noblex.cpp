#include "noblex.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace noblex {

static const char *const TAG = "noblex.climate";

const uint16_t NOBLEX_HEADER_MARK = 9000;
const uint16_t NOBLEX_HEADER_SPACE = 4500;
const uint16_t NOBLEX_BIT_MARK = 660;
const uint16_t NOBLEX_ONE_SPACE = 1640;
const uint16_t NOBLEX_ZERO_SPACE = 520;
const uint32_t NOBLEX_GAP = 20000;
const uint8_t NOBLEX_POWER = 0x10;

using IRNoblexMode = enum IRNoblexMode {
  IR_NOBLEX_MODE_AUTO = 0b000,
  IR_NOBLEX_MODE_COOL = 0b100,
  IR_NOBLEX_MODE_DRY = 0b010,
  IR_NOBLEX_MODE_FAN = 0b110,
  IR_NOBLEX_MODE_HEAT = 0b001,
};

using IRNoblexFan = enum IRNoblexFan {
  IR_NOBLEX_FAN_AUTO = 0b00,
  IR_NOBLEX_FAN_LOW = 0b10,
  IR_NOBLEX_FAN_MEDIUM = 0b01,
  IR_NOBLEX_FAN_HIGH = 0b11,
};

// Transmit via IR the state of this climate controller.
void NoblexClimate::transmit_state() {
  uint8_t remote_state[8] = {0x80, 0x10, 0x00, 0x0A, 0x50, 0x00, 0x20, 0x00};  // OFF, COOL, 24C, FAN_AUTO

  auto powered_on = this->mode != climate::CLIMATE_MODE_OFF;
  if (powered_on) {
    remote_state[0] |= 0x10;  // power bit
    remote_state[2] = 0x02;
  }
  if (powered_on != this->powered_on_assumed)
    this->powered_on_assumed = powered_on;

  auto temp = (uint8_t) roundf(clamp<float>(this->target_temperature, NOBLEX_TEMP_MIN, NOBLEX_TEMP_MAX));
  remote_state[1] = reverse_bits(uint8_t((temp - NOBLEX_TEMP_MIN) & 0x0F));

  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state[0] |= (IRNoblexMode::IR_NOBLEX_MODE_AUTO << 5);
      remote_state[1] = 0x90;  // NOBLEX_TEMP_MAP 25C
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[0] |= (IRNoblexMode::IR_NOBLEX_MODE_COOL << 5);
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state[0] |= (IRNoblexMode::IR_NOBLEX_MODE_DRY << 5);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state[0] |= (IRNoblexMode::IR_NOBLEX_MODE_FAN << 5);
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[0] |= (IRNoblexMode::IR_NOBLEX_MODE_HEAT << 5);
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      powered_on = false;
      this->powered_on_assumed = powered_on;
      remote_state[0] &= 0xEF;
      remote_state[2] = 0x00;
      break;
  }

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      remote_state[0] |= (IRNoblexFan::IR_NOBLEX_FAN_LOW << 2);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state[0] |= (IRNoblexFan::IR_NOBLEX_FAN_MEDIUM << 2);
      break;
    case climate::CLIMATE_FAN_HIGH:
      remote_state[0] |= (IRNoblexFan::IR_NOBLEX_FAN_HIGH << 2);
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      remote_state[0] |= (IRNoblexFan::IR_NOBLEX_FAN_AUTO << 2);
      break;
  }

  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      remote_state[0] |= 0x02;
      remote_state[4] = 0x58;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      remote_state[0] &= 0xFD;
      remote_state[4] = 0x50;
      break;
  }

  uint8_t crc = 0;
  for (uint8_t i : remote_state) {
    crc += reverse_bits(i);
  }
  crc = reverse_bits(uint8_t(crc & 0x0F)) >> 4;

  ESP_LOGD(TAG, "Sending noblex code: %02X%02X %02X%02X %02X%02X %02X%02X", remote_state[0], remote_state[1],
           remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6], remote_state[7]);

  ESP_LOGV(TAG, "CRC: %01X", crc);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(38000);

  // Header
  data->mark(NOBLEX_HEADER_MARK);
  data->space(NOBLEX_HEADER_SPACE);
  // Data (sent remote_state from the MSB to the LSB)
  for (uint8_t i : remote_state) {
    for (int8_t j = 7; j >= 0; j--) {
      if ((i == 4) & (j == 4)) {
        // Header intermediate
        data->mark(NOBLEX_BIT_MARK);
        data->space(NOBLEX_GAP);  // gap en bit 36
      } else {
        data->mark(NOBLEX_BIT_MARK);
        bool bit = i & (1 << j);
        data->space(bit ? NOBLEX_ONE_SPACE : NOBLEX_ZERO_SPACE);
      }
    }
  }
  // send crc
  for (int8_t i = 3; i >= 0; i--) {
    data->mark(NOBLEX_BIT_MARK);
    bool bit = crc & (1 << i);
    data->space(bit ? NOBLEX_ONE_SPACE : NOBLEX_ZERO_SPACE);
  }
  // Footer
  data->mark(NOBLEX_BIT_MARK);

  transmit.perform();
}  // end transmit_state()

// Handle received IR Buffer
bool NoblexClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t remote_state[8] = {0};
  uint8_t crc = 0, crc_calculated = 0;

  if (!receiving_) {
    // Validate header
    if (data.expect_item(NOBLEX_HEADER_MARK, NOBLEX_HEADER_SPACE)) {
      ESP_LOGV(TAG, "Header");
      receiving_ = true;
      // Read first 36 bits
      for (int i = 0; i < 5; i++) {
        // Read bit
        for (int j = 7; j >= 0; j--) {
          if ((i == 4) & (j == 4)) {
            remote_state[i] |= 1 << j;
            // Header intermediate
            ESP_LOGVV(TAG, "GAP");
            return false;
          } else if (data.expect_item(NOBLEX_BIT_MARK, NOBLEX_ONE_SPACE)) {
            remote_state[i] |= 1 << j;
          } else if (!data.expect_item(NOBLEX_BIT_MARK, NOBLEX_ZERO_SPACE)) {
            ESP_LOGVV(TAG, "Byte %d bit %d fail", i, j);
            return false;
          }
        }
        ESP_LOGV(TAG, "Byte %d %02X", i, remote_state[i]);
      }

    } else {
      ESP_LOGV(TAG, "Header fail");
      receiving_ = false;
      return false;
    }

  } else {
    // Read the remaining 28 bits
    for (int i = 4; i < 8; i++) {
      // Read bit
      for (int j = 7; j >= 0; j--) {
        if ((i == 4) & (j >= 4)) {
          // nothing
        } else if (data.expect_item(NOBLEX_BIT_MARK, NOBLEX_ONE_SPACE)) {
          remote_state[i] |= 1 << j;
        } else if (!data.expect_item(NOBLEX_BIT_MARK, NOBLEX_ZERO_SPACE)) {
          ESP_LOGVV(TAG, "Byte %d bit %d fail", i, j);
          return false;
        }
      }
      ESP_LOGV(TAG, "Byte %d %02X", i, remote_state[i]);
    }

    // Read crc
    for (int i = 3; i >= 0; i--) {
      if (data.expect_item(NOBLEX_BIT_MARK, NOBLEX_ONE_SPACE)) {
        crc |= 1 << i;
      } else if (!data.expect_item(NOBLEX_BIT_MARK, NOBLEX_ZERO_SPACE)) {
        ESP_LOGVV(TAG, "Bit %d CRC fail", i);
        return false;
      }
    }
    ESP_LOGV(TAG, "CRC %02X", crc);

    // Validate footer
    if (!data.expect_mark(NOBLEX_BIT_MARK)) {
      ESP_LOGV(TAG, "Footer fail");
      return false;
    }
    receiving_ = false;
  }

  for (uint8_t i : remote_state)
    crc_calculated += reverse_bits(i);
  crc_calculated = reverse_bits(uint8_t(crc_calculated & 0x0F)) >> 4;
  ESP_LOGVV(TAG, "CRC calc %02X", crc_calculated);

  if (crc != crc_calculated) {
    ESP_LOGV(TAG, "CRC fail");
    return false;
  }

  ESP_LOGD(TAG, "Received noblex code: %02X%02X %02X%02X %02X%02X %02X%02X", remote_state[0], remote_state[1],
           remote_state[2], remote_state[3], remote_state[4], remote_state[5], remote_state[6], remote_state[7]);

  auto powered_on = false;
  if ((remote_state[0] & NOBLEX_POWER) == NOBLEX_POWER) {
    powered_on = true;
    this->powered_on_assumed = powered_on;
  } else {
    powered_on = false;
    this->powered_on_assumed = powered_on;
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  // powr on/off button
  ESP_LOGV(TAG, "Power: %01X", powered_on);

  // Set received mode
  if (powered_on_assumed) {
    auto mode = (remote_state[0] & 0xE0) >> 5;
    ESP_LOGV(TAG, "Mode: %02X", mode);
    switch (mode) {
      case IRNoblexMode::IR_NOBLEX_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case IRNoblexMode::IR_NOBLEX_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case IRNoblexMode::IR_NOBLEX_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case IRNoblexMode::IR_NOBLEX_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case IRNoblexMode::IR_NOBLEX_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
    }
  }

  // Set received temp
  uint8_t temp = remote_state[1];
  ESP_LOGVV(TAG, "Temperature Raw: %02X", temp);

  temp = 0x0F & reverse_bits(temp);
  temp += NOBLEX_TEMP_MIN;
  ESP_LOGV(TAG, "Temperature Climate: %u", temp);
  this->target_temperature = temp;

  // Set received fan speed
  auto fan = (remote_state[0] & 0x0C) >> 2;
  ESP_LOGV(TAG, "Fan: %02X", fan);
  switch (fan) {
    case IRNoblexFan::IR_NOBLEX_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case IRNoblexFan::IR_NOBLEX_FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case IRNoblexFan::IR_NOBLEX_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case IRNoblexFan::IR_NOBLEX_FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // Set received swing status
  if (remote_state[0] & 0x02) {
    ESP_LOGV(TAG, "Swing vertical");
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    ESP_LOGV(TAG, "Swing OFF");
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  for (uint8_t &i : remote_state)
    i = 0;
  this->publish_state();
  return true;
}  // end on_receive()

}  // namespace noblex
}  // namespace esphome
