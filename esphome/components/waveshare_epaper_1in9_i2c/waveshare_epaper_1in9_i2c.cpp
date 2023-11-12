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
}

void HOT WaveShareEPaper1in9I2C::update() {
  ESP_LOGV(TAG, "WaveShareEPaper1in9I2C::update Called...");

  if (this->display_state_.is_changed()) {
    this->display_state_.flip();

    ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::update Display has changed, performing a refresh...");

    int temperature = this->display_state_.temperature_x10.current;
    bool valid_temperature = temperature != INT32_MAX;
    bool temperature_minus_sign = valid_temperature && temperature < 0;
    temperature = abs(temperature);

    int humidity = this->display_state_.humidity_x10.current;
    bool valid_humidity = humidity != INT32_MAX;
    bool humidity_minus_sign = valid_humidity && humidity < 0;
    humidity = abs(humidity);

    int temperature_digits[TEMPERATURE_DIGITS_LEN] = {-1, -1, -1, -1};
    int humidity_digits[HUMIDITY_DIGITS_LEN] = {-1, -1, -1};

    if (valid_temperature) {
      ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::update Temperature is valid, extracting digits (%d)...", temperature);
      temperature_digits[3] = temperature % 10;
      temperature_digits[2] = (temperature % 100) / 10;
      temperature_digits[1] = (temperature % 1000) / 100;
      temperature_digits[0] = temperature / 1000;
      if (temperature_digits[0] == 0) {
        temperature_digits[0] = -1;
        if (temperature_digits[1] == 0) {
          temperature_digits[1] = -1;
        }
      }

      ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::update Temperature digits: %d %d %d %d", temperature_digits[0],
                temperature_digits[1], temperature_digits[2], temperature_digits[3]);
    }

    if (valid_humidity) {
      ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::update Humidity is valid, extracting digits (%d)...", humidity);
      humidity_digits[2] = humidity % 10;
      humidity_digits[1] = (humidity % 100) / 10;
      humidity_digits[0] = humidity / 100;
      if (humidity_digits[0] == 0) {
        humidity_digits[0] = -1;
      }
      ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::update Humidity digits: %d %d %d", humidity_digits[0], humidity_digits[1],
                humidity_digits[2]);
    }

    uint8_t new_image[] = {
        temperature_minus_sign ? CHAR_EMPTY : get_pixel(temperature_digits[0], 1),
        temperature_minus_sign ? CHAR_MINUS_SIGN[0] : get_pixel(temperature_digits[1], 0),
        temperature_minus_sign ? CHAR_MINUS_SIGN[1] : get_pixel(temperature_digits[1], 1),
        get_pixel(temperature_digits[2], 0),
        get_pixel(temperature_digits[2], 1),
        humidity_minus_sign ? CHAR_MINUS_SIGN[0] : get_pixel(humidity_digits[0], 0),
        humidity_minus_sign ? CHAR_MINUS_SIGN[1] : get_pixel(humidity_digits[0], 1),
        get_pixel(humidity_digits[1], 0),
        get_pixel(humidity_digits[1], 1),
        get_pixel(humidity_digits[2], 0),
        get_pixel(humidity_digits[2], 1),
        get_pixel(temperature_digits[3], 0),
        get_pixel(temperature_digits[3], 1),
        CHAR_EMPTY,  // С°/F°/power/bluetooth
        CHAR_EMPTY   // Position 14 must always be empty
    };

    if (valid_temperature) {
      new_image[4] |= DOT_MASK;  // set top dot
      new_image[13] |= this->display_state_.is_celsius.current ? CHAR_CELSIUS : CHAR_FAHRENHEIT;
    }

    if (valid_humidity) {
      new_image[8] |= DOT_MASK;       // set bottom dot
      new_image[10] |= PERCENT_MASK;  // set percent symbol
    }

    if (this->display_state_.low_power.current) {
      new_image[13] |= LOW_POWER_ON_MASK;
    }

    if (this->display_state_.bluetooth.current) {
      new_image[13] |= BT_ON_MASK;
    }

    bool partial = this->at_update_ != 0;
    this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

    ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::refreshing...");
    if (partial) {
      this->write_lut_(LUT_DU_WB);
    } else {
      this->write_lut_(LUT_5S);
    }
    delay(300);  // NOLINT
    this->write_screen_(new_image);
  }

  this->deep_sleep_();
}

void WaveShareEPaper1in9I2C::reset_screen_() {
  ESP_LOGV(TAG, "WaveShareEPaper1in9I2C::reset_screen");

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
void HOT WaveShareEPaper1in9I2C::read_busy_() {
  ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::read_busy... screen is busy");
  delay(10);
  while (busy_pin_->digital_read() != 1) {  //=1 BUSY;
    delay(1);
  }
  delay(10);
  ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::read_busy... screen no longer busy");
}

void WaveShareEPaper1in9I2C::init_screen_() {
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
  ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::apply_temperature_compensation...");

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
    frame_time = 0x31;  // (49+1)*20ms=1000ms
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
  ESP_LOGVV(TAG, "WaveShareEPaper1in9I2C::write_lut...");
  this->send_commands_(lut, LUT_SIZE);
}

void WaveShareEPaper1in9I2C::write_screen_(const uint8_t framebuffer[FRAMEBUFFER_SIZE]) {
  ESP_LOGV(TAG, "WaveShareEPaper1in9I2C::write_screen...");
  uint8_t before_write_data[] = {
      CMD_SLEEP_OFF,        // Close the sleep
      CMD_POWER_ON,         // Turn on the power
      CMD_RAM_ADDR_0,       // Write RAM address
      CMD_DATA_1_LATCH_ON,  // Turn on the first SRAM
      CMD_DATA_1_LATCH_OFF  // Shut down the first SRAM
  };
  this->send_commands_(before_write_data, 5);

  // Write the image to the screen
  this->send_data_(framebuffer, FRAMEBUFFER_SIZE);
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

void WaveShareEPaper1in9I2C::deep_sleep_() {
  this->send_commands_(&CMD_POWER_OFF, 1, false);
  this->read_busy_();
  this->send_commands_(&CMD_SLEEP_ON, 1, true);
}

void WaveShareEPaper1in9I2C::set_temperature_unit(const char *unit) {
  this->display_state_.is_celsius.future = (strcasecmp(unit, "c") == 0);
}

void WaveShareEPaper1in9I2C::set_temperature(float temperature) {
  if (temperature >= TEMPERATURE_MIN && temperature <= TEMPERATURE_MAX) {
    ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::set_temperature(%f)", temperature);
    this->display_state_.temperature_x10.future = (int16_t) 10.0 * temperature;
  } else {
    ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::set_temperature(INVALID)");
    this->display_state_.temperature_x10.future = INT16_MAX;
  }
}

void WaveShareEPaper1in9I2C::set_humidity(float humidity) {
  if (humidity >= HUMIDITY_MIN && humidity <= HUMIDITY_MAX) {
    ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::set_humidity(%f)", humidity);
    this->display_state_.humidity_x10.future = (int16_t) 10.0 * humidity;
  } else {
    ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::set_humidity(INVALID)");
    this->display_state_.humidity_x10.future = INT16_MAX;
  }
}

void WaveShareEPaper1in9I2C::set_low_power_indicator(bool is_low_power) {
  this->display_state_.low_power.future = is_low_power;
}

void WaveShareEPaper1in9I2C::set_bluetooth_indicator(bool is_bluetooth) {
  this->display_state_.bluetooth.future = is_bluetooth;
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
