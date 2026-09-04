#include "serial_proxy.h"

#ifdef USE_SERIAL_PROXY

#include "esphome/core/log.h"

#include <cinttypes>
#include "esphome/core/util.h"

#ifdef USE_API
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#endif

#ifdef USE_SERIAL_PROXY_USB_INFO
#include "esphome/components/usb_uart/usb_uart.h"
#endif

namespace esphome::serial_proxy {

static const char *const TAG = "serial_proxy";

void SerialProxy::setup() {
  // Set up modem control pins if configured
  if (this->rts_pin_ != nullptr) {
    this->rts_pin_->setup();
    this->rts_pin_->digital_write(this->rts_state_);
  }
  if (this->dtr_pin_ != nullptr) {
    this->dtr_pin_->setup();
    this->dtr_pin_->digital_write(this->dtr_state_);
  }
#ifdef USE_API
  // instance_index_ is fixed at registration time; pre-set it so loop() only needs to update data
  this->outgoing_msg_.instance = this->instance_index_;
#endif
#ifdef USE_SERIAL_PROXY_TAP
  // A tap sets itself up before this runs (its setup priority is higher), so it may
  // already be waiting on the port -- a boot-time handshake with the device, say. Leaving
  // the loop enabled is what lets that finish; without it the tap would stall until a
  // client happened to subscribe.
  if (this->tap_ != nullptr && this->tap_->tap_needs_port()) {
    return;
  }
#endif
  // No subscriber at startup; disable loop until a client subscribes
  this->disable_loop();
}

#ifdef USE_SERIAL_PROXY_TAP
void SerialProxy::reset_mode_() {
  // The mode belongs to a session, not to the port. Carrying a departed client's choice
  // over to the next one would inject protocol bytes into a stream that never asked for
  // them -- a firmware upload, or any client built before this request existed and so
  // unable to turn it off. Guessing RAW is the safe direction: a client that wanted
  // protocol handling and did not ask for it merely sends its own acknowledgements.
  if (this->mode_ == api::enums::SERIAL_PROXY_MODE_RAW) {
    return;
  }
  ESP_LOGD(TAG, "Session ended, returning serial proxy [%" PRIu32 "] to RAW mode", this->instance_index_);
  this->mode_ = api::enums::SERIAL_PROXY_MODE_RAW;
}
#endif

void SerialProxy::loop() {
#ifdef USE_API
  // Detect subscriber disconnect
  if (this->api_connection_ != nullptr && (this->api_connection_->is_marked_for_removal() ||
                                           !this->api_connection_->is_connection_setup() || !api_is_connected())) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->api_connection_ = nullptr;
    this->reset_mode_();
  }

  // With no subscriber there is normally nothing to do, but a tap may still need the port
  // read -- it does its protocol work precisely while nobody else is listening.
  if (this->api_connection_ == nullptr) [[unlikely]] {
#ifdef USE_SERIAL_PROXY_TAP
    if (this->tap_ == nullptr || !this->tap_->tap_needs_port()) {
      this->disable_loop();
      return;
    }
#else
    this->disable_loop();
    return;
#endif
  }

  // Read available data from UART and forward to subscribed client
  size_t available = this->available();
  if (available == 0)
    return;

  this->read_and_send_(available);
#endif
}

#ifdef USE_API
void __attribute__((noinline)) SerialProxy::read_and_send_(size_t available) {
  // Read in chunks up to SERIAL_PROXY_MAX_READ_SIZE
  uint8_t buffer[SERIAL_PROXY_MAX_READ_SIZE];
  size_t to_read = std::min(available, sizeof(buffer));

  if (!this->read_array(buffer, to_read))
    return;

#ifdef USE_SERIAL_PROXY_TAP
  // Before forwarding, so a tap that answers the device (an acknowledgement, say) is not
  // waiting on the network round trip to a subscriber that may not even exist.
  if (this->tap_observing_()) {
    this->tap_->on_device_rx(buffer, to_read);
  }
#endif

  if (this->api_connection_ == nullptr) {
    return;
  }
  this->outgoing_msg_.set_data(buffer, to_read);
  this->api_connection_->send_serial_proxy_data(this->outgoing_msg_);
}
#endif

