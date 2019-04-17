#include "tcs34725.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcs34725 {

static const char *TAG = "tcs34725";

static const uint8_t TCS34725_ADDRESS = 0x29;
static const uint8_t TCS34725_COMMAND_BIT = 0x80;
static const uint8_t TCS34725_REGISTER_ID = TCS34725_COMMAND_BIT | 0x12;
static const uint8_t TCS34725_REGISTER_ATIME = TCS34725_COMMAND_BIT | 0x01;
static const uint8_t TCS34725_REGISTER_CONTROL = TCS34725_COMMAND_BIT | 0x0F;
static const uint8_t TCS34725_REGISTER_ENABLE = TCS34725_COMMAND_BIT | 0x00;
static const uint8_t TCS34725_REGISTER_CDATAL = TCS34725_COMMAND_BIT | 0x14;
static const uint8_t TCS34725_REGISTER_RDATAL = TCS34725_COMMAND_BIT | 0x16;
static const uint8_t TCS34725_REGISTER_GDATAL = TCS34725_COMMAND_BIT | 0x18;
static const uint8_t TCS34725_REGISTER_BDATAL = TCS34725_COMMAND_BIT | 0x1A;

void TCS34725Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCS34725...");
  uint8_t id;
  if (!this->read_byte(TCS34725_REGISTER_ID, &id)) {
    this->mark_failed();
    return;
  }

  uint8_t integration_reg = this->integration_time_;
  uint8_t gain_reg = this->gain_;
  if (!this->write_byte(TCS34725_REGISTER_ATIME, integration_reg) ||
      !this->write_byte(TCS34725_REGISTER_CONTROL, gain_reg)) {
    this->mark_failed();
    return;
  }

  if (!this->write_byte(TCS34725_REGISTER_ENABLE, 0x01)) {  // Power on (internal oscillator on)
    this->mark_failed();
    return;
  }
  delay(3);
  if (!this->write_byte(TCS34725_REGISTER_ENABLE, 0x03)) {  // Power on (internal oscillator on) + RGBC ADC Enable
    this->mark_failed();
    return;
  }
}

void TCS34725Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TCS34725:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with TCS34725 failed!");
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Clear Channel", this->clear_sensor_);
  LOG_SENSOR("  ", "Red Channel", this->red_sensor_);
  LOG_SENSOR("  ", "Green Channel", this->green_sensor_);
  LOG_SENSOR("  ", "Blue Channel", this->blue_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Color Temperature", this->color_temperature_sensor_);
}
float TCS34725Component::get_setup_priority() const { return setup_priority::DATA; }
void TCS34725Component::update() {
  uint16_t raw_c;
  uint16_t raw_r;
  uint16_t raw_g;
  uint16_t raw_b;

  if (!this->read_byte_16(TCS34725_REGISTER_CDATAL, &raw_c) || !this->read_byte_16(TCS34725_REGISTER_RDATAL, &raw_r) ||
      !this->read_byte_16(TCS34725_REGISTER_GDATAL, &raw_g) || !this->read_byte_16(TCS34725_REGISTER_BDATAL, &raw_b)) {
    ESP_LOGW(TAG, "Reading data from TCS34725 failed!");
    this->status_set_warning();
    return;
  }

  const float channel_c = raw_c / 655.35f;
  const float channel_r = raw_r / 655.35f;
  const float channel_g = raw_g / 655.35f;
  const float channel_b = raw_b / 655.35f;
  if (this->clear_sensor_ != nullptr)
    this->clear_sensor_->publish_state(channel_c);
  if (this->red_sensor_ != nullptr)
    this->red_sensor_->publish_state(channel_r);
  if (this->green_sensor_ != nullptr)
    this->green_sensor_->publish_state(channel_g);
  if (this->blue_sensor_ != nullptr)
    this->blue_sensor_->publish_state(channel_b);

  // Formulae taken from Adafruit TCS35725 library
  float illuminance = (-0.32466f * channel_r) + (1.57837f * channel_g) + (-0.73191f * channel_b);
  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(illuminance);

  // Color temperature
  // 1. Convert RGB to XYZ color space
  const float x = (-0.14282f * raw_r) + (1.54924f * raw_g) + (-0.95641f * raw_b);
  const float y = (-0.32466f * raw_r) + (1.57837f * raw_g) + (-0.73191f * raw_b);
  const float z = (-0.68202f * raw_r) + (0.77073f * raw_g) + (0.56332f * raw_b);

  // 2. Calculate chromacity coordinates
  const float xc = (x) / (x + y + z);
  const float yc = (y) / (x + y + z);

  // 3. Use McCamy's formula to determine the color temperature
  const float n = (xc - 0.3320f) / (0.1858f - yc);

  // 4. final color temperature in Kelvin.
  const float color_temperature = (449.0f * powf(n, 3.0f)) + (3525.0f * powf(n, 2.0f)) + (6823.3f * n) + 5520.33f;
  if (this->color_temperature_sensor_ != nullptr)
    this->color_temperature_sensor_->publish_state(color_temperature);

  ESP_LOGD(TAG, "Got R=%.1f%%,G=%.1f%%,B=%.1f%%,C=%.1f%% Illuminance=%.1flx Color Temperature=%.1fK", channel_r,
           channel_g, channel_b, channel_c, illuminance, color_temperature);

  this->status_clear_warning();
}
void TCS34725Component::set_integration_time(TCS34725IntegrationTime integration_time) {
  this->integration_time_ = integration_time;
}
void TCS34725Component::set_gain(TCS34725Gain gain) { this->gain_ = gain; }

}  // namespace tcs34725
}  // namespace esphome
