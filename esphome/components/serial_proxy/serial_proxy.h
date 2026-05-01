#pragma once

// WARNING: This component is EXPERIMENTAL. The API may change at any time
// without following the normal breaking changes policy. Use at your own risk.
// Once the API is considered stable, this warning will be removed.

#include "esphome/core/defines.h"

#ifdef USE_SERIAL_PROXY

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"

// Include api_pb2.h only when the API is enabled. The full include is needed
// to hold SerialProxyDataReceived by value as a pre-allocated member.
// Guarding prevents pulling conflicting Zephyr logging macro names into
// translation units that include this header without USE_API defined.
#ifdef USE_API
#include "esphome/components/api/api_pb2.h"
#endif

// Forward-declare types needed outside the USE_API guard.
namespace esphome::api {
class APIConnection;
namespace enums {
enum SerialProxyPortType : uint32_t;
enum SerialProxyRequestType : uint32_t;
}  // namespace enums
}  // namespace esphome::api

namespace esphome::serial_proxy {

/// Bit flags for the line_states field exchanged with API clients.
/// Bit positions are stable API — new signals must use the next available bit.
enum SerialProxyLineStateFlag : uint32_t {
  SERIAL_PROXY_LINE_STATE_FLAG_RTS = 1 << 0,  ///< RTS (Request To Send)
  SERIAL_PROXY_LINE_STATE_FLAG_DTR = 1 << 1,  ///< DTR (Data Terminal Ready)
};

/// Maximum bytes to read from UART in a single loop iteration
inline constexpr size_t SERIAL_PROXY_MAX_READ_SIZE = 256;

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
  void set_name(const char *name) { this->name_ = name; }

  /// Get the human-readable port name
  const char *get_name() const { return this->name_; }

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

  /// Get the currently subscribed API connection (nullptr if none)
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  /// Handle a subscribe/unsubscribe request from an API client
  void serial_proxy_request(api::APIConnection *api_connection, api::enums::SerialProxyRequestType type);

  /// Write data received from an API client to the serial device
  /// @param data Pointer to data buffer
  /// @param len Number of bytes to write
  void write_from_client(const uint8_t *data, size_t len);

  /// Set modem pin states from a bitmask of SerialProxyLineStateFlag values
  void set_modem_pins(uint32_t line_states);

  /// Get current modem pin states as a bitmask of SerialProxyLineStateFlag values
  uint32_t get_modem_pins() const;

  /// Flush the serial port (block until all TX data is sent)
  uart::UARTFlushResult flush_port();

  /// Set the RTS GPIO pin (from YAML configuration)
  void set_rts_pin(GPIOPin *pin) { this->rts_pin_ = pin; }

  /// Set the DTR GPIO pin (from YAML configuration)
  void set_dtr_pin(GPIOPin *pin) { this->dtr_pin_ = pin; }

 protected:
#ifdef USE_API
  /// Read from UART and send to API client (slow path with 256-byte stack buffer)
  void read_and_send_(size_t available);
#endif

  /// Instance index for identifying this proxy in API messages
  uint32_t instance_index_{0};

  /// Subscribed API client (only one allowed at a time)
  api::APIConnection *api_connection_{nullptr};

#ifdef USE_API
  /// Pre-allocated outgoing message; instance field is set once in setup()
  api::SerialProxyDataReceived outgoing_msg_;
#endif

  /// Human-readable port name (points to a string literal in flash)
  const char *name_{nullptr};

  /// Port type
  api::enums::SerialProxyPortType port_type_{};

  /// Optional GPIO pins for modem control
  GPIOPin *rts_pin_{nullptr};
  GPIOPin *dtr_pin_{nullptr};

  /// Current modem pin states
  bool rts_state_{false};
  bool dtr_state_{false};
};

}  // namespace esphome::serial_proxy

#endif  // USE_SERIAL_PROXY