#ifdef USE_SERIAL_PROXY_TAP

bool SerialProxy::tap_observing_() const {
  if (this->tap_ == nullptr) {
    return false;
  }
  // With no subscriber, a tap doing its own protocol work (the boot-time handshake with
  // the device, say) is served regardless of mode -- nobody has chosen one yet. Once a
  // subscriber holds the port, the mode alone decides, so RAW stays inert.
  if (this->api_connection_ == nullptr && this->tap_->tap_needs_port()) {
    return true;
  }
  // Otherwise the mode decides. RAW must be inert: a client that flips to RAW before
  // flashing firmware is entitled to a byte pipe with nothing injecting protocol bytes
  // into it, and "the tap turned out not to recognise the stream" is not good enough.
  return this->mode_ == api::enums::SERIAL_PROXY_MODE_PROTOCOL;
}

void SerialProxy::tap_pump() {
#ifdef USE_API
  // Nothing would consume the bytes; leave them in the FIFO
  if (!this->tap_observing_() && this->api_connection_ == nullptr) {
    return;
  }
  const size_t available = this->available();
  if (available > 0) {
    this->read_and_send_(available);
  }
#endif
}
#endif

void SerialProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Serial Proxy [%" PRIu32 "]:\n"
                "  Name: %s\n"
                "  Port Type: %s\n"
                "  RTS Pin: %s\n"
                "  DTR Pin: %s",
                this->instance_index_, this->name_ != nullptr ? this->name_ : "",
                this->port_type_ == api::enums::SERIAL_PROXY_PORT_TYPE_RS485        ? LOG_STR_LITERAL("RS485")
                : this->port_type_ == api::enums::SERIAL_PROXY_PORT_TYPE_RS232      ? LOG_STR_LITERAL("RS232")
                : this->port_type_ == api::enums::SERIAL_PROXY_PORT_TYPE_USB_SERIAL ? LOG_STR_LITERAL("USB_SERIAL")
                                                                                    : LOG_STR_LITERAL("TTL"),
                this->rts_pin_ != nullptr ? LOG_STR_LITERAL("configured") : LOG_STR_LITERAL("not configured"),
                this->dtr_pin_ != nullptr ? LOG_STR_LITERAL("configured") : LOG_STR_LITERAL("not configured"));
}

SerialProxyResult SerialProxy::configure(api::APIConnection *api_connection, uint32_t baudrate, bool flow_control,
                                         uint8_t parity, uint8_t stop_bits, uint8_t data_size) {
#ifdef USE_API
  if (!this->is_subscriber_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring configure request from client without port subscription [%" PRIu32 "]",
             this->instance_index_);
    return SerialProxyResult::SERIAL_PROXY_RESULT_PORT_IN_USE;
  }
