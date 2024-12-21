// Based on this datasheet:
// https://www.mouser.ca/datasheet/2/678/AVGO_S_A0002854364_1-2574547.pdf

#include "apds9306.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace apds9306 {

static const char *const TAG = "apds9306";

enum {  // APDS9306 registers
  APDS9306_MAIN_CTRL = 0x00,
  APDS9306_ALS_MEAS_RATE = 0x04,
  APDS9306_ALS_GAIN = 0x05,
  APDS9306_PART_ID = 0x06,
  APDS9306_MAIN_STATUS = 0x07,
  APDS9306_CLEAR_DATA_0 = 0x0A,  // LSB
  APDS9306_CLEAR_DATA_1 = 0x0B,
  APDS9306_CLEAR_DATA_2 = 0x0C,  // MSB
  APDS9306_ALS_DATA_0 = 0x0D,    // LSB
  APDS9306_ALS_DATA_1 = 0x0E,
  APDS9306_ALS_DATA_2 = 0x0F,  // MSB
  APDS9306_INT_CFG = 0x19,
  APDS9306_INT_PERSISTENCE = 0x1A,
  APDS9306_ALS_THRES_UP_0 = 0x21,  // LSB
  APDS9306_ALS_THRES_UP_1 = 0x22,
  APDS9306_ALS_THRES_UP_2 = 0x23,   // MSB
  APDS9306_ALS_THRES_LOW_0 = 0x24,  // LSB
  APDS9306_ALS_THRES_LOW_1 = 0x25,
  APDS9306_ALS_THRES_LOW_2 = 0x26,  // MSB
  APDS9306_ALS_THRES_VAR = 0x27
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
  ESP_LOGV(TAG, "Writing 0x%02x to 0x%02x", value, reg); \
  if (!this->write_byte(reg, value)) { \
    ESP_LOGE(TAG, "Failed writing 0x%02x to 0x%02x", value, reg); \
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

  // ALS resolution and measurement, see datasheet or init.py for options
  uint8_t als_meas_rate = ((this->bit_width_ & 0x07) << 4) | (this->measurement_rate_ & 0x07);
  APDS9306_WRITE_BYTE(APDS9306_ALS_MEAS_RATE, als_meas_rate);

  // ALS gain, see datasheet or init.py for options
  uint8_t als_gain = (this->gain_ & 0x07);
  APDS9306_WRITE_BYTE(APDS9306_ALS_GAIN, als_gain);

  // Set to standby mode
  APDS9306_WRITE_BYTE(APDS9306_MAIN_CTRL, 0x00);

  // Check for data, clear main status
  uint8_t status;
  APDS9306_WARNING_CHECK(this->read_byte(APDS9306_MAIN_STATUS, &status), "Reading MAIN STATUS failed.");

  // Set to active mode
  APDS9306_WRITE_BYTE(APDS9306_MAIN_CTRL, 0x02);

  ESP_LOGCONFIG(TAG, "APDS9306 setup complete");
}

void APDS9306::dump_config() {
  LOG_SENSOR("", "APDS9306", this);
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

  ESP_LOGCONFIG(TAG, "  Gain: %u", AMBIENT_LIGHT_GAIN_VALUES[this->gain_]);
  ESP_LOGCONFIG(TAG, "  Measurement rate: %u", MEASUREMENT_RATE_VALUES[this->measurement_rate_]);
  ESP_LOGCONFIG(TAG, "  Measurement Resolution/Bit width: %d", MEASUREMENT_BIT_WIDTH_VALUES[this->bit_width_]);

  LOG_UPDATE_INTERVAL(this);
}

void APDS9306::update() {
  // Check for new data
  uint8_t status;
  APDS9306_WARNING_CHECK(this->read_byte(APDS9306_MAIN_STATUS, &status), "Reading MAIN STATUS failed.");

  this->status_clear_warning();

  status &= 0b00001000;
  if (!status) {  // No new data
    return;
  }

  // Set to standby mode
  APDS9306_WRITE_BYTE(APDS9306_MAIN_CTRL, 0x00);

  // Clear MAIN STATUS
  APDS9306_WARNING_CHECK(this->read_byte(APDS9306_MAIN_STATUS, &status), "Reading MAIN STATUS failed.");

  uint8_t als_data[3];
  APDS9306_WARNING_CHECK(this->read_bytes(APDS9306_ALS_DATA_0, als_data, 3), "Reading ALS data has failed.");

  // Set to active mode
  APDS9306_WRITE_BYTE(APDS9306_MAIN_CTRL, 0x02);

  uint32_t light_level = 0x00 | encode_uint24(als_data[2], als_data[1], als_data[0]);

  float lux = ((float) light_level / AMBIENT_LIGHT_GAIN_VALUES[this->gain_]) *
              (100.0f / MEASUREMENT_RATE_VALUES[this->measurement_rate_]);

  ESP_LOGD(TAG, "Got illuminance=%.1flx from", lux);
  this->publish_state(lux);
}

}  // namespace apds9306
}  // namespace esphome
