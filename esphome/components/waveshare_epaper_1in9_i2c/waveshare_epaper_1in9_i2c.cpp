#include "waveshare_epaper_1in9_i2c.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "i2c_commands.h"
#include "display_utils.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static const char *const TAG = "waveshare_epaper_1in9_i2c";

void WaveShareEPaper1in9I2C::setup() {
  ESP_LOGD(TAG, "Setting up WaveShareEPaper1in9I2C...");
  this->reset_pin_->setup();
  this->busy_pin_->setup();
  this->init_screen_();
  this->write_lut_(LUT_5S);
  this->read_busy_();
  memset(this->framebuffer_, CHAR_EMPTY, FRAMEBUFFER_SIZE);
  this->write_screen_();
}

void WaveShareEPaper1in9I2C::update() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::update...");

  if (this->potential_refresh_) {
    this->potential_refresh_ = false;
    unsigned char new_image[] = {
        get_pixel(this->temperature_digits_[0], 1),
        this->temperature_positive_ ? get_pixel(this->temperature_digits_[1], 0) : CHAR_MINUS_SIGN[0],
        this->temperature_positive_ ? get_pixel(this->temperature_digits_[1], 1) : CHAR_MINUS_SIGN[1],
        get_pixel(this->temperature_digits_[2], 0),
        get_pixel(this->temperature_digits_[2], 1),
        this->humidity_positive_ ? get_pixel(this->humidity_digits_[0], 0) : CHAR_MINUS_SIGN[0],
        this->humidity_positive_ ? get_pixel(this->humidity_digits_[0], 1) : CHAR_MINUS_SIGN[1],
        get_pixel(this->humidity_digits_[1], 0),
        get_pixel(this->humidity_digits_[1], 1),
        get_pixel(this->humidity_digits_[2], 0),
        get_pixel(this->humidity_digits_[2], 1),
        get_pixel(this->temperature_digits_[3], 0),
        get_pixel(this->temperature_digits_[3], 1),
        this->degrees_type_,  // С°/F°/power/bluetooth
        CHAR_EMPTY            // Position 14 must always be empty
    };

    new_image[4] |= DOT_MASK;       // set top dot
    new_image[8] |= DOT_MASK;       // set bottom dot
    new_image[10] |= PERCENT_MASK;  // set percent symbol

    bool need_refresh = this->update_framebuffer_(new_image);

    if (need_refresh) {
      ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::refreshing...");
      this->write_lut_(LUT_DU_WB);
      delay(300);  // NOLINT
      this->write_screen_();
    }
  }

  this->deep_sleep_();
}

void WaveShareEPaper1in9I2C::reset_screen_() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::reset_screen");

  this->send_reset_(true);
  delay(200);  // NOLINT
  this->send_reset_(false);
  delay(20);
  this->send_reset_(true);
  delay(200);  // NOLINT
}

/**
 * Wait until the busy_pin goes LOW
 **/
void HOT WaveShareEPaper1in9I2C::read_busy_(void) {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::read_busy... screen is busy");
  delay(10);
  while (1) {  //=1 BUSY;
    if (busy_pin_->digital_read() == 1) {
      break;
    }
    delay(1);
  }
  delay(10);
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::read_busy... screen no longer busy");
}

void WaveShareEPaper1in9I2C::init_screen_(void) {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::init_screen...");
  this->reset_screen_();
  delay(100);  // NOLINT

  this->send_commands_(&CMD_POWER_ON, 1);
  delay(10);

  uint8_t data[] = {
      CMD_DCDC_BOOST_X8,  // DCDC Boost x8
      0xE8                // TSON
  };
  this->send_commands_(data, 2);
  delay(10);

  this->apply_temperature_compensation();
}

void WaveShareEPaper1in9I2C::apply_temperature_compensation() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::apply_temperature_compensation...");

  if (this->compensation_temp_ < 10) {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    this->send_commands_(data, 3);
  } else {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    this->send_commands_(data, 3);
  }

  delay(10);

  uint8_t frame_time;
  if (this->compensation_temp_ < 5) {
  } else if (this->compensation_temp_ < 10) {
    frame_time = 0x22;  // (34+1)*20ms=700ms
  } else if (this->compensation_temp_ < 15) {
    frame_time = 0x18;  // (24+1)*20ms=500ms;
  } else if (this->compensation_temp_ < 20) {
    frame_time = 0x13;  // (19+1)*20ms=400ms
  } else {
    frame_time = 0x0e;  // (14+1)*20ms=300ms
  }
  uint8_t data[] = {
      0xe7,       // Set default frame time
      frame_time  // Computed frame time
  };
  this->send_commands_(data, 2);
}

