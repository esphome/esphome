#include "serial_proxy.h"

#ifdef USE_SERIAL_PROXY

#include "esphome/core/log.h"

#ifdef USE_API
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
}

void SerialProxy::loop() {
  // Read available data from UART and forward to API clients
  size_t available = this->available();
  if (available == 0)
    return;

  // Read in chunks up to SERIAL_PROXY_MAX_READ_SIZE
  uint8_t buffer[SERIAL_PROXY_MAX_READ_SIZE];
  size_t to_read = std::min(available, sizeof(buffer));

  if (!this->read_array(buffer, to_read))
    return;

#ifdef USE_API
  if (api::global_api_server != nullptr) {
    api::global_api_server->send_serial_proxy_data(this->instance_index_, buffer, to_read);
  }
#endif
}

void SerialProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Serial Proxy [%u]:\n"
                "  Name: %s\n"
                "  Port Type: %s\n"
                "  RTS Pin: %s\n"
                "  DTR Pin: %s",
                this->instance_index_, this->name_.c_str(),
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

  // Apply UART parameters
  uart_comp->set_baud_rate(baudrate);
  uart_comp->set_stop_bits(stop_bits);
  uart_comp->set_data_bits(data_size);

  // Map parity enum to UARTParityOptions
  switch (parity) {
    case 0:
      uart_comp->set_parity(uart::UART_CONFIG_PARITY_NONE);
      break;
    case 1:
      uart_comp->set_parity(uart::UART_CONFIG_PARITY_EVEN);
      break;
    case 2:
      uart_comp->set_parity(uart::UART_CONFIG_PARITY_ODD);
      break;
    default:
      ESP_LOGW(TAG, "Unknown parity value: %u, using NONE", parity);
      uart_comp->set_parity(uart::UART_CONFIG_PARITY_NONE);
      break;
  }

    // Apply the new settings
    // load_settings() is available on ESP8266 and ESP32 platforms
#if defined(USE_ESP8266) || defined(USE_ESP32)
  uart_comp->load_settings(true);
#endif

  // Note: Hardware flow control configuration is stored but not yet applied
  // to the UART hardware - this requires additional platform support
  (void) flow_control;
}

void SerialProxy::write(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0)
    return;
  this->write_array(data, len);
}

void SerialProxy::set_modem_pins(bool rts, bool dtr) {
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

void SerialProxy::get_modem_pins(bool &rts, bool &dtr) const {
  rts = this->rts_state_;
  dtr = this->dtr_state_;
}

void SerialProxy::flush_port() {
  ESP_LOGV(TAG, "Flushing serial proxy [%u]", this->instance_index_);
  this->flush();
}

}  // namespace esphome::serial_proxy

#endif  // USE_SERIAL_PROXY