#endif
  ESP_LOGD(TAG,
           "Configuring serial proxy [%" PRIu32 "]: baud=%" PRIu32 ", flow_ctrl=%s, parity=%" PRIu8 ", stop=%" PRIu8
           ", data=%" PRIu8,
           this->instance_index_, baudrate, YESNO(flow_control), parity, stop_bits, data_size);

  auto *uart_comp = this->parent_;
  if (uart_comp == nullptr) {
    ESP_LOGE(TAG, "UART component not available");
    return SerialProxyResult::SERIAL_PROXY_RESULT_ERROR;
  }

  // Validate all parameters before applying any (values come from a remote client)
  if (baudrate == 0) {
    ESP_LOGW(TAG, "Invalid baud rate: 0");
    return SerialProxyResult::SERIAL_PROXY_RESULT_INVALID_ARGUMENT;
  }
  if (stop_bits < 1 || stop_bits > 2) {
    ESP_LOGW(TAG, "Invalid stop bits: %u (must be 1 or 2)", stop_bits);
    return SerialProxyResult::SERIAL_PROXY_RESULT_INVALID_ARGUMENT;
  }
  if (data_size < 5 || data_size > 8) {
    ESP_LOGW(TAG, "Invalid data bits: %u (must be 5-8)", data_size);
    return SerialProxyResult::SERIAL_PROXY_RESULT_INVALID_ARGUMENT;
  }
  if (parity > 2) {
    ESP_LOGW(TAG, "Invalid parity: %u (must be 0-2)", parity);
    return SerialProxyResult::SERIAL_PROXY_RESULT_INVALID_ARGUMENT;
  }
  if (flow_control) {
    ESP_LOGW(TAG, "Hardware flow control requested but is not yet supported");
    return SerialProxyResult::SERIAL_PROXY_RESULT_NOT_SUPPORTED;
  }

  // Skip a no-op reconfigure. Clients routinely re-send identical settings on every
  // port open, and on a USB UART each apply is a CDC SET_LINE_CODING control transfer.
  // Some bridges watch line-coding changes as a signalling channel (a magic baud
  // sequence to enter a bootloader, say), so redundant applies are not harmless.
  static const uart::UARTParityOptions PARITY_MAP[] = {
      uart::UART_CONFIG_PARITY_NONE,
      uart::UART_CONFIG_PARITY_EVEN,
      uart::UART_CONFIG_PARITY_ODD,
  };
  if (uart_comp->get_baud_rate() == baudrate && uart_comp->get_stop_bits() == stop_bits &&
      uart_comp->get_data_bits() == data_size && uart_comp->get_parity() == PARITY_MAP[parity]) {
    ESP_LOGV(TAG, "Settings unchanged, skipping reconfigure [%" PRIu32 "]", this->instance_index_);
    return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
  }

  // Apply validated parameters
  uart_comp->set_baud_rate(baudrate);
  uart_comp->set_stop_bits(stop_bits);
  uart_comp->set_data_bits(data_size);

  uart_comp->set_parity(PARITY_MAP[parity]);

  // load_settings() is available on ESP8266 and ESP32 platforms
#if defined(USE_ESP8266) || defined(USE_ESP32)
  uart_comp->load_settings(true);
#endif
  return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
}

SerialProxyResult SerialProxy::set_mode_from_client(api::APIConnection *api_connection,
                                                    api::enums::SerialProxyMode mode) {
#ifdef USE_API
  // Only the live subscriber may change the mode, so the mode cannot outlive a session
  if (!this->is_subscriber_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring mode request from client without port subscription [%" PRIu32 "]", this->instance_index_);
    return SerialProxyResult::SERIAL_PROXY_RESULT_PORT_IN_USE;
  }
#endif
  // Values come from a remote client
  if (mode != api::enums::SERIAL_PROXY_MODE_RAW && mode != api::enums::SERIAL_PROXY_MODE_PROTOCOL) {
    ESP_LOGW(TAG, "Invalid mode: %" PRIu32, static_cast<uint32_t>(mode));
    return SerialProxyResult::SERIAL_PROXY_RESULT_INVALID_ARGUMENT;
  }
  // PROTOCOL on a port with no tap would be a silent no-op; refuse so the client knows
#ifdef USE_SERIAL_PROXY_TAP
  const bool has_tap = this->tap_ != nullptr;
#else
  const bool has_tap = false;
#endif
  if (mode == api::enums::SERIAL_PROXY_MODE_PROTOCOL && !has_tap) {
    ESP_LOGW(TAG, "No tap on serial proxy [%" PRIu32 "]; PROTOCOL mode unavailable", this->instance_index_);
    return SerialProxyResult::SERIAL_PROXY_RESULT_NOT_SUPPORTED;
  }
  ESP_LOGD(TAG, "Serial proxy [%" PRIu32 "] mode set to %s", this->instance_index_,
           mode == api::enums::SERIAL_PROXY_MODE_PROTOCOL ? LOG_STR_LITERAL("PROTOCOL") : LOG_STR_LITERAL("RAW"));
#ifdef USE_SERIAL_PROXY_TAP
  const bool leaving_protocol_mode =
      this->mode_ != api::enums::SERIAL_PROXY_MODE_RAW && mode == api::enums::SERIAL_PROXY_MODE_RAW;
  this->mode_ = mode;

  // Only for an explicit client request, not for reset_mode_() at the end of a session:
  // an ordinary disconnect says nothing about the device, whereas a client deliberately
  // asking for raw bytes usually precedes changing what the device is.
  if (leaving_protocol_mode && this->tap_ != nullptr) {
    this->tap_->on_protocol_disabled();
  }
#endif
  return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
}

