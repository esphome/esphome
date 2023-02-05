#ifdef USE_ARDUINO

#include "fastled_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fastled_base {

static const char *const TAG = "fastled";

void FastLEDLightOutput::setup() {
  this->bus_->setup();
  this->leds_ = (CRGB *) this->bus_->channels();
  this->num_leds_ = this->bus_->num_chips_;
  this->effect_data_ = this->bus_->effect_data();
  ESP_LOGCONFIG(TAG, "Setting up FastLED light...:%p", this->leds_);
  // this->controller_->init();
  // this->controller_->setLeds(this->leds_, this->num_leds_);
  // this->effect_data_ = new uint8_t[this->num_leds_];  // NOLINT
  // if (!this->max_refresh_rate_.has_value()) {
  //   this->set_max_refresh_rate(this->controller_->getMaxRefreshRate());
  // }
}
void FastLEDLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "FastLED light:");
  ESP_LOGCONFIG(TAG, "  Num LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  Max refresh rate: %u", *this->max_refresh_rate_);
}
void FastLEDLightOutput::write_state(light::LightState *state) {
  ESP_LOGCONFIG(TAG, "FastLED light:write_state:%p:%d:%p", state, this->size(), this->leds_);
  // float red;
  // float green;
  // float blue;
  // state->get_rgb(&red, &green, &blue);
  // this->bus_->write_mapping(this->mapping_, red);
  // for (int i = 0; i < this->size(); i++) {
  //     this->leds_[i][0] = 0x60;
  //     this->leds_[i][1] = 0x60;
  //     this->leds_[i][2] = 0x60;
  // }
  this->bus_->schedule_refresh();
  // // protect from refreshing too often
  // uint32_t now = micros();
  // if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
  //   // try again next loop iteration, so that this change won't get lost
  //   this->schedule_show();
  //   return;
  // }
  // this->last_refresh_ = now;
  // this->mark_shown_();

  // ESP_LOGVV(TAG, "Writing RGB values to bus...");
  // this->controller_->showLeds();
}

}  // namespace fastled_base
}  // namespace esphome

#endif  // USE_ARDUINO
