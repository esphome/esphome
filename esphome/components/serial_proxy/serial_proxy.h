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
enum SerialProxyMode : uint32_t;
}  // namespace enums
}  // namespace esphome::api

namespace esphome::serial_proxy {

/// Bit flags for the line_states field exchanged with API clients.
/// Bit positions are stable API — new signals must use the next available bit.
enum SerialProxyLineStateFlag : uint32_t {
  SERIAL_PROXY_LINE_STATE_FLAG_RTS = 1 << 0,  ///< RTS (Request To Send)
  SERIAL_PROXY_LINE_STATE_FLAG_DTR = 1 << 1,  ///< DTR (Data Terminal Ready)
};

/// Result of a client-initiated operation; mapped to api::enums::SerialProxyStatus by the API layer
enum class SerialProxyResult : uint8_t {
  SERIAL_PROXY_RESULT_OK,                ///< Operation completed or request accepted
  SERIAL_PROXY_RESULT_ASSUMED_SUCCESS,   ///< Platform cannot confirm TX drain; success assumed
  SERIAL_PROXY_RESULT_PORT_IN_USE,       ///< Denied: another live client holds the port
  SERIAL_PROXY_RESULT_INVALID_ARGUMENT,  ///< A parameter value is out of range
  SERIAL_PROXY_RESULT_ERROR,             ///< Driver or hardware error
  SERIAL_PROXY_RESULT_TIMEOUT,           ///< Timed out before TX completed
  SERIAL_PROXY_RESULT_NOT_SUPPORTED,     ///< Requested feature is not available on this instance
};

/// Maximum bytes to read from UART in a single loop iteration
inline constexpr size_t SERIAL_PROXY_MAX_READ_SIZE = 256;

#ifdef USE_SERIAL_PROXY_TAP
/// Observes a port's traffic without owning it, and may inject bytes of its own.
///
/// This exists so protocol-aware behaviour can be layered onto a plain byte pipe without
/// the pipe knowing anything about the protocol: the tap is compiled in only when some
/// component asks for one, so a proxy carrying an RS485 meter pays nothing for it.
///
/// A tap is an observer, never a gatekeeper -- it cannot suppress or alter the bytes
/// flowing in either direction, so a misbehaving tap cannot corrupt the stream.
class SerialProxyTap {
 public:
  /// Bytes read from the device, before they are forwarded to any subscriber.
  virtual void on_device_rx(const uint8_t *data, size_t len) = 0;

  /// Bytes a subscriber sent towards the device, after they have been written.
  virtual void on_client_tx(const uint8_t *data, size_t len) = 0;

  /// True when the port must keep reading even with no subscriber attached, so a tap can
  /// do its own protocol work while nobody is listening. Honoured only while no
  /// subscriber holds the port; with one attached, the port mode alone decides.
  virtual bool tap_needs_port() const = 0;

  /// A client explicitly turned protocol handling off for this port. Distinct from the
  /// automatic reset when a session ends: this one means a client intends to do something
  /// else with the device -- reflash it, most likely -- so anything the tap believes about
  /// it should be treated as suspect.
  virtual void on_protocol_disabled() = 0;
};
#endif

class SerialProxy final : public uart::UARTDevice, public Component {
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

  /// Handle a mode change requested by an API client
  SerialProxyResult set_mode_from_client(api::APIConnection *api_connection, api::enums::SerialProxyMode mode);

  /// Configure UART parameters and apply them
  /// @param api_connection The API connection requesting the change
  /// @param baudrate Baud rate in bits per second
  /// @param flow_control True to enable hardware flow control
  /// @param parity Parity setting (0=none, 1=even, 2=odd)
  /// @param stop_bits Number of stop bits (1 or 2)
  /// @param data_size Number of data bits (5-8)
  SerialProxyResult configure(api::APIConnection *api_connection, uint32_t baudrate, bool flow_control, uint8_t parity,
                              uint8_t stop_bits, uint8_t data_size);

  /// Get the currently subscribed API connection (nullptr if none)
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  /// Handle a subscribe/unsubscribe request from an API client
  SerialProxyResult serial_proxy_request(api::APIConnection *api_connection, api::enums::SerialProxyRequestType type);

  /// Write data received from an API client to the serial device
  /// @param api_connection The API connection sending the data
  /// @param data Pointer to data buffer
  /// @param len Number of bytes to write
  void write_from_client(api::APIConnection *api_connection, const uint8_t *data, size_t len);

  /// Set modem pin states from a bitmask of SerialProxyLineStateFlag values
  SerialProxyResult set_modem_pins(api::APIConnection *api_connection, uint32_t line_states);

