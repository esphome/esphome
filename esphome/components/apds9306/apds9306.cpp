#include "apds9306.h"
#include "esphome/core/log.h"

namespace esphome {
namespace apds9306 {

static const char *const TAG = "apds9306";

#define APDS9306_ERROR_CHECK(func) \
if (!(func)) {\
  this->mark_failed();\
  return;\
}
#define APDS9306_WARNING_CHECK(func, warning) \
if(!(func)) { \
  ESP_LOGW(TAG, warning); \
  this->status_set_warning(); \
  return; \
}
#define APDS9306_WRITE_BYTE(reg, value) APDS9306_ERROR_CHECK(this->write_byte(reg, value));

void APDS9306::setup() {
  ESP_LOGCONFIG(TAG, "Setting up APDS9306...");
  uint8_t id;
  if (!this->read_byte(0x06, &id)) { // Part ID register
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (id != 0xB1 && id != 0xB3) { //0xB1 for APDS9306 0xB3 for APDS9306-065
    this->error_code_ = WRONG_ID;
    this->mark_failed();
    return;
  }

  // Conversions
  switch (gain_) {
    case 0:
      gain_val_ = 1;
      break;
    case 1:
      gain_val_ = 3;
      break;
    case 2:
      gain_val_ = 6;
      break;
    case 3:
      gain_val_ = 9;
      break;
    case 4:
      gain_val_ = 18;
      break;
  }

  switch (measurement_rate_) {
    case 0:
      rate_val_ = 25;
      break;
    case 1:
      rate_val_ = 50;
      break;
    case 2:
      rate_val_ = 100;
      break;
    case 3:
      rate_val_ = 200;
      break;
    case 4:
      rate_val_ = 500;
      break;
    case 5:
      rate_val_ = 1000;
      break;
  }

  switch (bit_width_) {
    case 0:
      bit_width_val_ = 20;
      break;
    case 1:
      bit_width_val_ = 19;
      break;
    case 2:
      bit_width_val_ = 18;
      break;
    case 3:
      bit_width_val_ = 17;
      break;
    case 4:
      bit_width_val_ = 16;
      break;
    case 5:
      bit_width_val_ = 13;
      break;
  }

  // MAIN_CTRL (0x00)
  // Trigger software reset
  APDS9306_WRITE_BYTE(0x00, 0x10);
  // Put in standby mode
  APDS9306_WRITE_BYTE(0x00, 0x00);

  // ALS_MEAS_RATE (0x04)
  uint8_t als_meas_rate = 0;
  APDS9306_ERROR_CHECK(this->read_byte(0x04, &als_meas_rate));
  als_meas_rate &= 0x77;
  // ALS resolution, see datasheet or init.py for options
  als_meas_rate |= (this->bit_width_ & 0b111) << 4;
  // ALS measurement rate, see datasheet or init.py for options
  als_meas_rate |= (this->measurement_rate_ & 0b111);

  APDS9306_WRITE_BYTE(0x04, als_meas_rate);

  // ALS_GAIN (0x05)
  uint8_t als_gain = 0;
  APDS9306_ERROR_CHECK(this->read_byte(0x05, &als_gain));
  als_gain &= 0x07;
  // ALS gain, see datasheet or init.py for options
  als_gain |= (this->gain_ & 0b111);

  APDS9306_WRITE_BYTE(0x05, als_gain);

  // MAIN_CTRL (0x00)
  // Set to active mode
  APDS9306_WRITE_BYTE(0x00, 0x02);
}

void APDS9306::dump_config() {
  LOG_SENSOR("", TAG, this);
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
  APDS9306_WARNING_CHECK(this->read_byte(0x07, &status), "Reading status bit failed.");

  if (!(status &= 0b00001000)) // No new data
    return;

  uint8_t als_data[3];
  APDS9306_WARNING_CHECK(this->read_byte(0x0D, als_data, 3), "Reading ALS data has failed.");

  uint32_t light_level = 0x0000 | ( (als_data[0]) + (als_data[1] << 8) + (als_data[2] << 16));

  float lux = (light_level / gain_val_) * (100.0f / rate_val_);

  ESP_LOGD(TAG, "Got illuminance=%.1flx", lux);
  this->publish_state(lux);
  this->status_clear_warning();
}

} // namespace apds9306
} // namespace esphome
