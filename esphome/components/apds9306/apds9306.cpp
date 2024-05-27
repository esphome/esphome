#include "apds9306.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace apds9306 {

static const char* const TAG = "apds9306";

enum { // APDS9306 registers
  APDS9306_MAIN_CTRL = 0x00,
  APDS9306_ALS_MEAS_RATE = 0x04,
  APDS9306_ALS_GAIN = 0x05,
  APDS9306_PART_ID = 0x06,
  APDS9306_MAIN_STATUS = 0x07,
  APDS9306_CLEAR_DATA_0 = 0x0A, // LSB
  APDS9306_CLEAR_DATA_1 = 0x0B,
  APDS9306_CLEAR_DATA_2 = 0x0C, // MSB
  APDS9306_ALS_DATA_0 = 0x0D,   // LSB
  APDS9306_ALS_DATA_1 = 0x0E,
  APDS9306_ALS_DATA_2 = 0x0F,   // MSB
  APDS9306_INT_CFG = 0x19,
  APDS9306_INT_PERSISTENCE = 0x1A,
  APDS9306_ALS_THRES_UP_0 = 0x21,   // LSB
  APDS9306_ALS_THRES_UP_1 = 0x22,
  APDS9306_ALS_THRES_UP_2 = 0x23,   // MSB
  APDS9306_ALS_THRES_LOW_0 = 0x24,  // LSB
  APDS9306_ALS_THRES_LOW_1 = 0x25,
  APDS9306_ALS_THRES_LOW_2 = 0x26,  // MSB
  APDS9306_ALS_THRES_VAR = 0x27,
};

#define APDS9306_ERROR_CHECK(func, error) \
  if (!(func)) { \
    ESP_LOGE(TAG, error); \
    this->mark_failed(); \
    return; \
  }
#define APDS9306_WARNING_CHECK(func, warning) \
  if (!(func)) { \
    ESP_LOGW(TAG, warning); \
    this->status_set_warning(); \
    return; \
  }
#define APDS9306_WRITE_BYTE(reg, value) \
  ESP_LOGV(TAG, "Writing %x to %x", value, reg); \
  if (!this->write_byte(reg, value)) { \
    ESP_LOGE(TAG, "Failed writing 0x%x to 0x%x", value, reg); \
    this->mark_failed(); \
    return; \
  }

void APDS9306::setup() {
  ESP_LOGCONFIG(TAG, "Setting up APDS9306...");

  uint8_t id;
  if (!this->read_byte(APDS9306_PART_ID, &id)) {  // Part ID register
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (id != 0xB1 && id != 0xB3) {  // 0xB1 for APDS9306 0xB3 for APDS9306-065
    this->error_code_ = WRONG_ID;
    this->mark_failed();
    return;
  }

  // Trigger software reset and put in standby mode
  APDS9306_WRITE_BYTE(APDS9306_MAIN_CTRL, 0x10);

  // ALS resolution and measurement, see datasheet of init.py for options
  uint8_t als_meas_rate = ((this->bit_width_ & 0x07) << 4) | (this->measurement_rate_ & 0x07);

  APDS9306_WRITE_BYTE(APDS9306_ALS_MEAS_RATE, als_meas_rate);

  // ALS gain, see datasheet or init.py for options
  uint8_t als_gain = (this->gain_ & 0x07);

  APDS9306_WRITE_BYTE(APDS9306_ALS_GAIN, als_gain);

  // Set to active mode
  APDS9306_WRITE_BYTE(APDS9306_MAIN_CTRL, 0x02);
}

void APDS9306::dump_config() {
  LOG_SENSOR("", "apds9306", this);
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGE(TAG, "Communication with APDS9306 failed!");
        break;
      case WRONG_ID:
        ESP_LOGE(TAG, "APDS9306 has invalid id!");
        break;
      default:
        ESP_LOGE(TAG, "Setting up APDS9306 registers failed!");
        break;
    }
  }

  ESP_LOGCONFIG(TAG, "  Gain: %d", gain_val_);
  ESP_LOGCONFIG(TAG, "  Measurement rate: %dms", rate_val_);
  ESP_LOGCONFIG(TAG, "  Measurement Resolution/Bit width: %d", bit_width_val_);

  LOG_UPDATE_INTERVAL(this);
}

void APDS9306::update() {
  uint8_t status;
  APDS9306_WARNING_CHECK(this->read_byte(APDS9306_MAIN_STATUS, &status), "Reading status bit failed.");

  this->status_clear_warning();

  if (!(status &= 0b00001000))  // No new data
    return;

  // Conversions
  switch (this->gain_) {
    case 0:
      this->gain_val_ = 1;
      break;
    case 1:
      this->gain_val_ = 3;
      break;
    case 2:
      this->gain_val_ = 6;
      break;
    case 3:
      this->gain_val_ = 9;
      break;
    case 4:
      this->gain_val_ = 18;
      break;
  }

  switch (this->measurement_rate_) {
    case 0:
      this->rate_val_ = 25;
      break;
    case 1:
      this->rate_val_ = 50;
      break;
    case 2:
      this->rate_val_ = 100;
      break;
    case 3:
      this->rate_val_ = 200;
      break;
    case 4:
      this->rate_val_ = 500;
      break;
    case 5:
      this->rate_val_ = 1000;
      break;
  }

  switch (this->bit_width_) {
    case 0:
      this->bit_width_val_ = 20;
      break;
    case 1:
      this->bit_width_val_ = 19;
      break;
    case 2:
      this->bit_width_val_ = 18;
      break;
    case 3:
      this->bit_width_val_ = 17;
      break;
    case 4:
      this->bit_width_val_ = 16;
      break;
    case 5:
      this->bit_width_val_ = 13;
      break;
  }

  uint8_t als_data[3];
  APDS9306_WARNING_CHECK(this->read_byte(APDS9306_ALS_DATA_0, als_data, 3), "Reading ALS data has failed.");

  uint32_t light_level = encode_uint24(als_data[2], als_data[1], als_data[0]);

  float lux = ((float) light_level / this->gain_val_) * (100.0f / this->rate_val_);

  ESP_LOGD(TAG, "Got illuminance=%.1flx", lux);
  this->publish_state(lux);
}

}  // namespace apds9306
}  // namespace esphome
