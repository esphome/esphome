#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/core/defines.h"

#ifdef USE_SERIAL_PROXY

#include "esphome/components/api/api_pb2.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"

#include <string>

namespace esphome::serial_proxy {

/// Maximum bytes to read from UART in a single loop iteration
static constexpr size_t SERIAL_PROXY_MAX_READ_SIZE = 256;

class SerialProxy : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /// Get the instance index (position in Application's serial_proxies_ vector)
  uint32_t get_instance_index() const { return this->instance_index_; }

  /// Set the instance index (called by Application::register_serial_proxy)
  void set_instance_index(uint32_t index) { this->instance_index_ = index; }

  /// Set the human-readable port name (from YAML configuration)
  void set_name(const std::string &name) { this->name_ = name; }

  /// Get the human-readable port name
  const std::string &get_name() const { return this->name_; }

  /// Set the port type (from YAML configuration)
  void set_port_type(api::enums::SerialProxyPortType port_type) { this->port_type_ = port_type; }

  /// Get the port type
  api::enums::SerialProxyPortType get_port_type() const { return this->port_type_; }

  /// Configure UART parameters and apply them
  /// @param baudrate Baud rate in bits per second
  /// @param flow_control True to enable hardware flow control
  /// @param parity Parity setting (0=none, 1=even, 2=odd)
  /// @param stop_bits Number of stop bits (1 or 2)
  /// @param data_size Number of data bits (5-8)
  void configure(uint32_t baudrate, bool flow_control, uint8_t parity, uint8_t stop_bits, uint8_t data_size);

  /// Write data to the serial device
  /// @param data Pointer to data buffer
  /// @param len Number of bytes to write
  void write(const uint8_t *data, size_t len);

  /// Set modem pin states (RTS and DTR)
  /// @param rts Desired RTS pin state
  /// @param dtr Desired DTR pin state
  void set_modem_pins(bool rts, bool dtr);

  /// Get current modem pin states
  /// @param[out] rts Current RTS pin state
  /// @param[out] dtr Current DTR pin state
  void get_modem_pins(bool &rts, bool &dtr) const;

  /// Flush the serial port (block until all TX data is sent)
  void flush_port();

  /// Set the RTS GPIO pin (from YAML configuration)
  void set_rts_pin(GPIOPin *pin) { this->rts_pin_ = pin; }

  /// Set the DTR GPIO pin (from YAML configuration)
  void set_dtr_pin(GPIOPin *pin) { this->dtr_pin_ = pin; }

 protected:
  /// Instance index for identifying this proxy in API messages
  uint32_t instance_index_{0};

  /// Human-readable port name
  std::string name_;

  /// Port type
  api::enums::SerialProxyPortType port_type_{api::enums::SERIAL_PROXY_PORT_TYPE_TTL};

  /// Optional GPIO pins for modem control
  GPIOPin *rts_pin_{nullptr};
  GPIOPin *dtr_pin_{nullptr};

  /// Current modem pin states
  bool rts_state_{false};
  bool dtr_state_{false};
};

}  // namespace esphome::serial_proxy

#endif  // USE_SERIAL_PROXY
