#include "daikin_arc.h"

#include <cmath>

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daikin_arc {

static const char *const TAG = "daikin.climate";

void DaikinArcClimate::setup() {
  climate_ir::ClimateIR::setup();

  // Never send nan to HA
  if (std::isnan(this->target_humidity))
    this->target_humidity = 0;
  if (std::isnan(this->current_temperature))
    this->current_temperature = 0;
  if (std::isnan(this->current_humidity))
    this->current_humidity = 0;
}

void DaikinArcClimate::transmit_query_() {
  uint8_t remote_header[8] = {0x11, 0xDA, 0x27, 0x00, 0x84, 0x87, 0x20, 0x00};

  // Calculate checksum
  for (int i = 0; i < sizeof(remote_header) - 1; i++) {
    remote_header[sizeof(remote_header) - 1] += remote_header[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_IR_FREQUENCY);

  data->mark(DAIKIN_ARC_PRE_MARK);
  data->space(DAIKIN_ARC_PRE_SPACE);

  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);

  for (uint8_t i : remote_header) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      bool bit = i & mask;
      data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(0);

  transmit.perform();
}

void DaikinArcClimate::transmit_state() {
  // 0x11, 0xDA, 0x27, 0x00, 0xC5, 0x00, 0x00, 0xD7, 0x11, 0xDA, 0x27, 0x00,
  // 0x42, 0x49, 0x05, 0xA2,
  uint8_t remote_header[20] = {0x11, 0xDA, 0x27, 0x00, 0x02, 0xd0, 0x02, 0x03, 0x80, 0x03, 0x82, 0x30, 0x41, 0x1f, 0x82,
                               0xf4,
                               /*                                                      とつど */
                               /*                                                       0x13 */
                               0x00, 0x24, 0x00, 0x00};

  // 05    0 [1:3]MODE   1 [OFF TMR] [ON TMR] Power
  // 06-07 TEMP
  // 08    [0:3] SPEED  [4:7] Swing
  // 09    00
  // 10    00
  // 11, 12: timer
  // 13    [0:6] 0000000 [7] POWERMODE
  // 14    0a
  // 15    c4
  // 16    [0:3] 8  00 [6:7] SENSOR WIND = 11 / NORMAL = 00
  // 17    24

  uint8_t remote_state[19] = {
      0x11, 0xDA, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x60, 0x00, 0x0a, 0xC4,
      /*                               MODE  TEMP  HUMD  FANH  FANL
         パワフル音声応答     */
      /*                                                                           ON
         0x01入 0x0a     */
      /*                                                                           OF
         0x00切 0x02     */
      0x80, 0x24, 0x00
      /* センサー風         */
      /* ON 0x83           */
      /* OF 0x80           */
  };

  remote_state[5] = this->operation_mode_() | 0x08;
  remote_state[6] = this->temperature_();
  remote_state[7] = this->humidity_();
  static uint8_t last_humidity = 0x66;
  if (remote_state[7] != last_humidity && this->mode != climate::CLIMATE_MODE_OFF) {
    ESP_LOGD(TAG, "Set Humditiy: %d, %d\n", (int) this->target_humidity, (int) remote_state[7]);
    remote_header[9] |= 0x10;
    last_humidity = remote_state[7];
  }
  uint16_t fan_speed = this->fan_speed_();
  remote_state[8] = fan_speed >> 8;
  remote_state[9] = fan_speed & 0xff;

  // Calculate checksum
  for (int i = 0; i < sizeof(remote_header) - 1; i++) {
    remote_header[sizeof(remote_header) - 1] += remote_header[i];
  }

  // Calculate checksum
  for (int i = 0; i < DAIKIN_STATE_FRAME_SIZE - 1; i++) {
    remote_state[DAIKIN_STATE_FRAME_SIZE - 1] += remote_state[i];
  }

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_IR_FREQUENCY);

  data->mark(DAIKIN_ARC_PRE_MARK);
  data->space(DAIKIN_ARC_PRE_SPACE);

  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);

  for (uint8_t i : remote_header) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      bool bit = i & mask;
      data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(DAIKIN_MESSAGE_SPACE);

  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);

  for (uint8_t i : remote_state) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(DAIKIN_BIT_MARK);
      bool bit = i & mask;
      data->space(bit ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE);
    }
  }
  data->mark(DAIKIN_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t DaikinArcClimate::operation_mode_() {
  uint8_t operating_mode = DAIKIN_MODE_ON;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= DAIKIN_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= DAIKIN_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= DAIKIN_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= DAIKIN_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= DAIKIN_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DAIKIN_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint16_t DaikinArcClimate::fan_speed_() {
  uint16_t fan_speed;
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DAIKIN_FAN_1 << 8;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DAIKIN_FAN_3 << 8;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DAIKIN_FAN_5 << 8;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = DAIKIN_FAN_AUTO << 8;
  }

  // If swing is enabled switch first 4 bits to 1111
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      fan_speed |= 0x0F00;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      fan_speed |= 0x000F;
      break;
    case climate::CLIMATE_SWING_BOTH:
      fan_speed |= 0x0F0F;
      break;
    default:
      break;
  }
  return fan_speed;
}

