#include "sn74hc595.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sn74hc595 {

static const char *const TAG = "sn74hc595";

void SN74HC595Component::pre_setup_() {
  ESP_LOGCONFIG(TAG, "Setting up SN74HC595...");

  if (this->have_oe_pin_) {  // disable output
    this->oe_pin_->setup();
    this->oe_pin_->digital_write(true);
  }
}
void SN74HC595Component::post_setup_() {
  this->latch_pin_->setup();
  this->latch_pin_->digital_write(false);

  // send state to shift register
  this->write_gpio();
}

void SN74HC595GPIOComponent::setup() {
  this->pre_setup_();
  this->clock_pin_->setup();
  this->data_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->data_pin_->digital_write(false);
  this->post_setup_();
}

#ifdef USE_SPI
void SN74HC595SPIComponent::setup() {
  this->pre_setup_();
  this->spi_setup();
  this->post_setup_();
}
#endif

void SN74HC595Component::dump_config() { ESP_LOGCONFIG(TAG, "SN74HC595:"); }

void SN74HC595Component::digital_write_(uint16_t pin, bool value) {
  if (pin >= this->sr_count_ * 8) {
    ESP_LOGE(TAG, "Pin %u is out of range! Maximum pin number with %u chips in series is %u", pin, this->sr_count_,
             (this->sr_count_ * 8) - 1);
    return;
  }
  if (value) {
    this->output_bytes_[pin / 8] |= (1 << (pin % 8));
  } else {
    this->output_bytes_[pin / 8] &= ~(1 << (pin % 8));
  }
  this->write_gpio();
}

void SN74HC595GPIOComponent::write_gpio() {
  for (auto byte = this->output_bytes_.rbegin(); byte != this->output_bytes_.rend(); byte++) {
    for (int8_t i = 7; i >= 0; i--) {
      bool bit = (*byte >> i) & 1;
      this->data_pin_->digital_write(bit);
      this->clock_pin_->digital_write(true);
      this->clock_pin_->digital_write(false);
    }
  }
  SN74HC595Component::write_gpio();
}

#ifdef USE_SPI
void SN74HC595SPIComponent::write_gpio() {
  for (auto byte = this->output_bytes_.rbegin(); byte != this->output_bytes_.rend(); byte++) {
    this->enable();
    this->transfer_byte(*byte);
    this->disable();
  }
  SN74HC595Component::write_gpio();
}
#endif

void SN74HC595Component::write_gpio() {
  // pulse latch to activate new values
  this->latch_pin_->digital_write(true);
  this->latch_pin_->digital_write(false);

  // enable output if configured
  if (this->have_oe_pin_) {
    this->oe_pin_->digital_write(false);
  }
}

float SN74HC595Component::get_setup_priority() const { return setup_priority::IO; }

void SN74HC595GPIOPin::digital_write(bool value) {
  this->parent_->digital_write_(this->pin_, value != this->inverted_);
}
std::string SN74HC595GPIOPin::dump_summary() const { return str_snprintf("%u via SN74HC595", 18, pin_); }

}  // namespace sn74hc595
}  // namespace esphome