  /// Get current modem pin states as a bitmask of SerialProxyLineStateFlag values
  uint32_t get_modem_pins() const;

  /// Get the modem pins this instance can drive as a bitmask of SerialProxyLineStateFlag values
  uint32_t get_configured_modem_pins() const {
    return (this->rts_pin_ != nullptr ? static_cast<uint32_t>(SERIAL_PROXY_LINE_STATE_FLAG_RTS) : 0u) |
           (this->dtr_pin_ != nullptr ? static_cast<uint32_t>(SERIAL_PROXY_LINE_STATE_FLAG_DTR) : 0u);
  }

  /// Flush the serial port (block until all TX data is sent)
  /// @param api_connection The API connection requesting the flush
  SerialProxyResult flush_port(api::APIConnection *api_connection);

  /// Set the RTS GPIO pin (from YAML configuration)
  void set_rts_pin(GPIOPin *pin) { this->rts_pin_ = pin; }

  /// Set the DTR GPIO pin (from YAML configuration)
  void set_dtr_pin(GPIOPin *pin) { this->dtr_pin_ = pin; }

#ifdef USE_SERIAL_PROXY_TAP
  /// Attach a traffic observer. At most one, set once at setup time.
  void set_tap(SerialProxyTap *tap) { this->tap_ = tap; }

  /// Write bytes originating from the tap rather than from a client. Bypasses the
  /// subscriber ownership check, but only while the tap is being served bytes -- so a
  /// port in RAW mode with a subscriber attached stays inert. Returns false when the
  /// bytes were dropped for that reason.
  bool write_from_tap(const uint8_t *data, size_t len) {
    if (!this->tap_observing_()) {
      return false;
    }
    this->write_array(data, len);
    return true;
  }

  /// Whether the tap is currently being served bytes. Can flip false with no callback
  /// (a subscriber attaching in RAW mode, say), so a tap should check before starting
  /// protocol work and when a reply seems overdue.
  bool tap_is_observed() const { return this->tap_observing_(); }

  /// Resume reading after a tap's needs change. loop() disables itself when there is
  /// neither a subscriber nor a tap that wants the port, so a tap starting fresh work
  /// must ask for it back. Must be called from the main loop.
  void tap_request_port() { this->enable_loop(); }

  /// Whether the underlying device is present. On a USB UART this tracks enumeration, so
  /// a tap can notice the device being unplugged and plugged back in.
  bool is_device_connected() const { return this->parent_->is_connected(); }

  /// Run one read-and-dispatch cycle immediately. Lets a tap make progress before the
  /// main loop is running -- during setup, for instance, while a component is still
  /// blocking on can_proceed(). Must not be called from on_device_rx() or
  /// on_client_tx(): each nested cycle costs a 256-byte stack frame.
  void tap_pump();
#endif

 protected:
#ifdef USE_API
  /// Read from UART, hand the bytes to any tap, and forward them to a subscriber
  /// (slow path with a 256-byte stack buffer)
  void read_and_send_(size_t available);

  /// True when the given connection is the live subscriber. Every port operation
  /// (write, configure, modem pins, flush, mode) requires this, so an unsubscribed
  /// client can never share the wire with the subscriber or an active tap.
  bool is_subscriber_(api::APIConnection *api_connection) const { return this->api_connection_ == api_connection; }
#endif

#ifdef USE_SERIAL_PROXY_TAP
  /// Return the port to RAW when a subscriber goes away, so the mode never outlives it
  void reset_mode_();
#else
  /// Without a tap, PROTOCOL is refused, so the mode is fixed at RAW and there is
  /// nothing to reset
  void reset_mode_() {}
#endif

#ifdef USE_SERIAL_PROXY_TAP
  /// True when the tap should be shown the traffic passing through this port
  bool tap_observing_() const;
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

#ifdef USE_SERIAL_PROXY_TAP
  /// How the bytes passing through are treated; zero is SERIAL_PROXY_MODE_RAW
  api::enums::SerialProxyMode mode_{};
#endif

  /// Optional GPIO pins for modem control
  GPIOPin *rts_pin_{nullptr};
  GPIOPin *dtr_pin_{nullptr};

  /// Current modem pin states
  bool rts_state_{false};
  bool dtr_state_{false};

#ifdef USE_SERIAL_PROXY_TAP
  SerialProxyTap *tap_{nullptr};
#endif
};

}  // namespace esphome::serial_proxy

#endif  // USE_SERIAL_PROXY
