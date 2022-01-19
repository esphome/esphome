#pragma once

#ifdef USE_ARDUINO

// MideaUART
#include <Appliance/ApplianceBase.h>
#include <Helpers/Logger.h>

// Include global defines
#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"
#include "ir_transmitter.h"

namespace esphome {
namespace midea {

/* Stream from UART component */
class UARTStream : public Stream {
 public:
  void set_uart(uart::UARTComponent *uart) { this->uart_ = uart; }

  /* Stream interface implementation */

  int available() override { return this->uart_->available(); }
  int read() override {
    uint8_t data;
    this->uart_->read_byte(&data);
    return data;
  }
  int peek() override {
    uint8_t data;
    this->uart_->peek_byte(&data);
    return data;
  }
  size_t write(uint8_t data) override {
    this->uart_->write_byte(data);
    return 1;
  }
  size_t write(const uint8_t *data, size_t size) override {
    this->uart_->write_array(data, size);
    return size;
  }
  void flush() override { this->uart_->flush(); }

 protected:
  uart::UARTComponent *uart_;
};

template<typename T> class ApplianceBase : public Component {
  static_assert(std::is_base_of<dudanov::midea::ApplianceBase, T>::value,
                "T must derive from dudanov::midea::ApplianceBase class");

 public:
  ApplianceBase() {
    this->base_.setStream(&this->stream_);
    this->base_.addOnStateCallback(std::bind(&ApplianceBase::on_status_change, this));
    dudanov::midea::ApplianceBase::setLogger(
        [](int level, const char *tag, int line, const String &format, va_list args) {
          esp_log_vprintf_(level, tag, line, format.c_str(), args);
        });
  }

#ifdef USE_REMOTE_TRANSMITTER
  void set_transmitter(RemoteTransmitterBase *transmitter) { this->transmitter_.set_transmitter(transmitter); }
#endif

  /* UART communication */

  void set_uart_parent(uart::UARTComponent *parent) { this->stream_.set_uart(parent); }
  void set_period(uint32_t ms) { this->base_.setPeriod(ms); }
  void set_response_timeout(uint32_t ms) { this->base_.setTimeout(ms); }
  void set_request_attempts(uint32_t attempts) { this->base_.setNumAttempts(attempts); }

  /* Component methods */

  void setup() override { this->base_.setup(); }
  void loop() override { this->base_.loop(); }
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }
  bool can_proceed() override {
    return this->base_.getAutoconfStatus() != dudanov::midea::AutoconfStatus::AUTOCONF_PROGRESS;
  }

  void set_beeper_feedback(bool state) { this->base_.setBeeper(state); }
  void set_autoconf(bool value) { this->base_.setAutoconf(value); }
  virtual void on_status_change() = 0;

 protected:
  T base_;
  UARTStream stream_;
#ifdef USE_REMOTE_TRANSMITTER
  IrTransmitter transmitter_;
#endif
};

}  // namespace midea
}  // namespace esphome

#endif  // USE_ARDUINO
