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
  rst_pin_->setup();
  busy_pin_->setup();
  init_screen();
  write_lut(LUT_5S);
  read_busy();
  write_screen(EMPTY_DISPLAY);
}

void WaveShareEPaper1in9I2C::update() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::update...");
  bool need_refresh = false;

  if (potential_refresh) {
    potential_refresh = false;
    unsigned char new_image[] = {get_pixel(temperature_digits[0], 1),
                                 temperature_positive ? get_pixel(temperature_digits[1], 0) : CHAR_MINUS_SIGN[0],
                                 temperature_positive ? get_pixel(temperature_digits[1], 1) : CHAR_MINUS_SIGN[1],
                                 get_pixel(temperature_digits[2], 0),
                                 get_pixel(temperature_digits[2], 1),
                                 humidity_positive ? get_pixel(humidity_digits[0], 0) : CHAR_MINUS_SIGN[0],
                                 humidity_positive ? get_pixel(humidity_digits[0], 1) : CHAR_MINUS_SIGN[1],
                                 get_pixel(humidity_digits[1], 0),
                                 get_pixel(humidity_digits[1], 1),
                                 get_pixel(humidity_digits[2], 0),
                                 get_pixel(humidity_digits[2], 1),
                                 get_pixel(temperature_digits[3], 0),
                                 get_pixel(temperature_digits[3], 1),
                                 degrees_type,  // С°/F°/power/bluetooth
                                 image[14]};

    new_image[4] |= DOT_MASK;       // set top dot
    new_image[8] |= DOT_MASK;       // set bottom dot
    new_image[10] |= PERCENT_MASK;  // set percent symbol

    need_refresh = update_framebuffer(new_image);
  }

  if (need_refresh) {
    ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::updating...");
    write_lut(LUT_DU_WB);
    delay(300);
    write_screen(image);
  }

  deep_sleep();
}

void WaveShareEPaper1in9I2C::reset_screen() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::reset_screen");

  rst_pin_->digital_write(true);
  delay(200);
  rst_pin_->digital_write(false);
  delay(20);
  rst_pin_->digital_write(true);
  delay(200);
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
  reset_screen();
  delay(100);

  uint8_t data[] = {
      0x2B,  // Power on
      0xFF   // Dummy
  };
  command_device_->write(data, 1);
  delay(10);

  data[0] = 0xA7;  // boost
  data[1] = 0xE8;  // TSON
  command_device_->write(data, 2);
  delay(10);

  apply_temperature_compensation();
}

void WaveShareEPaper1in9I2C::apply_temperature_compensation() {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::apply_temperature_compensation...");

  if (compensation_temp < 10) {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    command_device_->write(data, 3);
  } else {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    command_device_->write(data, 3);
  }

  delay(10);

  uint8_t frame_time;
  if (compensation_temp < 5) {
  } else if (compensation_temp < 10) {
    frame_time = 0x22;  // (34+1)*20ms=700ms
  } else if (compensation_temp < 15) {
    frame_time = 0x18;  // (24+1)*20ms=500ms;
  } else if (compensation_temp < 20) {
    frame_time = 0x13;  // (19+1)*20ms=400ms
  } else {
    frame_time = 0x0e;  // (14+1)*20ms=300ms
  }
  uint8_t data[] = {
      0xe7,       // Set default frame time
      frame_time  // Computed frame time
  };
  command_device_->write(data, 2);
}

void WaveShareEPaper1in9I2C::write_lut(const uint8_t lut[LUT_SIZE]) {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::write_lut...");
  command_device_->write(lut, LUT_SIZE);
}

void WaveShareEPaper1in9I2C::write_screen(const uint8_t *image) {
  ESP_LOGD(TAG, "WaveShareEPaper1in9I2C::write_screen...");
  uint8_t before_write_data[] = {
      0xAC,  // Close the sleep
      0x2B,  // Turn on the power
      0x40,  // Write RAM address
      0xA9,  // Turn on the first SRAM
      0xA8   // Shut down the first SRAM
  };
  command_device_->write(before_write_data, 5);

  // Write the image to the screen
  data_device_->write(image, FRAMEBUFFER_SIZE);
  uint8_t bg_color = inverted_colors ? 0x03 : 0x00;
  data_device_->write(&bg_color, 1);

  uint8_t after_write_data_1[] = {
      0xAB,  // Turn on the second SRAM
      0xAA,  // Shut down the second SRAM
      0xAF   // display on
  };
  command_device_->write(after_write_data_1, 3);

  read_busy();
  // delay(2000);

  uint8_t after_write_data_2[] = {
      0xAE,  // Display off
      0x28,  // HV OFF
      0xAD   // Sleep in
  };
  command_device_->write(after_write_data_2, 3);
}

void WaveShareEPaper1in9I2C::deep_sleep(void) {
  uint8_t data = 0x28;  // HV OFF
  command_device_->write(&data, 1, false);
  read_busy();
  data = 0xAC;  // Close the sleep;
  command_device_->write(&data, 1, true);
}

bool WaveShareEPaper1in9I2C::update_framebuffer(unsigned char new_image[15]) {
  bool need_update = false;
  for (int i = 0; i < FRAMEBUFFER_SIZE; i++) {
    if (new_image[i] != image[i]) {
      image[i] = new_image[i];
      need_update = true;
    }
  }
  return need_update;
}

void WaveShareEPaper1in9I2C::set_temperature(float temperature) {
  temperature_positive = temperature > 0;
  parse_number(temperature_positive ? temperature : -1.0 * temperature, temperature_digits, TEMPERATURE_DIGITS_LEN);
  potential_refresh = true;
}

void WaveShareEPaper1in9I2C::set_humidity(float humidity) {
  humidity_positive = humidity > 0;
  parse_number(humidity_positive ? humidity : -1.0 * humidity, humidity_digits, HUMIDITY_DIGITS_LEN);
  potential_refresh = true;
}

void WaveShareEPaper1in9I2C::set_low_power_indicator(bool is_low_power) {
  if (is_low_power) {
    degrees_type |= LOW_POWER_ON_MASK;
  } else {
    degrees_type &= LOW_POWER_OFF_MASK;
  }
  potential_refresh = true;
}

void WaveShareEPaper1in9I2C::set_bluetooth_indicator(bool is_bluetooth) {
  if (is_bluetooth) {
    degrees_type |= BT_ON_MASK;
  } else {
    degrees_type &= BT_OFF_MASK;
  }
  potential_refresh = true;
}
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