uint8_t DaikinArcClimate::temperature_() {
  // Force special temperatures depending on the mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 0x32;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_DRY:
      return 0xc0;
    default:
      float new_temp = clamp<float>(this->target_temperature, DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX);
      uint8_t temperature = (uint8_t) floor(new_temp);
      return temperature << 1 | (new_temp - temperature > 0 ? 0x01 : 0);
  }
}

uint8_t DaikinArcClimate::humidity_() {
  if (this->target_humidity == 39) {
    return 0;
  } else if (this->target_humidity <= 40 || this->target_humidity == 44) {
    return 40;
  } else if (this->target_humidity <= 45 || this->target_humidity == 49)  // 41 - 45
  {
    return 45;
  } else if (this->target_humidity <= 50 || this->target_humidity == 52)  // 45 - 50
  {
    return 50;
  } else {
    return 0xff;
  }
}

climate::ClimateTraits DaikinArcClimate::traits() {
  climate::ClimateTraits traits = climate_ir::ClimateIR::traits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_current_humidity(false);
  traits.set_supports_target_humidity(true);
  traits.set_visual_min_humidity(38);
  traits.set_visual_max_humidity(52);
  return traits;
}

bool DaikinArcClimate::parse_state_frame_(const uint8_t frame[]) {
  uint8_t checksum = 0;
  for (int i = 0; i < (DAIKIN_STATE_FRAME_SIZE - 1); i++) {
    checksum += frame[i];
  }
  if (frame[DAIKIN_STATE_FRAME_SIZE - 1] != checksum) {
    ESP_LOGI(TAG, "checksum error");
    return false;
  }

  char buf[DAIKIN_STATE_FRAME_SIZE * 3 + 1] = {0};
  for (size_t i = 0; i < DAIKIN_STATE_FRAME_SIZE; i++) {
    sprintf(buf, "%s%02x ", buf, frame[i]);
  }
  ESP_LOGD(TAG, "FRAME %s", buf);

  uint8_t mode = frame[5];
  if (mode & DAIKIN_MODE_ON) {
    switch (mode & 0xF0) {
      case DAIKIN_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case DAIKIN_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DAIKIN_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case DAIKIN_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case DAIKIN_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  uint8_t temperature = frame[6];
  if (!(temperature & 0xC0)) {
    this->target_temperature = temperature >> 1;
    this->target_temperature += (temperature & 0x1) ? 0.5 : 0;
  }
  this->target_humidity = frame[7];  // 0, 40, 45, 50, 0xff
  uint8_t fan_mode = frame[8];
  uint8_t swing_mode = frame[9];
  if (fan_mode & 0xF && swing_mode & 0xF) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (fan_mode & 0xF) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (swing_mode & 0xF) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
  switch (fan_mode & 0xF0) {
    case DAIKIN_FAN_1:
    case DAIKIN_FAN_2:
    case DAIKIN_FAN_SILENT:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DAIKIN_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DAIKIN_FAN_4:
    case DAIKIN_FAN_5:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case DAIKIN_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }
  /*
  05    0 [1:3]MODE   1 [OFF TMR] [ON TMR] Power
  06-07 TEMP
  08    [0:3] SPEED  [4:7] Swing
  09    00
  10    00
  11, 12: timer
  13    [0:6] 0000000 [7] POWERMODE
  14    0a
  15    c4
  16    [0:3] 8  00 [6:7] SENSOR WIND = 11 / NORMAL = 00
  17    24
                             05 06 07 08 09 10 11 12 13 14 15 16 17 18
  None  FRAME 11 da 27 00 00 49 2e 00 b0 00 00 06 60 00 0a c4 80 24 11
  1H    FRAME 11 da 27 00 00 4d 2e 00 b0 00 00 c6 30 00 2a c4 80 24 c5
  1H30  FRAME 11 da 27 00 00 4d 2e 00 b0 00 00 a6 32 00 2a c4 80 24 a7
  2H    FRAME 11 da 27 00 00 4d 2e 00 b0 00 00 86 34 00 2a c4 80 24 89

  */
  this->publish_state();
  return true;
}

bool DaikinArcClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t state_frame[DAIKIN_STATE_FRAME_SIZE] = {};

  bool valid_daikin_frame = false;
  if (data.expect_item(DAIKIN_HEADER_MARK, DAIKIN_HEADER_SPACE)) {
    valid_daikin_frame = true;
    int bytes_count = data.size() / 2 / 8;
    std::unique_ptr<char[]> buf(new char[bytes_count * 3 + 1]);
    buf[0] = '\0';
    for (size_t i = 0; i < bytes_count; i++) {
      uint8_t byte = 0;
      for (int8_t bit = 0; bit < 8; bit++) {
        if (data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ONE_SPACE)) {
          byte |= 1 << bit;
        } else if (!data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ZERO_SPACE)) {
          valid_daikin_frame = false;
          break;
        }
      }
      sprintf(buf.get(), "%s%02x ", buf.get(), byte);
    }
    ESP_LOGD(TAG, "WHOLE FRAME %s  size: %d", buf.get(), data.size());
  }
  if (!valid_daikin_frame) {
    char sbuf[16 * 10 + 1];
    sbuf[0] = '\0';
    for (size_t j = 0; j < data.size(); j++) {
      if ((j - 2) % 16 == 0) {
        if (j > 0) {
          ESP_LOGD(TAG, "DATA %04x: %s", (j - 16 > 0xffff ? 0 : j - 16), sbuf);
        }
        sbuf[0] = '\0';
      }
      char type_ch = ' ';
      // debug_tolerance = 25%

      if (DAIKIN_DBG_LOWER(DAIKIN_ARC_PRE_MARK) <= data[j] && data[j] <= DAIKIN_DBG_UPPER(DAIKIN_ARC_PRE_MARK))
        type_ch = 'P';
      if (DAIKIN_DBG_LOWER(DAIKIN_ARC_PRE_SPACE) <= -data[j] && -data[j] <= DAIKIN_DBG_UPPER(DAIKIN_ARC_PRE_SPACE))
        type_ch = 'a';
      if (DAIKIN_DBG_LOWER(DAIKIN_HEADER_MARK) <= data[j] && data[j] <= DAIKIN_DBG_UPPER(DAIKIN_HEADER_MARK))
        type_ch = 'H';
      if (DAIKIN_DBG_LOWER(DAIKIN_HEADER_SPACE) <= -data[j] && -data[j] <= DAIKIN_DBG_UPPER(DAIKIN_HEADER_SPACE))
        type_ch = 'h';
      if (DAIKIN_DBG_LOWER(DAIKIN_BIT_MARK) <= data[j] && data[j] <= DAIKIN_DBG_UPPER(DAIKIN_BIT_MARK))
        type_ch = 'B';
      if (DAIKIN_DBG_LOWER(DAIKIN_ONE_SPACE) <= -data[j] && -data[j] <= DAIKIN_DBG_UPPER(DAIKIN_ONE_SPACE))
        type_ch = '1';
      if (DAIKIN_DBG_LOWER(DAIKIN_ZERO_SPACE) <= -data[j] && -data[j] <= DAIKIN_DBG_UPPER(DAIKIN_ZERO_SPACE))
        type_ch = '0';

      if (abs(data[j]) > 100000) {
        sprintf(sbuf, "%s%-5d[%c] ", sbuf, data[j] > 0 ? 99999 : -99999, type_ch);
      } else {
        sprintf(sbuf, "%s%-5d[%c] ", sbuf, (int) (round(data[j] / 10.) * 10), type_ch);
      }
      if (j == data.size() - 1) {
        ESP_LOGD(TAG, "DATA %04x: %s", (j - 8 > 0xffff ? 0 : j - 8), sbuf);
      }
    }
  }

  data.reset();

  if (!data.expect_item(DAIKIN_HEADER_MARK, DAIKIN_HEADER_SPACE)) {
    ESP_LOGI(TAG, "non daikin_arc expect item");
    return false;
  }

  for (uint8_t pos = 0; pos < DAIKIN_STATE_FRAME_SIZE; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(DAIKIN_BIT_MARK, DAIKIN_ZERO_SPACE)) {
        ESP_LOGI(TAG, "non daikin_arc expect item pos: %d", pos);
        return false;
      }
    }
    state_frame[pos] = byte;
    if (pos == 0) {
      // frame header
      if (byte != 0x11) {
        ESP_LOGI(TAG, "non daikin_arc expect pos: %d header: %02x", pos, byte);
        return false;
      }
    } else if (pos == 1) {
      // frame header
      if (byte != 0xDA) {
        ESP_LOGI(TAG, "non daikin_arc expect pos: %d header: %02x", pos, byte);
        return false;
      }
    } else if (pos == 2) {
      // frame header
      if (byte != 0x27) {
        ESP_LOGI(TAG, "non daikin_arc expect pos: %d header: %02x", pos, byte);
        return false;
      }
    } else if (pos == 3) {  // NOLINT(bugprone-branch-clone)
      // frame header
      if (byte != 0x00) {
        ESP_LOGI(TAG, "non daikin_arc expect pos: %d header: %02x", pos, byte);
        return false;
      }
    } else if (pos == 4) {
      // frame type
      if (byte != 0x00) {
        ESP_LOGI(TAG, "non daikin_arc expect pos: %d header: %02x", pos, byte);
        return false;
      }
    } else if (pos == 5) {
      if (data.size() == 385) {
        /*
        11 da 27 00 00 1a 0c 04 2c 21 61 07 00 07 0c 00 18 00 0e 3c 00 6c 1b 61
                       Inside Temp
                          Outside Temp
                              Humdidity

        */
        this->current_temperature = state_frame[5];  // Inside temperature
        // this->current_temperature = state_frame[6]; // Outside temperature
        this->publish_state();
        return true;
      } else if ((byte & 0x40) != 0x40) {
        ESP_LOGI(TAG, "non daikin_arc expect pos: %d header: %02x", pos, byte);
        return false;
      }
    }
  }
  return this->parse_state_frame_(state_frame);
}

void DaikinArcClimate::control(const climate::ClimateCall &call) {
  if (call.get_target_humidity().has_value()) {
    this->target_humidity = *call.get_target_humidity();
  }
  climate_ir::ClimateIR::control(call);
}

}  // namespace daikin_arc
}  // namespace esphome
