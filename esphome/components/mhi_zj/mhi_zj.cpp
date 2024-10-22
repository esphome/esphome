#include "mhi_zj.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mhi_zj {

static const char *const TAG = "mhi_zj.climate";

// Power
const uint32_t MHI_OFF = 0x08;
const uint32_t MHI_ON = 0x00;

// Operating mode
const uint8_t MHI_AUTO = 0x07;
const uint8_t MHI_HEAT = 0x03;
const uint8_t MHI_COOL = 0x06;
const uint8_t MHI_DRY = 0x05;
const uint8_t MHI_FAN = 0x04;

// Fan speed
const uint8_t MHI_FAN_AUTO = 0xE0;
const uint8_t MHI_FAN1 = 0xA0;
const uint8_t MHI_FAN2 = 0x80;
const uint8_t MHI_FAN3 = 0x60;
const uint8_t MHI_HIPOWER = 0x40;
const uint8_t MHI_ECONO = 0x00;

// Vertical swing
const uint8_t MHI_VS_SWING = 0x0A;
const uint8_t MHI_VS_UP = 0x02;
const uint8_t MHI_VS_MUP = 0x18;
const uint8_t MHI_VS_MIDDLE = 0x10;
const uint8_t MHI_VS_MDOWN = 0x08;
const uint8_t MHI_VS_DOWN = 0x00;
const uint8_t MHI_VS_STOP = 0x1A;

// Horizontal swing
const uint8_t MHI_HS_SWING = 0x4C;
const uint8_t MHI_HS_MIDDLE = 0x48;
const uint8_t MHI_HS_LEFT = 0xC8;
const uint8_t MHI_HS_MLEFT = 0x88;
const uint8_t MHI_HS_MRIGHT = 0x08;
const uint8_t MHI_HS_RIGHT = 0xC4;
const uint8_t MHI_HS_STOP = 0xCC;
const uint8_t MHI_HS_LEFTRIGHT = 0x84;
const uint8_t MHI_HS_RIGHTLEFT = 0x44;
const uint8_t MHI_HS_3DAUTO = 0x04;

// // Only available in Auto, Cool and Heat mode
const uint8_t MHI_3DAUTO_ON = 0x00;
const uint8_t MHI_3DAUTO_OFF = 0x12;

// // NOT available in Fan or Dry mode
const uint8_t MHI_SILENT_ON = 0x00;
const uint8_t MHI_SILENT_OFF = 0x80;

// Pulse parameters in usec
const uint16_t MHI_BIT_MARK = 400;
const uint16_t MHI_ONE_SPACE = 1200;
const uint16_t MHI_ZERO_SPACE = 400;
const uint16_t MHI_HEADER_MARK = 3200;
const uint16_t MHI_HEADER_SPACE = 1600;
const uint16_t MHI_MIN_GAP = 17500;

