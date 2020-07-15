#include "toshiba.h"

namespace esphome {
namespace toshiba {

const uint16_t TOSHIBA_HEADER_MARK = 4380;
const uint16_t TOSHIBA_HEADER_SPACE = 4370;
const uint16_t TOSHIBA_GAP_SPACE = 5480;
const uint16_t TOSHIBA_BIT_MARK = 540;
const uint16_t TOSHIBA_ZERO_SPACE = 540;
const uint16_t TOSHIBA_ONE_SPACE = 1620;

const uint8_t TOSHIBA_COMMAND_DEFAULT = 0x01;
const uint8_t TOSHIBA_COMMAND_TIMER = 0x02;
const uint8_t TOSHIBA_COMMAND_POWER = 0x08;
const uint8_t TOSHIBA_COMMAND_MOTION = 0x02;

const uint8_t TOSHIBA_MODE_AUTO = 0x00;
const uint8_t TOSHIBA_MODE_COOL = 0x01;
const uint8_t TOSHIBA_MODE_DRY = 0x02;
const uint8_t TOSHIBA_MODE_HEAT = 0x03;
const uint8_t TOSHIBA_MODE_FAN_ONLY = 0x04;
const uint8_t TOSHIBA_MODE_OFF = 0x07;

const uint8_t TOSHIBA_FAN_SPEED_AUTO = 0x00;
const uint8_t TOSHIBA_FAN_SPEED_QUIET = 0x20;
const uint8_t TOSHIBA_FAN_SPEED_1 = 0x40;
const uint8_t TOSHIBA_FAN_SPEED_2 = 0x60;
const uint8_t TOSHIBA_FAN_SPEED_3 = 0x80;
const uint8_t TOSHIBA_FAN_SPEED_4 = 0xa0;
const uint8_t TOSHIBA_FAN_SPEED_5 = 0xc0;

const uint8_t TOSHIBA_POWER_HIGH = 0x01;
const uint8_t TOSHIBA_POWER_ECO = 0x03;

const uint8_t TOSHIBA_MOTION_SWING = 0x04;
const uint8_t TOSHIBA_MOTION_FIX = 0x00;

static const char *TAG = "toshiba.climate";

void ToshibaClimate::transmit_state() {
  uint8_t message[16] = {0};
  uint8_t message_length = 9;

  /* Header */
  message[0] = 0xf2;
  message[1] = 0x0d;

  /* Message length */
  message[2] = message_length - 6;

  /* First checksum */
  message[3] = message[0] ^ message[1] ^ message[2];

  /* Command */
  message[4] = TOSHIBA_COMMAND_DEFAULT;

  /* Temperature */
  uint8_t temperature = static_cast<uint8_t>(this->target_temperature);
  if (temperature < 17) {
    temperature = 17;
  }
  if (temperature > 30) {
    temperature = 30;
  }
  message[5] = (temperature - 17) << 4;

  /* Mode and fan */
  uint8_t mode;
  switch (this->mode) {
    case climate::CLIMATE_MODE_OFF:
      mode = TOSHIBA_MODE_OFF;
      break;

    case climate::CLIMATE_MODE_HEAT:
      mode = TOSHIBA_MODE_HEAT;
      break;

    case climate::CLIMATE_MODE_COOL:
      mode = TOSHIBA_MODE_COOL;
      break;

    case climate::CLIMATE_MODE_AUTO:
    default:
      mode = TOSHIBA_MODE_AUTO;
  }

  message[6] |= mode | TOSHIBA_FAN_SPEED_AUTO;

  /* Zero */
  message[7] = 0x00;

  /* If timers bit in the command is set, two extra bytes are added here */

  /* If power bit is set in the command, one extra byte is added here */

  /* The last byte is the xor of all bytes from [4] */
  for (uint8_t i = 4; i < 8; i++) {
    message[8] ^= message[i];
  }

  /* Transmit */
  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();
  data->set_carrier_frequency(38000);

  for (uint8_t copy = 0; copy < 2; copy++) {
    data->mark(TOSHIBA_HEADER_MARK);
    data->space(TOSHIBA_HEADER_SPACE);

    for (uint8_t byte = 0; byte < message_length; byte++) {
      for (uint8_t bit = 0; bit < 8; bit++) {
        data->mark(TOSHIBA_BIT_MARK);
        if (message[byte] & (1 << (7 - bit))) {
          data->space(TOSHIBA_ONE_SPACE);
        } else {
          data->space(TOSHIBA_ZERO_SPACE);
        }
      }
    }

    data->mark(TOSHIBA_BIT_MARK);
    data->space(TOSHIBA_GAP_SPACE);
  }

  transmit.perform();
}

bool ToshibaClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t message[16] = {0};
  uint8_t message_length = 4;

  /* Validate header */
  if (!data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE)) {
    return false;
  }

  /* Decode bytes */
  for (uint8_t byte = 0; byte < message_length; byte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(TOSHIBA_BIT_MARK, TOSHIBA_ONE_SPACE)) {
        message[byte] |= 1 << (7 - bit);
      } else if (data.expect_item(TOSHIBA_BIT_MARK, TOSHIBA_ZERO_SPACE)) {
        /* Bit is already clear */
      } else {
        return false;
      }
    }

    /* Update length */
    if (byte == 3) {
      /* Validate the first checksum before trusting the length field */
      if ((message[0] ^ message[1] ^ message[2]) != message[3]) {
        return false;
      }
      message_length = message[2] + 6;
    }
  }

  /* Validate the second checksum before trusting any more of the message */
  uint8_t checksum = 0;
  for (uint8_t i = 4; i < message_length - 1; i++) {
    checksum ^= message[i];
  }

  if (checksum != message[message_length - 1]) {
    return false;
  }

  /* Check if this is a short swing/fix message */
  if (message[4] & TOSHIBA_COMMAND_MOTION) {
    /* Not supported yet */
    return false;
  }

  /* Get the mode. */
  switch (message[6] & 0x0f) {
    case TOSHIBA_MODE_OFF:
      this->mode = climate::CLIMATE_MODE_OFF;
      break;

    case TOSHIBA_MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      break;

    case TOSHIBA_MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;

    case TOSHIBA_MODE_AUTO:
    default:
      /* Note: Dry and Fan-only modes are reported as Auto, as they are not supported yet */
      this->mode = climate::CLIMATE_MODE_AUTO;
  }

  /* Get the target temperature */
  this->target_temperature = (message[5] >> 4) + 17;

  this->publish_state();
  return true;
}

} /* namespace toshiba */
} /* namespace esphome */
