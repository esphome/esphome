#include "waveshare_epaper.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static const char *const TAG = "waveshare_epaper_1in9_i2c";

static const uint8_t const EMPTY_DISPLAY[FRAMEBUFFER_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned LUT_SIZE = 7;

// 5S waveform for better anti-ghosting
static const uint8_t const LUT_5S[LUT_SIZE] = {0x82, 0x28, 0x20, 0xA8, 0xA0, 0x50, 0x65};

// White extinction diagram + black out diagram
static const uint8_t const LUT_DU_WB[LUT_SIZE] = {0x82, 0x80, 0x00, 0xC0, 0x80, 0x80, 0x62};

static const uint8_t const DOT_MASK = 0b0000000000100000;
static const uint8_t const PERCENT_MASK = 0b0000000000100000;

static const uint8_t const LOW_POWER_ON_MASK = 0b0000000000010000;
static const uint8_t const LOW_POWER_OFF_MASK = 0b1111111111101111;

static const uint8_t const BT_ON_MASK = 0b0000000000001000;
static const uint8_t const BT_OFF_MASK = 0b1111111111110111;

float WaveShareEPaper1in9I2C::get_setup_priority() const { return setup_priority::IO; }

void WaveShareEPaper1in9I2C::setup() {
  ESP_LOGD(TAG, "Setting up WaveShareEPaper1in9I2C...");
  this->reset_pin_->setup();
  this->busy_pin_->setup();
  this->init_screen();
  this->write_lut(LUT_5S);
  this->read_busy();
  memcpy(this->framebuffer_, EMPTY_DISPLAY, FRAMEBUFFER_SIZE);
  this->write_screen();
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

    bool need_refresh = this->update_framebuffer(new_image);

    if (need_refresh) {
      ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::updating...");
      this->write_lut(LUT_DU_WB);
      delay(300);  // NOLINT
      this->write_screen();
    }
  }

  deep_sleep();
}

void WaveShareEPaper1in9I2C::reset_screen() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::reset_screen");

  reset_pin_->digital_write(true);
  delay(200);  // NOLINT
  reset_pin_->digital_write(false);
  delay(20);
  reset_pin_->digital_write(true);
  delay(200);  // NOLINT
}

/**
 * Wait until the busy_pin goes LOW
 **/
void WaveShareEPaper1in9I2C::read_busy(void) {
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

void WaveShareEPaper1in9I2C::init_screen(void) {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::init_screen...");
  this->reset_screen();
  delay(100);  // NOLINT

  uint8_t data[] = {
      0x2B,  // Power on
      0xFF   // Dummy
  };
  this->command_device_->write(data, 1);
  delay(10);

  data[0] = 0xA7;  // boost
  data[1] = 0xE8;  // TSON
  this->command_device_->write(data, 2);
  delay(10);

  this->apply_temperature_compensation();
}

void WaveShareEPaper1in9I2C::apply_temperature_compensation() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::apply_temperature_compensation...");

  if (this->compensation_temp_ < 10) {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    this->command_device_->write(data, 3);
  } else {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    this->command_device_->write(data, 3);
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
  this->command_device_->write(data, 2);
}

void WaveShareEPaper1in9I2C::write_lut(const uint8_t lut[LUT_SIZE]) {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::write_lut...");
  this->command_device_->write(lut, LUT_SIZE);
}

void WaveShareEPaper1in9I2C::write_screen() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::write_screen...");
  uint8_t before_write_data[] = {
      0xAC,  // Close the sleep
      0x2B,  // Turn on the power
      0x40,  // Write RAM address
      0xA9,  // Turn on the first SRAM
      0xA8   // Shut down the first SRAM
  };
  this->command_device_->write(before_write_data, 5);

  // Write the image to the screen
  this->data_device_->write(this->framebuffer_, FRAMEBUFFER_SIZE);
  uint8_t bg_color = this->inverted_colors_ ? 0x03 : 0x00;
  this->data_device_->write(&bg_color, 1);

  uint8_t after_write_data_1[] = {
      0xAB,  // Turn on the second SRAM
      0xAA,  // Shut down the second SRAM
      0xAF   // display on
  };
  this->command_device_->write(after_write_data_1, 3);

  this->read_busy();
  // delay(2000);

  uint8_t after_write_data_2[] = {
      0xAE,  // Display off
      0x28,  // HV OFF
      0xAD   // Sleep in
  };
  this->command_device_->write(after_write_data_2, 3);
}

void WaveShareEPaper1in9I2C::deep_sleep(void) {
  uint8_t data = 0x28;  // HV OFF
  this->command_device_->write(&data, 1, false);
  this->read_busy();
  data = 0xAC;  // Close the sleep;
  this->command_device_->write(&data, 1, true);
}

bool WaveShareEPaper1in9I2C::update_framebuffer(unsigned char new_image[15]) {
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
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
