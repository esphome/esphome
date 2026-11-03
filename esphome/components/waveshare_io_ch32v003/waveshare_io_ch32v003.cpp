#include "waveshare_io_ch32v003.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome::waveshare_io_ch32v003 {

static const uint8_t IO_EXTENSION_DIRECTION = 0x02;
static const uint8_t IO_EXTENSION_IO_OUTPUT_ADDR = 0x03;
static const uint8_t IO_EXTENSION_IO_INPUT_ADDR = 0x04;
static const uint8_t IO_EXTENSION_PWM_ADDR = 0x05;
static const uint8_t IO_EXTENSION_ADC_ADDR = 0x06;
static const uint8_t IO_EXTENSION_RTC_INT_ADDR = 0x07;

ESPHOME_LOG_TAG(TAG, "waveshare_io_ch32v003");

void WaveshareIOCH32V003Component::setup() {
  this->mode_mask_ = 0xFF;    // Set all pins to output mode
  this->output_mask_ = 0xFF;  // Set all pins to high (output mode)

  bool step1 = this->write_gpio_modes_();
  bool step2 = this->write_gpio_outputs_();

  if (!step1 || !step2) {
    this->mark_failed(LOG_STR("Failed to initialize Waveshare IO expander"));
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
}

uint16_t WaveshareIOCH32V003Component::get_adc_value() {
  if (this->is_failed())
    return 0;

  uint16_t adc_value;
  if (!this->status_comm_result(this->read_byte_16_le(IO_EXTENSION_ADC_ADDR, &adc_value),
                                LOG_STR("Failed to read ADC register")))
    return 0;
  return adc_value;
}

uint8_t WaveshareIOCH32V003Component::get_rtc_interrupt_status() {
  if (this->is_failed())
    return 0;

  uint8_t data = 0;
  if (!this->status_comm_result(this->read_bytes(IO_EXTENSION_RTC_INT_ADDR, &data, 1),
                                LOG_STR("Failed to read RTC interrupt register")))
    return 0;
  return data;
}

void WaveshareIOCH32V003Component::set_pwm_value(uint8_t value) {
  if (this->is_failed())
    return;

  // The safe PWM range is enforced by write_pwm_level(), as in the original Waveshare IO
  // library function "void IO_EXTENSION_Pwm_Output(uint8_t Value)".
  this->status_comm_result(this->write_byte(IO_EXTENSION_PWM_ADDR, value), LOG_STR("Failed to set PWM duty cycle"));
}

void WaveshareIOCH32V003Component::write_pwm_level(float state, uint8_t min_value, uint8_t max_value) {
  auto pwm_value = static_cast<uint8_t>(state * 255.0f);
  uint8_t final_pwm_value = std::clamp(pwm_value, min_value, max_value);
  if (final_pwm_value != pwm_value) {
    ESP_LOGVV(TAG, "Clamping PWM value %u to safe range [%u, %u]", pwm_value, min_value, max_value);
  }
  this->set_pwm_value(final_pwm_value);
}

bool WaveshareIOCH32V003Component::write_gpio_modes_() {
  if (this->is_failed())
    return false;
  return this->status_comm_result(this->write_byte(IO_EXTENSION_DIRECTION, this->mode_mask_),
                                  LOG_STR("Failed to write mode register"));
}

bool WaveshareIOCH32V003Component::write_gpio_outputs_() {
  if (this->is_failed())
    return false;
  return this->status_comm_result(this->write_byte(IO_EXTENSION_IO_OUTPUT_ADDR, this->output_mask_),
                                  LOG_STR("Failed to write output register"));
}

bool WaveshareIOCH32V003Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;

  uint8_t data = 0;
  if (!this->status_comm_result(this->read_bytes(IO_EXTENSION_IO_INPUT_ADDR, &data, 1),
                                LOG_STR("Failed to read input register")))
    return false;
  this->input_mask_ = data;
  return true;
}

void WaveshareIOCH32V003Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  assign_bits(this->output_mask_, 1 << pin, value);

  uint8_t data = this->output_mask_;
  this->status_comm_result(this->write_byte(IO_EXTENSION_IO_OUTPUT_ADDR, data),
                           LOG_STR("Failed to write output register"));
}

bool WaveshareIOCH32V003Component::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }
float WaveshareIOCH32V003Component::get_setup_priority() const { return setup_priority::IO; }

}  // namespace esphome::waveshare_io_ch32v003

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::waveshare_io_ch32v003::WaveshareIOCH32V003Component, "EXIO%u via WaveshareIO")