void WaveShareEPaper1in9I2C::write_lut_(const uint8_t lut[LUT_SIZE]) {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::write_lut...");
  this->send_commands_(lut, LUT_SIZE);
}

void WaveShareEPaper1in9I2C::write_screen_() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::write_screen...");
  uint8_t before_write_data[] = {
      CMD_SLEEP_OFF,        // Close the sleep
      CMD_POWER_ON,         // Turn on the power
      CMD_RAM_ADDR_0,       // Write RAM address
      CMD_DATA_1_LATCH_ON,  // Turn on the first SRAM
      CMD_DATA_1_LATCH_OFF  // Shut down the first SRAM
  };
  this->send_commands_(before_write_data, 5);

  // Write the image to the screen
  this->send_data_(this->framebuffer_, FRAMEBUFFER_SIZE);
  uint8_t bg_color = this->inverted_colors_ ? 0x03 : 0x00;
  this->send_data_(&bg_color, 1);

  uint8_t after_write_data_1[] = {
      CMD_DATA_2_LATCH_ON,   // Turn on the second SRAM
      CMD_DATA_2_LATCH_OFF,  // Shut down the second SRAM
      CMD_DISPLAY_ON         // display on
  };
  this->send_commands_(after_write_data_1, 3);

  this->read_busy_();

  uint8_t after_write_data_2[] = {
      CMD_DISPLAY_OFF,  // Display off
      CMD_POWER_OFF,    // HV OFF
      CMD_SLEEP_ON      // Sleep in
  };
  this->send_commands_(after_write_data_2, 3);
}

void WaveShareEPaper1in9I2C::deep_sleep_(void) {
  this->send_commands_(&CMD_POWER_OFF, 1, false);
  this->read_busy_();
  this->send_commands_(&CMD_SLEEP_ON, 1, true);
}

bool HOT WaveShareEPaper1in9I2C::update_framebuffer_(unsigned char new_image[15]) {
  bool need_update = false;
  for (int i = 0; i < FRAMEBUFFER_SIZE; i++) {
    if (new_image[i] != this->framebuffer_[i]) {
      this->framebuffer_[i] = new_image[i];
      need_update = true;
    }
  }
  return need_update;
}

void WaveShareEPaper1in9I2C::set_temperature(float temperature) {
  this->temperature_positive_ = temperature > 0;
  parse_number(this->temperature_positive_ ? temperature : -1.0 * temperature, this->temperature_digits_,
               TEMPERATURE_DIGITS_LEN);
  this->potential_refresh_ = true;
}

void WaveShareEPaper1in9I2C::set_humidity(float humidity) {
  this->humidity_positive_ = humidity > 0;
  parse_number(this->humidity_positive_ ? humidity : -1.0 * humidity, this->humidity_digits_, HUMIDITY_DIGITS_LEN);
  this->potential_refresh_ = true;
}

void WaveShareEPaper1in9I2C::set_low_power_indicator(bool is_low_power) {
  if (is_low_power) {
    this->degrees_type_ |= LOW_POWER_ON_MASK;
  } else {
    this->degrees_type_ &= LOW_POWER_OFF_MASK;
  }
  this->potential_refresh_ = true;
}

void WaveShareEPaper1in9I2C::set_bluetooth_indicator(bool is_bluetooth) {
  if (is_bluetooth) {
    this->degrees_type_ |= BT_ON_MASK;
  } else {
    this->degrees_type_ &= BT_OFF_MASK;
  }
  this->potential_refresh_ = true;
}

void WaveShareEPaper1in9I2C::dump_config() {
  ESP_LOGCONFIG(TAG, "Waveshare E-Paper 1.9in I2C");
  ESP_LOGCONFIG(TAG, "  I2C Command Address: 0x%02X", this->command_device_address_);
  ESP_LOGCONFIG(TAG, "  I2C Data Address: 0x%02X", this->data_device_address_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveShareEPaper1in9I2C::send_commands_(const uint8_t *data, uint8_t len, bool stop) {
  this->command_device_->write(data, len, stop);
}

void WaveShareEPaper1in9I2C::send_data_(const uint8_t *data, uint8_t len, bool stop) {
  this->data_device_->write(data, len, stop);
}
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