bool MhiClimate::on_receive(remote_base::RemoteReceiveData data) {
  ESP_LOGD(TAG, "Received some bytes");

  // The protocol sends the data twice, read here
  // uint32_t loop_read;

  uint8_t bytes[11] = {};

  // for (uint16_t loop = 1; loop <= 2; loop++) {
  if (!data.expect_item(MHI_HEADER_MARK, MHI_HEADER_SPACE))
    return false;

  // loop_read = 0;
  for (unsigned char &a_byte : bytes) {
    uint8_t byte = 0;
    for (int8_t a_bit = 0; a_bit < 8; a_bit++) {
      if (data.expect_item(MHI_BIT_MARK, MHI_ONE_SPACE)) {
        byte |= 1 << a_bit;
      } else if (!data.expect_item(MHI_BIT_MARK, MHI_ZERO_SPACE)) {
        return false;
      }
    }
    a_byte = byte;
  }

  ESP_LOGD(TAG, "Received bytes 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9],
           bytes[10]);

  // Check the static bytes
  if (bytes[0] != 0x52 || bytes[1] != 0xAE || bytes[2] != 0xC3 || bytes[3] != 0x26 || bytes[4] != 0xD9) {
    return false;
  }

  ESP_LOGD(TAG, "Passed check 1");

  // Check the inversed bytes
  if (bytes[5] != (~bytes[6] & 0xFF) || bytes[7] != (~bytes[8] & 0xFF) || bytes[9] != (~bytes[10] & 0xFF)) {
    return false;
  }

  ESP_LOGD(TAG, "Passed check 2");

  auto power_mode = bytes[9] & 0x08;
  auto operation_mode = bytes[9] & 0x07;
  auto temperature = ((~bytes[9] & 0xF0) >> 4) + 17;
  auto fan_speed = bytes[7] & 0xE0;
  auto swing_v = ((bytes[5] & 0x02) | (bytes[7] & 0x18));
  auto swing_h = (bytes[5] & 0xCC);

  ESP_LOGD(TAG,
           "Resulting numbers: powerMode=0x%02X operationMode=0x%02X temperature=%d fanSpeed=0x%02X swingV=0x%02X "
           "swingH=0x%02X",
           power_mode, operation_mode, temperature, fan_speed, swing_v, swing_h);

  if (power_mode == MHI_ON) {
    // Power and operating mode
    switch (operation_mode) {
      case MHI_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        // swingV = MHI_VS_UP;
        break;
      case MHI_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        // swingV = MHI_VS_DOWN;
        break;
      case MHI_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case MHI_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      default:
      case MHI_AUTO:
        this->mode = climate::CLIMATE_MODE_AUTO;
        // swingV = MHI_VS_MIDDLE;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  // Temperature
  this->target_temperature = temperature;

  // Horizontal and vertical swing
  if (swing_v == MHI_VS_SWING && swing_h == MHI_HS_SWING) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (swing_v == MHI_VS_SWING) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (swing_h == MHI_HS_SWING) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  // Fan speed
  switch (fan_speed) {
    case MHI_FAN1:  // Only to support remote feedback
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case MHI_FAN2:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case MHI_FAN3:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case MHI_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      switch (swing_h) {
        case MHI_HS_MIDDLE:
          this->fan_mode = climate::CLIMATE_FAN_MIDDLE;
          break;
        case MHI_HS_RIGHTLEFT:
          this->fan_mode = climate::CLIMATE_FAN_FOCUS;
          break;
        case MHI_HS_LEFTRIGHT:
          this->fan_mode = climate::CLIMATE_FAN_DIFFUSE;
          break;
      }
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  ESP_LOGD(TAG, "Finish it");

  this->publish_state();
  return true;
}

void MhiClimate::transmit_state() {
  uint8_t remote_state[] = {0x52, 0xAE, 0xC3, 0x26, 0xD9, 0x11, 0x00, 0x07, 0x00, 0x00, 0x00};

  // ----------------------
  // Initial values
  // ----------------------

  auto operating_mode = MHI_AUTO;
  auto power_mode = MHI_ON;
  auto clean_mode = 0x20;  // always off

  auto temperature = 22;
  auto fan_speed = MHI_FAN_AUTO;
  auto swing_v = MHI_VS_STOP;
  auto swing_h = MHI_HS_STOP;

  // ----------------------
  // Assign the values
  // ----------------------

  // Power and operating mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode = MHI_COOL;
      swing_v = MHI_VS_UP;  // custom preferred value for this mode
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode = MHI_COOL;
      swing_v = MHI_VS_MIDDLE;  // custom preferred value for this mode
      break;
    case climate::CLIMATE_MODE_AUTO:
      operating_mode = MHI_COOL;
      swing_v = MHI_VS_MIDDLE;  // custom preferred value for this mode
      swing_h = MHI_HS_3DAUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode = MHI_FAN;
      swing_v = MHI_VS_MIDDLE;  // custom preferred value for this mode
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode = MHI_DRY;
      swing_v = MHI_VS_MIDDLE;  // custom preferred value for this mode
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      power_mode = MHI_OFF;
      break;
  }

  // Temperature
  if (this->target_temperature > 17 && this->target_temperature < 31)
    temperature = this->target_temperature;

  // Horizontal and vertical swing
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_BOTH:
      swing_v = MHI_VS_SWING;
      swing_h = MHI_HS_SWING;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      swing_h = MHI_HS_SWING;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      swing_v = MHI_VS_SWING;
      break;
    case climate::CLIMATE_SWING_OFF:
    default:
      // Already on STOP
      break;
  }

  // Fan speed
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = MHI_FAN1;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = MHI_FAN2;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = MHI_FAN3;
      break;
    case climate::CLIMATE_FAN_MIDDLE:
      fan_speed = MHI_FAN_AUTO;
      swing_h = MHI_HS_MIDDLE;
      swing_v = MHI_VS_SWING;
      break;
    case climate::CLIMATE_FAN_FOCUS:
      fan_speed = MHI_HIPOWER;
      swing_h = MHI_HS_RIGHTLEFT;
      swing_v = MHI_VS_SWING;
      break;
    case climate::CLIMATE_FAN_DIFFUSE:
      fan_speed = MHI_HIPOWER;
      swing_h = MHI_HS_LEFTRIGHT;
      swing_v = MHI_VS_SWING;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = MHI_FAN_AUTO;
      break;
  }

  // ----------------------
  // Assign the bytes
  // ----------------------

  // Power state + operating mode
  remote_state[5] |= swing_h | (swing_v & 0x02) | clean_mode;

  // Temperature
  remote_state[7] |= fan_speed | (swing_v & 0x18);

  // Fan speed
  remote_state[9] |= operating_mode | power_mode | ((~(((uint8_t) temperature - 17) << 4)) & 0xF0);

  // There is no real checksum, but some bytes are inverted
  remote_state[6] = ~remote_state[5];
  remote_state[8] = ~remote_state[7];
  remote_state[10] = ~remote_state[9];

  // ESP_LOGD(TAG, "Sending MHI target temp: %.1f state: %02X mode: %02X temp: %02X", this->target_temperature,
  // remote_state[5], remote_state[6], remote_state[7]);

  auto *bytes = remote_state;
  ESP_LOGD(TAG, "Sent bytes 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9],
           bytes[10]);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(MHI_HEADER_MARK);
  data->space(MHI_HEADER_SPACE);

  // Data
  for (uint8_t i : remote_state) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(MHI_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? MHI_ONE_SPACE : MHI_ZERO_SPACE);
    }
  }
  data->mark(MHI_BIT_MARK);
  data->space(0);

  transmit.perform();
}
}  // namespace mhi_zj
}  // namespace esphome
