#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "esphome/components/gpio_expander/cached_gpio.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "const.h"

namespace esphome::m5pm1 {

class M5PM1Component : public Component, public i2c::I2CDevice, public gpio_expander::CachedGpioExpander<uint8_t, 8> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_interrupt_gpio(uint8_t pin) { this->interrupt_gpio_ = pin; }
  bool is_interrupt_gpio(uint8_t pin) const { return this->interrupt_pin_ != nullptr && pin == this->interrupt_gpio_; }
  void set_gpio_interrupt_enabled(uint8_t pin, bool enabled);
  void set_power_interrupt_enabled(uint8_t mask);
  void set_system_interrupt_enabled(uint8_t mask);
  void set_pwm_frequency(uint16_t pwm_frequency) { this->pwm_frequency_ = pwm_frequency; }
  void set_single_click_timing(M5PM1ButtonTiming timing) { this->single_click_timing_ = timing; }
  void set_double_click_timing(M5PM1ButtonTiming timing) { this->double_click_timing_ = timing; }
  void set_long_press_timing(M5PM1LongPressTiming timing) { this->long_press_timing_ = timing; }
  void set_single_click_reset(bool enabled) { this->single_click_reset_ = enabled; }
  void set_double_click_poweroff(bool enabled) { this->double_click_poweroff_ = enabled; }
  void set_long_press_download(bool enabled) { this->long_press_download_ = enabled; }
  void set_single_click_interrupt_enabled(bool enabled);
  void set_double_click_interrupt_enabled(bool enabled);

