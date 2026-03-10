#include "serial_proxy.h"

#ifdef USE_SERIAL_PROXY

#include "esphome/core/log.h"
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
}

void SerialProxy::loop() {
#ifdef USE_API
  // Detect subscriber disconnect
  if (this->api_connection_ != nullptr && (this->api_connection_->is_marked_for_removal() ||
                                           !this->api_connection_->is_connection_setup() || !api_is_connected())) {
    ESP_LOGW(TAG, "Subscriber disconnected");
    this->api_connection_ = nullptr;
  }

  if (this->api_connection_ == nullptr)
    return;

  // Read available data from UART and forward to subscribed client
  size_t available = this->available();
  if (available == 0)
    return;

  // Read in chunks up to SERIAL_PROXY_MAX_READ_SIZE
  uint8_t buffer[SERIAL_PROXY_MAX_READ_SIZE];
  size_t to_read = std::min(available, sizeof(buffer));

  if (!this->read_array(buffer, to_read))
    return;

  this->outgoing_msg_.set_data(buffer, to_read);
  this->api_connection_->send_serial_proxy_data(this->outgoing_msg_);
#endif
}

void SerialProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Serial Proxy [%u]:\n"
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

void SerialProxy::configure(uint32_t baudrate, bool flow_control, uint8_t parity, uint8_t stop_bits,
                            uint8_t data_size) {
  ESP_LOGD(TAG, "Configuring serial proxy [%u]: baud=%u, flow_ctrl=%s, parity=%u, stop=%u, data=%u",
           this->instance_index_, baudrate, YESNO(flow_control), parity, stop_bits, data_size);

  auto *uart_comp = this->parent_;
  if (uart_comp == nullptr) {
    ESP_LOGE(TAG, "UART component not available");
    return;
  }

  // Validate all parameters before applying any (values come from a remote client)
  if (baudrate == 0) {
    ESP_LOGW(TAG, "Invalid baud rate: 0");
    return;
  }
  if (stop_bits < 1 || stop_bits > 2) {
    ESP_LOGW(TAG, "Invalid stop bits: %u (must be 1 or 2)", stop_bits);
    return;
  }
  if (data_size < 5 || data_size > 8) {
    ESP_LOGW(TAG, "Invalid data bits: %u (must be 5-8)", data_size);
    return;
  }
  if (parity > 2) {
    ESP_LOGW(TAG, "Invalid parity: %u (must be 0-2)", parity);
    return;
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

  if (flow_control) {
    ESP_LOGW(TAG, "Hardware flow control requested but is not yet supported");
  }
}

void SerialProxy::write_from_client(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0)
    return;
  this->write_array(data, len);
}

void SerialProxy::set_modem_pins(uint32_t line_states) {
  const bool rts = (line_states & SERIAL_PROXY_LINE_STATE_FLAG_RTS) != 0;
  const bool dtr = (line_states & SERIAL_PROXY_LINE_STATE_FLAG_DTR) != 0;
  ESP_LOGV(TAG, "Setting modem pins [%u]: RTS=%s, DTR=%s", this->instance_index_, ONOFF(rts), ONOFF(dtr));

  if (this->rts_pin_ != nullptr) {
    this->rts_state_ = rts;
    this->rts_pin_->digital_write(rts);
  }
  if (this->dtr_pin_ != nullptr) {
    this->dtr_state_ = dtr;
    this->dtr_pin_->digital_write(dtr);
  }
}

uint32_t SerialProxy::get_modem_pins() const {
  return (this->rts_state_ ? SERIAL_PROXY_LINE_STATE_FLAG_RTS : 0u) |
         (this->dtr_state_ ? SERIAL_PROXY_LINE_STATE_FLAG_DTR : 0u);
}

uart::FlushResult SerialProxy::flush_port() {
  ESP_LOGV(TAG, "Flushing serial proxy [%u]", this->instance_index_);
  return this->flush();
}

#ifdef USE_API
void SerialProxy::serial_proxy_request(api::APIConnection *api_connection, api::enums::SerialProxyRequestType type) {
  switch (type) {
    case api::enums::SERIAL_PROXY_REQUEST_TYPE_SUBSCRIBE:
      if (this->api_connection_ != nullptr) {
        ESP_LOGE(TAG, "Only one API subscription is allowed at a time");
        return;
      }
      this->api_connection_ = api_connection;
      ESP_LOGV(TAG, "API connection subscribed to serial proxy [%u]", this->instance_index_);
      break;
    case api::enums::SERIAL_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      if (this->api_connection_ != api_connection) {
        ESP_LOGV(TAG, "API connection is not subscribed to serial proxy [%u]", this->instance_index_);
        return;
      }
      this->api_connection_ = nullptr;
      ESP_LOGV(TAG, "API connection unsubscribed from serial proxy [%u]", this->instance_index_);
      break;
    default:
      ESP_LOGW(TAG, "Unknown serial proxy request type: %u", static_cast<uint32_t>(type));
      break;
  }
}
#endif

}  // namespace esphome::serial_proxy

#endif  // USE_SERIAL_PROXY