void SerialProxy::write_from_client(api::APIConnection *api_connection, const uint8_t *data, size_t len) {
#ifdef USE_API
  // Bytes from anyone but the live subscriber would interleave with the subscriber's
  // traffic -- or with an active tap's -- on the wire
  if (!this->is_subscriber_(api_connection)) {
    if (this->api_connection_ != nullptr) {
      ESP_LOGW(TAG, "Ignoring write from client that does not hold serial proxy [%" PRIu32 "]", this->instance_index_);
    } else {
      // A legacy client streaming writes without subscribing would flood WARN, one per
      // request; writes are the only high-rate, unacknowledged operation, so keep this
      // visible without drowning the log
      ESP_LOGV(TAG, "Ignoring write from client without port subscription [%" PRIu32 "]", this->instance_index_);
    }
    return;
  }
#endif
  if (data == nullptr || len == 0)
    return;
  this->write_array(data, len);

#ifdef USE_SERIAL_PROXY_TAP
  // After the write, so the tap observes the same ordering the device does
  if (this->tap_observing_()) {
    this->tap_->on_client_tx(data, len);
  }
#endif
}

SerialProxyResult SerialProxy::set_modem_pins(api::APIConnection *api_connection, uint32_t line_states) {
#ifdef USE_API
  if (!this->is_subscriber_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring modem pin request from client without port subscription [%" PRIu32 "]",
             this->instance_index_);
    return SerialProxyResult::SERIAL_PROXY_RESULT_PORT_IN_USE;
  }
#endif
  // Asserting a pin that is not configured must fail so the client learns the signal never
  // reached the wire; deasserting an absent pin is harmless and stays allowed. Clients can
  // avoid this by masking against SerialProxyInfo.configured_line_states.
  if ((line_states & ~this->get_configured_modem_pins()) != 0) {
    ESP_LOGW(TAG, "Requested modem pin not configured on serial proxy [%" PRIu32 "]", this->instance_index_);
    return SerialProxyResult::SERIAL_PROXY_RESULT_NOT_SUPPORTED;
  }
  const bool rts = (line_states & SERIAL_PROXY_LINE_STATE_FLAG_RTS) != 0;
  const bool dtr = (line_states & SERIAL_PROXY_LINE_STATE_FLAG_DTR) != 0;
  ESP_LOGV(TAG, "Setting modem pins [%" PRIu32 "]: RTS=%s, DTR=%s", this->instance_index_, ONOFF(rts), ONOFF(dtr));

  if (this->rts_pin_ != nullptr) {
    this->rts_state_ = rts;
    this->rts_pin_->digital_write(rts);
  }
  if (this->dtr_pin_ != nullptr) {
    this->dtr_state_ = dtr;
    this->dtr_pin_->digital_write(dtr);
  }
  return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
}

#if defined(USE_SERIAL_PROXY_USB_INFO) && defined(USE_API)
void SerialProxy::get_usb_info(usb_host::UsbDeviceInfo &info, api::SerialProxyGetUsbInfoResponse &resp) const {
  if (this->usb_channel_ == nullptr) {
    resp.status = api::enums::SERIAL_PROXY_STATUS_NOT_SUPPORTED;
    return;
  }
  resp.interface_number = this->usb_channel_->get_index();
  if (!this->usb_channel_->get_parent()->get_device_info(info)) {
    // No device attached right now; not an error
    return;
  }
  resp.connected = true;
  resp.vendor_id = info.vendor_id;
  resp.product_id = info.product_id;
  resp.bcd_device = info.bcd_device;
  resp.manufacturer = StringRef(info.manufacturer);
  resp.product = StringRef(info.product);
  resp.serial_number = StringRef(info.serial_number);
}
#endif

