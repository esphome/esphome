#include "tm1651.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tm1651 {

static const char *TAG = "tm1651.display";
static const uint8_t MAX_LEVEL = 7;
static const uint8_t MAX_INPUT_LEVEL = 1;

void TM1651Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TM1651...");

  uint8_t clk = clk_pin_->get_pin();
  uint8_t dio = dio_pin_->get_pin();

  battery_display_ = new TM1651(clk, dio);
  battery_display_->init();
  battery_display_->clearDisplay();
}

void TM1651Display::dump_config() {
  ESP_LOGCONFIG(TAG, "TM1651 Battery Display");
  LOG_PIN("  CLK: ", clk_pin_);
  LOG_PIN("  DIO: ", dio_pin_);
}

void TM1651Display::set_level(float new_level) {
  this->level_ = calculate_level(new_level);
  this->repaint();
}

void TM1651Display::set_brightness(uint8_t new_brightness) {
  this->brightness_ = calculate_brightness(new_brightness);
  this->repaint();
}

void TM1651Display::repaint() {
  battery_display_->set(this->brightness_);
  battery_display_->displayLevel(this->level_);
}

uint8_t TM1651Display::calculate_level(float level) {
  if (level < 0) {
    return 0;
  } else if (level > MAX_INPUT_LEVEL) {
    return MAX_INPUT_LEVEL;
  }

  float calculated_level = MAX_LEVEL / (MAX_INPUT_LEVEL / level);
  return (uint8_t) roundf(calculated_level);
}

uint8_t TM1651Display::calculate_brightness(uint8_t new_brightness) {
  if (new_brightness <= 1) {
    return 0;
  } else if (new_brightness == 2) {
    return 2;
  } else if (new_brightness >= 3) {
    return 7;
  }

  return 0;
}

}  // namespace tm1651
}  // namespace esphome
