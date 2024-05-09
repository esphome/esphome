#include "apds9306.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

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

    // MAIN_CTRL (0x00)
    // Set to active mode
    APDS9306_WRITE_BYTE(0x00, 0x02);

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
    // Trigger software reset
    APDS9306_WRITE_BYTE(0x00, 0x10);
  }

  void APDS9306::dump_config() {
    ESP_LOGCONFIG(TAG, "APDS9306:");
    LOG_I2C_DEVICE(this);

    LOG_UPDATE_INTERVAL(this);

    LOG_SENSOR("  ", "Light level", this->light_level_sensor_);

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
  }

  void APDS9306::update() {
    uint8_t status;
    APDS9306_WARNING_CHECK(this->read_byte(0x07, &status), "Reading status bit failed.");
    this->status_clear_warning();

    this->get_light_level_(status);
  }

  void APDS9306::get_light_level_(uint8_t status) {
    //if (!(status &= 0b00001000)) // No new data
    //  return;

    uint8_t als_data[3];
    APDS9306_WARNING_CHECK(this->read_byte(0x0D, als_data, 3), "Reading ALS data has failed.");

    uint32_t light_level = 0x0000;
    light_level |= als_data[0];
    light_level |= als_data[1] << 8;
    light_level |= als_data[2] << 16;

    if (this->light_level_sensor_ != nullptr)
      this->light_level_sensor_->publish_state(light_level);
  }

} // namespace apds9306
} // namespace esphome