uint32_t SerialProxy::get_modem_pins() const {
  return (this->rts_state_ ? static_cast<uint32_t>(SERIAL_PROXY_LINE_STATE_FLAG_RTS) : 0u) |
         (this->dtr_state_ ? static_cast<uint32_t>(SERIAL_PROXY_LINE_STATE_FLAG_DTR) : 0u);
}

SerialProxyResult SerialProxy::flush_port(api::APIConnection *api_connection) {
#ifdef USE_API
  // Flushing stalls the port, so it gets the same ownership check as writes
  if (!this->is_subscriber_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring flush from client without port subscription [%" PRIu32 "]", this->instance_index_);
    return SerialProxyResult::SERIAL_PROXY_RESULT_PORT_IN_USE;
  }
#endif
  ESP_LOGV(TAG, "Flushing serial proxy [%" PRIu32 "]", this->instance_index_);
  switch (this->flush()) {
    case uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS:
      return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
    case uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS:
      return SerialProxyResult::SERIAL_PROXY_RESULT_ASSUMED_SUCCESS;
    case uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT:
      return SerialProxyResult::SERIAL_PROXY_RESULT_TIMEOUT;
    case uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED:
      return SerialProxyResult::SERIAL_PROXY_RESULT_ERROR;
  }
  return SerialProxyResult::SERIAL_PROXY_RESULT_ERROR;  // Unreachable; all enum values handled above
}

#ifdef USE_API
SerialProxyResult SerialProxy::serial_proxy_request(api::APIConnection *api_connection,
                                                    api::enums::SerialProxyRequestType type) {
  switch (type) {
    case api::enums::SERIAL_PROXY_REQUEST_TYPE_SUBSCRIBE:
      if (this->api_connection_ == api_connection) {
        ESP_LOGV(TAG, "API connection is already subscribed to serial proxy [%" PRIu32 "]", this->instance_index_);
        return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
      }
      if (this->api_connection_ != nullptr) {
        // A living subscriber keeps exclusive access. Its connection may be dead without
        // loop() having noticed yet (e.g. the client crashed and reconnected quickly);
        // in that case let the new client take over instead of locking it out.
        if (this->api_connection_->is_connection_setup()) {
          ESP_LOGE(TAG, "Only one API subscription is allowed at a time");
          return SerialProxyResult::SERIAL_PROXY_RESULT_PORT_IN_USE;
        }
        ESP_LOGW(TAG, "Previous subscriber disconnected; taking over subscription");
        // End the dead client's session before starting the new one, so its mode
        // cannot leak into a session that never asked for it
        this->api_connection_ = nullptr;
        this->reset_mode_();
      }
      this->api_connection_ = api_connection;
      this->enable_loop();
      ESP_LOGV(TAG, "API connection subscribed to serial proxy [%" PRIu32 "]", this->instance_index_);
      return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
    case api::enums::SERIAL_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      // Unsubscribe is idempotent: not being subscribed is not an error
      if (this->api_connection_ != api_connection) {
        ESP_LOGV(TAG, "API connection is not subscribed to serial proxy [%" PRIu32 "]", this->instance_index_);
        return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
      }
      this->api_connection_ = nullptr;
      this->reset_mode_();
#ifdef USE_SERIAL_PROXY_TAP
      // Keep the loop alive for a tap that still needs the port (mirrors loop())
      if (this->tap_ == nullptr || !this->tap_->tap_needs_port()) {
        this->disable_loop();
      }
#else
      this->disable_loop();
#endif
      ESP_LOGV(TAG, "API connection unsubscribed from serial proxy [%" PRIu32 "]", this->instance_index_);
      return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
    default:
      ESP_LOGW(TAG, "Unknown serial proxy request type: %" PRIu32, static_cast<uint32_t>(type));
      return SerialProxyResult::SERIAL_PROXY_RESULT_NOT_SUPPORTED;
  }
}
#endif

}  // namespace esphome::serial_proxy

#endif  // USE_SERIAL_PROXY