  template<typename F> void add_on_power_interrupt_callback(F &&callback) {
    this->power_interrupt_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_system_interrupt_callback(F &&callback) {
    this->system_interrupt_callback_.add(std::forward<F>(callback));
  }

  void pin_mode(uint8_t pin, gpio::Flags flags);
  bool set_pin_function(uint8_t pin, M5PM1PinFunction function);
  bool set_pin_wakeup(uint8_t pin, bool enable, M5PM1WakeupEdge edge);
  bool set_pin_power_hold(uint8_t pin, bool enable);

  const M5PM1DeviceInfo &get_device_info() const { return this->device_info_; }
  bool read_power_source(uint8_t *value);
  bool read_wake_source(uint8_t *value);
  bool clear_wake_source(uint8_t mask);
  uint8_t get_power_config() const { return this->power_config_; }
  bool read_power_config(uint8_t *value);
  bool set_power_config(uint8_t mask, bool enable);
  uint8_t get_power_hold() const { return this->power_hold_; }
  bool read_power_hold(uint8_t *value);
  bool set_power_hold(uint8_t mask, bool enable);
  bool set_battery_low_voltage(uint16_t millivolts);
  bool set_i2c_sleep_timeout(uint8_t seconds);

  bool set_watchdog_timeout(uint8_t seconds);
  bool feed_watchdog();
  bool system_command(M5PM1SystemCommand command);

  bool read_vref_voltage(uint16_t *millivolts);
  bool read_battery_voltage(uint16_t *millivolts);
  bool read_vin_voltage(uint16_t *millivolts);
  bool read_5vout_voltage(uint16_t *millivolts);
  bool read_adc(M5PM1ADCChannel channel, uint16_t *value);

  bool update_pwm_frequency(uint16_t frequency);
  bool get_pwm_frequency(uint16_t *frequency);
  bool set_pwm(M5PM1PWMChannel channel, uint16_t duty, M5PM1PWMPolarity polarity, bool enable);
  bool get_pwm(M5PM1PWMChannel channel, uint16_t *duty, M5PM1PWMPolarity *polarity, bool *enable);

  bool set_timer(uint32_t seconds, M5PM1TimerAction action);
  bool clear_timer();

  bool read_irq_status(M5PM1IRQStatus *status);
  bool clear_irq_status();
  bool set_irq_masks(const M5PM1IRQStatus &masks);
  bool get_irq_masks(M5PM1IRQStatus *masks);

  bool get_button_state(bool *pressed, bool *pressed_since_last_read);
  bool set_button_timing(M5PM1ButtonTiming single, M5PM1LongPressTiming long_press, M5PM1ButtonTiming double_click);
  bool set_single_click_reset_disabled(bool disabled);
  bool set_double_click_poweroff_disabled(bool disabled);
  bool set_download_mode_locked(bool locked);

  bool set_neopixel_count(uint8_t count);
  // single led
  bool write_neopixel(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
  // addressable led
  bool write_neopixels(const uint16_t *rgb565, size_t count);
  bool refresh_neopixels();

  bool set_aw8737a_pulse(uint8_t pin, uint8_t pulse_count, bool refresh);
  bool write_rtc_memory(uint8_t offset, const uint8_t *data, size_t length);
  bool read_rtc_memory(uint8_t offset, uint8_t *data, size_t length);

 protected:
  static void IRAM_ATTR gpio_intr(M5PM1Component *arg);

  bool digital_read_hw(uint8_t pin) override;
  bool digital_read_cache(uint8_t pin) override;
  void digital_write_hw(uint8_t pin, bool value) override;

  bool read_u16_(uint8_t reg, uint16_t *value);
  bool write_u16_(uint8_t reg, uint16_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);
  bool write_pin_field_(uint8_t pin, uint8_t reg0, uint8_t reg1, uint8_t value);
  bool read_device_info_();
  bool read_initial_state_();
  bool process_irq_();
  void dispatch_irq_events_();
  bool valid_pin_(uint8_t pin) const { return pin < M5PM1_GPIO_COUNT; }
  void set_warning_(const char *message);

  uint16_t pwm_frequency_{500};
  M5PM1ButtonTiming single_click_timing_{M5PM1ButtonTiming::M5PM1_BUTTON_TIMING_250_MS};
  M5PM1ButtonTiming double_click_timing_{M5PM1ButtonTiming::M5PM1_BUTTON_TIMING_250_MS};
  M5PM1LongPressTiming long_press_timing_{M5PM1LongPressTiming::M5PM1_LONG_PRESS_TIMING_2_S};
  bool single_click_reset_{true};
  bool double_click_poweroff_{true};
  bool long_press_download_{true};

  M5PM1DeviceInfo device_info_{};
  uint8_t mode_mask_{0};
  uint8_t output_mask_{0};
  uint8_t input_mask_{0};
  uint8_t drive_mask_{0x1F};
  uint8_t power_config_{0};
  uint8_t power_hold_{0};
  uint8_t wake_source_{0};
  // M5PM1 IRQ masks use 1 to disable an event. GPIO IRQs are enabled by input pins during code generation.
  M5PM1IRQStatus irq_masks_{0x1F, 0x3F, 0x07};
  InternalGPIOPin *interrupt_pin_{nullptr};
  uint8_t interrupt_gpio_{0xFF};
  // The ISR only marks work pending; all I2C reads, clears, and callbacks run in loop().
  volatile bool irq_pending_{false};
  // Keep a software copy until the hardware status has been cleared successfully.
  M5PM1IRQStatus pending_irq_status_{};
  uint8_t pending_wake_source_{0};
  LazyCallbackManager<void(uint8_t)> power_interrupt_callback_;
  LazyCallbackManager<void(uint8_t, uint8_t)> system_interrupt_callback_;
};

class M5PM1GPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  // This getter is useful when setting Neopixel and AW8737A
  // since these two functionalities require the pin to be configured as output (push-pull/open-drain) first
  // before multiplex pins for alternative functions
  uint8_t get_pin() const { return this->pin_; }
  size_t dump_summary(char *buffer, size_t len) const override;
  gpio::Flags get_flags() const override { return this->flags_; }

  void set_parent(M5PM1Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
  void set_wakeup(bool wakeup) { this->wakeup_ = wakeup; }
  void set_wakeup_edge(M5PM1WakeupEdge wakeup_edge) { this->wakeup_edge_ = wakeup_edge; }

 protected:
  M5PM1Component *parent_{nullptr};
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_NONE};
  bool wakeup_{false};
  M5PM1WakeupEdge wakeup_edge_{M5PM1WakeupEdge::M5PM1_WAKEUP_EDGE_FALLING};
};

}  // namespace esphome::m5pm1
