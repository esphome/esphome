#ifdef USE_ARDUINO

#include "tm1651.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tm1651 {

static const char *const TAG = "tm1651.display";

static const uint8_t MAX_INPUT_LEVEL_PERCENT = 100;
static const uint8_t TM1651_MAX_LEVEL = 7;

static const uint8_t TM1651_BRIGHTNESS_LOW = 0;
static const uint8_t TM1651_BRIGHTNESS_MEDIUM = 2;
static const uint8_t TM1651_BRIGHTNESS_HIGH = 7;

void TM1651Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TM1651...");

  uint8_t clk = clk_pin_->get_pin();
  uint8_t dio = dio_pin_->get_pin();

  battery_display_ = make_unique<TM1651>(clk, dio);
  battery_display_->init();
  battery_display_->clearDisplay();
}

void TM1651Display::dump_config() {
  ESP_LOGCONFIG(TAG, "TM1651 Battery Display");
  LOG_PIN("  CLK: ", clk_pin_);
  LOG_PIN("  DIO: ", dio_pin_);
}

void TM1651Display::set_level_percent(uint8_t new_level) {
  this->level_ = calculate_level_(new_level);
  this->repaint_();
}

void TM1651Display::set_level(uint8_t new_level) {
  this->level_ = new_level;
  this->repaint_();
}

void TM1651Display::set_brightness(uint8_t new_brightness) {
  this->brightness_ = calculate_brightness_(new_brightness);
  this->repaint_();
}

void TM1651Display::turn_on() {
  this->is_on_ = true;
  this->repaint_();
}

void TM1651Display::turn_off() {
  this->is_on_ = false;
  battery_display_->displayLevel(0);
}

void TM1651Display::repaint_() {
  if (!this->is_on_) {
    return;
  }

  battery_display_->set(this->brightness_);
  battery_display_->displayLevel(this->level_);
}

uint8_t TM1651Display::calculate_level_(uint8_t new_level) {
  if (new_level == 0) {
    return 0;
  }

  float calculated_level = TM1651_MAX_LEVEL / (float) (MAX_INPUT_LEVEL_PERCENT / (float) new_level);
  return (uint8_t) roundf(calculated_level);
}

uint8_t TM1651Display::calculate_brightness_(uint8_t new_brightness) {
  if (new_brightness <= 1) {
    return TM1651_BRIGHTNESS_LOW;
  } else if (new_brightness == 2) {
    return TM1651_BRIGHTNESS_MEDIUM;
  } else if (new_brightness >= 3) {
    return TM1651_BRIGHTNESS_HIGH;
  }

  return TM1651_BRIGHTNESS_LOW;
}

}  // namespace tm1651
}  // namespace esphome

#endif  // USE_ARDUINO
