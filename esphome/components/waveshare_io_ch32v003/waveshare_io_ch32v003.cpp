#include "waveshare_io_ch32v003.h"
#include "esphome/core/log.h"

namespace esphome::waveshare_io_ch32v003 {

static const uint8_t IO_EXTENSION_DIRECTION = 0x02;
static const uint8_t IO_EXTENSION_IO_OUTPUT_ADDR = 0x03;
static const uint8_t IO_EXTENSION_IO_INPUT_ADDR = 0x04;
static const uint8_t IO_EXTENSION_PWM_ADDR = 0x05;
static const uint8_t IO_EXTENSION_ADC_ADDR = 0x06;
static const uint8_t IO_EXTENSION_RTC_INT_ADDR = 0x07;

static const char *const TAG = "waveshare_io_ch32v003";

void WaveshareIOCH32V003Component::setup() {
  this->mode_mask_ = 0xFF;    // Set all pins to output mode
  this->output_mask_ = 0xFF;  // Set all pins to high (output mode)

  bool step1 = this->write_gpio_modes_();
  bool step2 = this->write_gpio_outputs_();

  if (!step1 || !step2) {
    ESP_LOGE(TAG, "Failed to initialize Waveshare IO expander");
    this->mark_failed();
    return;
  }

  this->disable_loop();
}

void WaveshareIOCH32V003Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  // bits: 0 = input, 1 = output
  if (flags == gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->mode_mask_ &= ~(1 << pin);
    this->enable_loop();
  } else if (flags == gpio::FLAG_OUTPUT) {
    // Set mode mask bit
    this->mode_mask_ |= 1 << pin;
  }
  this->write_gpio_modes_();
}

void WaveshareIOCH32V003Component::loop() { this->reset_pin_cache_(); }

void WaveshareIOCH32V003Component::dump_config() {
  ESP_LOGCONFIG(TAG, "WaveshareIO:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

uint16_t WaveshareIOCH32V003Component::get_adc_value() {
  if (this->is_failed())
    return 0;

  uint8_t data[2];
  if (!this->read_bytes(IO_EXTENSION_ADC_ADDR, data, 2)) {
    this->status_set_warning(LOG_STR("Failed to read ADC register"));
    return 0;
  }
  uint16_t adc_value = (data[1] << 8) | data[0];
  this->status_clear_warning();
  return adc_value;
}

uint8_t WaveshareIOCH32V003Component::get_rtc_interrupt_status() {
  if (this->is_failed())
    return 0;

  uint8_t data = 0;
  if (!this->read_bytes(IO_EXTENSION_RTC_INT_ADDR, &data, 1)) {
    this->status_set_warning(LOG_STR("Failed to read RTC interrupt register"));
    return 0;
  }
  this->status_clear_warning();
  return data;
}

void WaveshareIOCH32V003Component::set_pwm_value(uint8_t value) {
  if (this->is_failed())
    return;

  // PWM limits are enforced at the output component level to protect hardware
  // based on circuit schematic requirements. This follows the pattern from the
  // original Waveshare IO library function "void IO_EXTENSION_Pwm_Output(uint8_t Value)".

  if (!this->write_byte(IO_EXTENSION_PWM_ADDR, value)) {
    this->status_set_warning(LOG_STR("Failed to set PWM duty cycle"));
    return;
  }

  this->status_clear_warning();
}

bool WaveshareIOCH32V003Component::write_gpio_modes_() {
  if (this->is_failed())
    return false;
  if (!this->write_byte(IO_EXTENSION_DIRECTION, this->mode_mask_)) {
    this->status_set_warning(LOG_STR("Failed to write mode register"));
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool WaveshareIOCH32V003Component::write_gpio_outputs_() {
  if (this->is_failed())
    return false;
  if (!this->write_byte(IO_EXTENSION_IO_OUTPUT_ADDR, this->output_mask_)) {
    this->status_set_warning(LOG_STR("Failed to write output register"));
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool WaveshareIOCH32V003Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;

  uint8_t data = 0;
  if (!this->read_bytes(IO_EXTENSION_IO_INPUT_ADDR, &data, 1)) {
    this->status_set_warning(LOG_STR("Failed to read input register"));
    return false;
  }
  this->input_mask_ = data;

  this->status_clear_warning();
  return true;
}

void WaveshareIOCH32V003Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }

  uint8_t data = this->output_mask_;
  if (!this->write_byte(IO_EXTENSION_IO_OUTPUT_ADDR, data)) {
    this->status_set_warning(LOG_STR("Failed to write output register"));
    return;
  }

  this->status_clear_warning();
}

bool WaveshareIOCH32V003Component::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }
float WaveshareIOCH32V003Component::get_setup_priority() const { return setup_priority::IO; }

void WaveshareIOCH32V003GPIOPin::setup() { this->pin_mode(this->flags_); }
void WaveshareIOCH32V003GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool WaveshareIOCH32V003GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) ^ this->inverted_; }

void WaveshareIOCH32V003GPIOPin::digital_write(bool value) {
  this->parent_->digital_write(this->pin_, value ^ this->inverted_);
}

size_t WaveshareIOCH32V003GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "EXIO%u via WaveshareIO", this->pin_);
}

}  // namespace esphome::waveshare_io_ch32v003
