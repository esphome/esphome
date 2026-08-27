#include "serial_proxy.h"

#ifdef USE_SERIAL_PROXY

#include "esphome/core/log.h"

#include <cinttypes>
#include "esphome/core/util.h"

#ifdef USE_API
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
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
  // No subscriber at startup; disable loop until a client subscribes
  this->disable_loop();
}

void SerialProxy::loop() {
#ifdef USE_API
  // Safety check — loop should only run when subscribed, but guard against races
  if (this->api_connection_ == nullptr) [[unlikely]] {
    this->disable_loop();
    return;
  }

  // Detect subscriber disconnect
  if (this->api_connection_->is_marked_for_removal() || !this->api_connection_->is_connection_setup() ||
      !api_is_connected()) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->api_connection_ = nullptr;
    this->disable_loop();
    return;
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

  this->outgoing_msg_.set_data(buffer, to_read);
  this->api_connection_->send_serial_proxy_data(this->outgoing_msg_);
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
                this->port_type_ == api::enums::SERIAL_PROXY_PORT_TYPE_RS485   ? "RS485"
                : this->port_type_ == api::enums::SERIAL_PROXY_PORT_TYPE_RS232 ? "RS232"
                                                                               : "TTL",
                this->rts_pin_ != nullptr ? "configured" : "not configured",
                this->dtr_pin_ != nullptr ? "configured" : "not configured");
}

SerialProxyResult SerialProxy::configure(api::APIConnection *api_connection, uint32_t baudrate, bool flow_control,
                                         uint8_t parity, uint8_t stop_bits, uint8_t data_size) {
#ifdef USE_API
  if (this->port_claimed_by_other_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring configure request from client without port access [%" PRIu32 "]", this->instance_index_);
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

  // Apply validated parameters
  uart_comp->set_baud_rate(baudrate);
  uart_comp->set_stop_bits(stop_bits);
  uart_comp->set_data_bits(data_size);

  // Map parity value to UARTParityOptions
  static const uart::UARTParityOptions PARITY_MAP[] = {
      uart::UART_CONFIG_PARITY_NONE,
      uart::UART_CONFIG_PARITY_EVEN,
      uart::UART_CONFIG_PARITY_ODD,
  };
  uart_comp->set_parity(PARITY_MAP[parity]);

  // load_settings() is available on ESP8266 and ESP32 platforms
#if defined(USE_ESP8266) || defined(USE_ESP32)
  uart_comp->load_settings(true);
#endif
  return SerialProxyResult::SERIAL_PROXY_RESULT_OK;
}

void SerialProxy::write_from_client(api::APIConnection *api_connection, const uint8_t *data, size_t len) {
#ifdef USE_API
  // Bytes from a client other than the live subscriber would interleave with the
  // subscriber's traffic on the wire
  if (this->port_claimed_by_other_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring write from client without port access [%" PRIu32 "]", this->instance_index_);
    return;
  }
#endif
  if (data == nullptr || len == 0)
    return;
  this->write_array(data, len);
}

SerialProxyResult SerialProxy::set_modem_pins(api::APIConnection *api_connection, uint32_t line_states) {
#ifdef USE_API
  if (this->port_claimed_by_other_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring modem pin request from client without port access [%" PRIu32 "]", this->instance_index_);
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

uint32_t SerialProxy::get_modem_pins() const {
  return (this->rts_state_ ? static_cast<uint32_t>(SERIAL_PROXY_LINE_STATE_FLAG_RTS) : 0u) |
         (this->dtr_state_ ? static_cast<uint32_t>(SERIAL_PROXY_LINE_STATE_FLAG_DTR) : 0u);
}

SerialProxyResult SerialProxy::flush_port(api::APIConnection *api_connection) {
#ifdef USE_API
  // Flushing stalls the port, so it gets the same ownership check as writes
  if (this->port_claimed_by_other_(api_connection)) {
    ESP_LOGW(TAG, "Ignoring flush from client without port access [%" PRIu32 "]", this->instance_index_);
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
bool SerialProxy::port_claimed_by_other_(api::APIConnection *api_connection) const {
  return this->api_connection_ != nullptr && this->api_connection_ != api_connection &&
         this->api_connection_->is_connection_setup();
}

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
      this->disable_loop();
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
