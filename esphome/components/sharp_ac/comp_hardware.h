#pragma once

#include <cstdarg>
#include <memory>

#include "esphome/components/button/button.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "core_frame.h"
#include "core_logic.h"
#include "core_messages.h"
#include "core_state.h"
#include "core_types.h"

namespace esphome::sharp_ac {

using climate::ClimateCall;
using climate::ClimateFanMode;
using climate::ClimateMode;
using climate::ClimatePreset;
using climate::ClimateSwingMode;
using climate::ClimateTraits;

class VaneSelectVertical;
class VaneSelectHorizontal;
class ConnectionStatusSensor;
class ReconnectButton;
class SharpAc;

class ESPHomeHardwareInterface : public SharpAcHardwareInterface {
 public:
  explicit ESPHomeHardwareInterface(uart::UARTDevice *uart_device) : uart_device_(uart_device) {}

  size_t read_array(uint8_t *data, size_t len) override { return this->uart_device_->read_array(data, len) ? len : 0; }

  size_t available() override { return this->uart_device_->available(); }

  void write_array(const uint8_t *data, size_t len) override { this->uart_device_->write_array(data, len); }

  uint8_t peek() override { return this->uart_device_->peek(); }

  uint8_t read() override { return this->uart_device_->read(); }

  uint32_t get_millis() override { return millis(); }

  void log_debug(const char *tag, const char *format, ...) override {
    va_list args;
    va_start(args, format);
    esp_log_vprintf_(ESPHOME_LOG_LEVEL_DEBUG, tag, 0, format, args);
    va_end(args);
  }

  void format_hex_pretty_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t len) override {
    esphome::format_hex_pretty_to(buffer, buffer_size, data, len, ' ');
  }

 private:
  uart::UARTDevice *uart_device_;
};

class ESPHomeStateCallback : public SharpAcStateCallback {
 public:
  explicit ESPHomeStateCallback(SharpAc *sharp_ac) : sharp_ac_(sharp_ac) {}

  void on_state_update() override;
  void on_ion_state_update(bool state) override;
  void on_vane_horizontal_update(SwingHorizontal val) override;
  void on_vane_vertical_update(SwingVertical val) override;
  void on_connection_status_update(int status) override;

 private:
  SharpAc *sharp_ac_;
};

class SharpAc : public climate::Climate, public uart::UARTDevice, public Component {
 public:
  SharpAc();

  void control(const climate::ClimateCall &call) override;
  void dump_config() override;
  void loop() override;
  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  esphome::climate::ClimateTraits traits() override;

  void set_ion(bool state);
  void set_vane_horizontal(SwingHorizontal val);
  void set_vane_vertical(SwingVertical val);

  void publish_update();

  void set_ion_switch(switch_::Switch *ion_switch) { this->ion_switch_ = ion_switch; }
  void set_vane_vertical_select(VaneSelectVertical *vane) { this->vane_vertical_ = vane; }
  void set_vane_horizontal_select(VaneSelectHorizontal *vane) { this->vane_horizontal_ = vane; }
  void set_connection_status_sensor(text_sensor::TextSensor *sensor) { this->connection_status_sensor_ = sensor; }
  void set_reconnect_button(button::Button *button) { this->reconnect_button_ = button; }

  void update_connection_status(int status);
  void trigger_reconnect();

 private:
  std::unique_ptr<ESPHomeHardwareInterface> hardware_interface_;
  std::unique_ptr<ESPHomeStateCallback> state_callback_;
  std::unique_ptr<SharpAcCore> core_;

  switch_::Switch *ion_switch_{nullptr};
  VaneSelectVertical *vane_vertical_{nullptr};
  VaneSelectHorizontal *vane_horizontal_{nullptr};
  text_sensor::TextSensor *connection_status_sensor_{nullptr};
  button::Button *reconnect_button_{nullptr};
};

}  // namespace esphome::sharp_ac
