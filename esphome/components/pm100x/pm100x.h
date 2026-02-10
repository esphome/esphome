#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pm100x {

enum class PM100XModel {
  PM1003,
  PM1006,
  PM1006K,
};

// Base class without UART dependency (for PWM-only mode)
class PM100XComponent : public PollingComponent {
 public:
  PM100XComponent() = default;

  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { this->pm_2_5_sensor_ = pm_2_5_sensor; }
  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0_sensor) { this->pm_1_0_sensor_ = pm_1_0_sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { this->pm_10_0_sensor_ = pm_10_0_sensor; }
  void set_pwm_sensor(sensor::Sensor *pwm_sensor) { this->pwm_sensor_ = pwm_sensor; }
  void set_model(PM100XModel model) { this->model_ = model; }
  void set_startup_delay(uint32_t delay_ms) { this->startup_delay_ms_ = delay_ms; }
  void setup() override;
  void dump_config() override;
  virtual void loop() override;
  virtual void update() override;

  float get_setup_priority() const override;

  // Virtual methods for UART access (implemented by subclass)
  virtual bool has_uart() const { return false; }
  virtual void write_uart_array(const uint8_t *data, size_t len) {}
  virtual bool read_uart_byte(uint8_t *byte) { return false; }
  virtual int uart_available() { return 0; }
  virtual void check_uart_baud_rate(uint32_t baud_rate) {}

 protected:
  optional<bool> check_byte_() const;
  void parse_data_();
  void handle_pwm_state_(float duty_percent);
  const uint8_t *get_response_header_(size_t &length) const;
  const uint8_t *get_command_measure_(size_t &length) const;
  size_t get_frame_data_length_() const;
  float duty_to_pm25_(float duty_percent) const;
  uint8_t pm100x_checksum_(const uint8_t *command_data, size_t length) const;
  uint16_t get_16_bit_uint_(uint8_t start_index) const {
    return encode_uint16(this->data_[start_index], this->data_[start_index + 1]);
  }

  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *pwm_sensor_{nullptr};

  uint8_t data_[20];
  uint8_t data_index_{0};
  uint32_t start_time_{0};
  bool initial_delay_done_{false};
  uint32_t startup_delay_ms_{15000};
  PM100XModel model_{PM100XModel::PM1003};
};

// UART-enabled subclass (for UART-only mode)
class PM100XComponentUART : public PM100XComponent, public uart::UARTDevice {
 public:
  void loop() override { PM100XComponent::loop(); }
  void update() override { PM100XComponent::update(); }

 protected:
  bool has_uart() const override { return this->parent_ != nullptr; }
  void write_uart_array(const uint8_t *data, size_t len) override { this->write_array(data, len); }
  bool read_uart_byte(uint8_t *byte) override { return this->read_byte(byte); }
  int uart_available() override { return this->available(); }
  void check_uart_baud_rate(uint32_t baud_rate) override { this->check_uart_settings(baud_rate); }
};

}  // namespace esphome::pm100x
