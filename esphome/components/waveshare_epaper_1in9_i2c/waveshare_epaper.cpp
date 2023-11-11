#include "waveshare_epaper.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

unsigned char DSPNUM_9in1_off[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static const char *const TAG = "waveshare19paperdisplay";

float WaveShareEPaper1in9I2C::get_setup_priority() const { return setup_priority::IO; }

void WaveShareEPaper1in9I2C::setup() {
  ESP_LOGI(TAG, "Setting up WaveShareEPaper1in9I2C...");
  rst_pin_->setup();
  busy_pin_->setup();
  init();
  lut_5S();
  read_busy();
  write_screen(DSPNUM_9in1_off);
}

void WaveShareEPaper1in9I2C::update() {
  ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::update...");
  bool need_refresh = false;

  if (potential_refresh) {
    potential_refresh = false;
    unsigned char new_image[] = {getPixel(temperature_digits[0], 1),
                                 temperature_positive ? getPixel(temperature_digits[1], 0) : MINUS_SIGN[0],
                                 temperature_positive ? getPixel(temperature_digits[1], 1) : MINUS_SIGN[1],
                                 getPixel(temperature_digits[2], 0),
                                 getPixel(temperature_digits[2], 1),
                                 humidity_positive ? getPixel(humidity_digits[0], 0) : MINUS_SIGN[0],
                                 humidity_positive ? getPixel(humidity_digits[0], 1) : MINUS_SIGN[1],
                                 getPixel(humidity_digits[1], 0),
                                 getPixel(humidity_digits[1], 1),
                                 getPixel(humidity_digits[2], 0),
                                 getPixel(humidity_digits[2], 1),
                                 getPixel(temperature_digits[3], 0),
                                 getPixel(temperature_digits[3], 1),
                                 degrees_type,  // С°/F°/power/bluetooth
                                 image[14]};

    new_image[4] |= DOT;       // set top dot
    new_image[8] |= DOT;       // set bottom dot
    new_image[10] |= PERCENT;  // set percent symbol

    need_refresh = set_data(new_image);
  }

  if (need_refresh) {
    ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::updating...");
    lut_DU_WB();
    delay(300);
    write_screen(image);
  }

  deep_sleep();
}

void WaveShareEPaper1in9I2C::reset() {
  ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::reset");

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
  ESP_LOGE(TAG, "e-Paper busy");
  delay(10);
  while (1) {  //=1 BUSY;
    if (busy_pin_->digital_read() == 1) {
      break;
    }
    delay(1);
  }
  delay(10);
  ESP_LOGE(TAG, "e-Paper busy release");
}

void WaveShareEPaper1in9I2C::init(void) {
  ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::init...");
  reset();
  delay(100);

  uint8_t data[] = {0x2B, 0xFF};
  command_device_->write(data, 1);
  delay(10);

  data[0] = 0xA7;
  data[1] = 0xE8;
  command_device_->write(data, 2);
  delay(10);

  temperature_compensation();
}

void WaveShareEPaper1in9I2C::temperature_compensation(void) {
  ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::temperature_compensation...");

  int temperature = 20;

  if (temperature < 10) {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    command_device_->write(data, 3);
  } else {
    uint8_t data[] = {0x7E, 0x81, 0xB4};
    command_device_->write(data, 3);
  }

  delay(10);

  if (temperature < 5) {
    uint8_t data[] = {0xe7, 0x31};
    command_device_->write(data, 2);
  } else if (temperature < 10) {
    uint8_t data[] = {0xe7, 0x22};
    command_device_->write(data, 2);
  } else if (temperature < 15) {
    uint8_t data[] = {0xe7, 0x18};
    command_device_->write(data, 2);
  } else if (temperature < 20) {
    uint8_t data[] = {0xe7, 0x13};
    command_device_->write(data, 2);
  } else {
    uint8_t data[] = {0xe7, 0x0e};
    command_device_->write(data, 2);
  }
}

/**
 * Set DU WB type of updating
 * DU waveform white extinction diagram + black out diagram
 * Bureau of brush waveform
 **/
void WaveShareEPaper1in9I2C::lut_DU_WB(void) {
  ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::lut_DU_WB...");
  uint8_t data[] = {0x82, 0x80, 0x00, 0xC0, 0x80, 0x80, 0x62};
  command_device_->write(data, 7);
}

/**
 * Set 5S type of updating
 * 5 waveform  better ghosting
 * Boot waveform
 **/
void WaveShareEPaper1in9I2C::lut_5S(void) {
  ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::lut_DU_WB...");
  uint8_t data[] = {0x82, 0x28, 0x20, 0xA8, 0xA0, 0x50, 0x65};
  command_device_->write(data, 7);
}

void WaveShareEPaper1in9I2C::write_screen(unsigned char *image) {
  ESP_LOGI(TAG, "WaveShareEPaper1in9I2C::write_screen...");
  uint8_t before_write_data[] = {0xAC, 0x2B, 0x40, 0xA9, 0xA8};

  command_device_->write(before_write_data, 5);

  data_device_->write(image, 15);
  uint8_t bg_color = inverted_colors ? 0x03 : 0x00;
  data_device_->write(&bg_color, 1);

  uint8_t after_write_data_1[] = {0xAB, 0xAA, 0xAF};
  command_device_->write(after_write_data_1, 3);

  read_busy();
  // delay(2000);

  uint8_t after_write_data_2[] = {0xAE, 0x28, 0xAD};
  command_device_->write(after_write_data_2, 3);
}

void WaveShareEPaper1in9I2C::deep_sleep(void) {
  uint8_t data = 0x28;  // POWER_OFF
  command_device_->write(&data, 1, false);
  read_busy();
  data = 0xAC;  // DEEP_SLEEP;
  command_device_->write(&data, 1, true);
}

bool WaveShareEPaper1in9I2C::set_data(unsigned char new_image[15]) {
  bool need_update = false;
  for (int i = 0; i < 15; i++) {
    if (new_image[i] != image[i]) {
      image[i] = new_image[i];
      need_update = true;
    }
  }
  return need_update;
}

void WaveShareEPaper1in9I2C::set_temperature(float temperature) {
  temperature_positive = temperature > 0;
  parseNumber(temperature_positive ? temperature : -1.0 * temperature, temperature_digits, 4);
  potential_refresh = true;
}

void WaveShareEPaper1in9I2C::set_humidity(float humidity) {
  humidity_positive = humidity > 0;
  parseNumber(humidity_positive ? humidity : -1.0 * humidity, humidity_digits, 3);
  potential_refresh = true;
}

void WaveShareEPaper1in9I2C::set_low_power_indicator(bool is_low_power) {
  if (is_low_power) {
    degrees_type |= LOW_POWER_ON;
  } else {
    degrees_type &= LOW_POWER_OFF;
  }
  potential_refresh = true;
}

void WaveShareEPaper1in9I2C::set_bluetooth_indicator(bool is_bluetooth) {
  if (is_bluetooth) {
    degrees_type |= BT_ON;
  } else {
    degrees_type &= BT_OFF;
  }
  potential_refresh = true;
}
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
