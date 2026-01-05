#include "improv_serial_component.h"
#ifdef USE_WIFI
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include "esphome/components/logger/logger.h"

namespace esphome {
namespace improv_serial {

static const char *const TAG = "improv_serial";

void ImprovSerialComponent::setup() {
  global_improv_serial_component = this;
#ifdef USE_ESP32
  this->uart_num_ = logger::global_logger->get_uart_num();
#elif defined(USE_ARDUINO)
  this->hw_serial_ = logger::global_logger->get_hw_serial();
#endif

  if (wifi::global_wifi_component->has_sta()) {
    this->state_ = improv::STATE_PROVISIONED;
  } else {
    wifi::global_wifi_component->start_scanning();
  }
}

void ImprovSerialComponent::loop() {
  if (this->last_read_byte_ && (millis() - this->last_read_byte_ > IMPROV_SERIAL_TIMEOUT)) {
    this->last_read_byte_ = 0;
    this->rx_buffer_.clear();
    ESP_LOGV(TAG, "Timeout");
  }

  auto byte = this->read_byte_();
  while (byte.has_value()) {
    if (this->parse_improv_serial_byte_(byte.value())) {
      this->last_read_byte_ = millis();
    } else {
      this->last_read_byte_ = 0;
      this->rx_buffer_.clear();
    }
    byte = this->read_byte_();
  }

  if (this->state_ == improv::STATE_PROVISIONING) {
    if (wifi::global_wifi_component->is_connected()) {
      wifi::global_wifi_component->save_wifi_sta(this->connecting_sta_.get_ssid(),
                                                 this->connecting_sta_.get_password());
      this->connecting_sta_ = {};
      this->cancel_timeout("wifi-connect-timeout");
      this->set_state_(improv::STATE_PROVISIONED);

      std::vector<uint8_t> url = this->build_rpc_settings_response_(improv::WIFI_SETTINGS);
      this->send_response_(url);
    }
  }
}

void ImprovSerialComponent::dump_config() { ESP_LOGCONFIG(TAG, "Improv Serial:"); }

optional<uint8_t> ImprovSerialComponent::read_byte_() {
  optional<uint8_t> byte;
  uint8_t data = 0;
#ifdef USE_ESP32
  switch (logger::global_logger->get_uart()) {
    case logger::UART_SELECTION_UART0:
    case logger::UART_SELECTION_UART1:
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6) && \
    !defined(USE_ESP32_VARIANT_ESP32C61) && !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32S3)
    case logger::UART_SELECTION_UART2:
#endif  // !USE_ESP32_VARIANT_ESP32C3 && !USE_ESP32_VARIANT_ESP32C6 && !USE_ESP32_VARIANT_ESP32C61 &&
        // !USE_ESP32_VARIANT_ESP32S2 && !USE_ESP32_VARIANT_ESP32S3
      if (this->uart_num_ >= 0) {
        size_t available;
        uart_get_buffered_data_len(this->uart_num_, &available);
        if (available) {
          uart_read_bytes(this->uart_num_, &data, 1, 0);
          byte = data;
        }
      }
      break;
#if defined(USE_LOGGER_USB_CDC) && defined(CONFIG_ESP_CONSOLE_USB_CDC)
    case logger::UART_SELECTION_USB_CDC:
      if (esp_usb_console_available_for_read()) {
        esp_usb_console_read_buf((char *) &data, 1);
        byte = data;
      }
      break;
#endif  // USE_LOGGER_USB_CDC
#ifdef USE_LOGGER_USB_SERIAL_JTAG
    case logger::UART_SELECTION_USB_SERIAL_JTAG: {
      if (usb_serial_jtag_read_bytes((char *) &data, 1, 0)) {
        byte = data;
      }
      break;
    }
#endif  // USE_LOGGER_USB_SERIAL_JTAG
    default:
      break;
  }
#elif defined(USE_ARDUINO)
  if (this->hw_serial_->available()) {
    this->hw_serial_->readBytes(&data, 1);
    byte = data;
  }
#endif
  return byte;
}

void ImprovSerialComponent::write_data_(const uint8_t *data, const size_t size) {
  // First, set length field
  this->tx_header_[TX_LENGTH_IDX] = this->tx_header_[TX_TYPE_IDX] == TYPE_RPC_RESPONSE ? size : 1;

  const bool there_is_data = data != nullptr && size > 0;
  // If there_is_data, checksum must not include our optional data byte
  const uint8_t header_checksum_len = there_is_data ? TX_BUFFER_SIZE - 3 : TX_BUFFER_SIZE - 2;
  // Only transmit the full buffer length if there is no data (only state/error byte is provided in this case)
  const uint8_t header_tx_len = there_is_data ? TX_BUFFER_SIZE - 3 : TX_BUFFER_SIZE;
  // Calculate checksum for message
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < header_checksum_len; i++) {
    checksum += this->tx_header_[i];
  }
  if (there_is_data) {
    // Include data in checksum
    for (size_t i = 0; i < size; i++) {
      checksum += data[i];
    }
  }
  this->tx_header_[TX_CHECKSUM_IDX] = checksum;

#ifdef USE_ESP32
  switch (logger::global_logger->get_uart()) {
    case logger::UART_SELECTION_UART0:
    case logger::UART_SELECTION_UART1:
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6) && \
    !defined(USE_ESP32_VARIANT_ESP32C61) && !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32S3)
    case logger::UART_SELECTION_UART2:
#endif
      uart_write_bytes(this->uart_num_, this->tx_header_, header_tx_len);
      if (there_is_data) {
        uart_write_bytes(this->uart_num_, data, size);
        uart_write_bytes(this->uart_num_, &this->tx_header_[TX_CHECKSUM_IDX], 2);  // Footer: checksum and newline
      }
      break;
#if defined(USE_LOGGER_USB_CDC) && defined(CONFIG_ESP_CONSOLE_USB_CDC)
    case logger::UART_SELECTION_USB_CDC:
      esp_usb_console_write_buf((const char *) this->tx_header_, header_tx_len);
      if (there_is_data) {
        esp_usb_console_write_buf((const char *) data, size);
        esp_usb_console_write_buf((const char *) &this->tx_header_[TX_CHECKSUM_IDX],
                                  2);  // Footer: checksum and newline
      }
      break;
#endif
#ifdef USE_LOGGER_USB_SERIAL_JTAG
    case logger::UART_SELECTION_USB_SERIAL_JTAG:
      usb_serial_jtag_write_bytes((const char *) this->tx_header_, header_tx_len, 20 / portTICK_PERIOD_MS);
      if (there_is_data) {
        usb_serial_jtag_write_bytes((const char *) data, size, 20 / portTICK_PERIOD_MS);
        usb_serial_jtag_write_bytes((const char *) &this->tx_header_[TX_CHECKSUM_IDX], 2,
                                    20 / portTICK_PERIOD_MS);  // Footer: checksum and newline
      }
      break;
#endif
    default:
      break;
  }
#elif defined(USE_ARDUINO)
  this->hw_serial_->write(this->tx_header_, header_tx_len);
  if (there_is_data) {
    this->hw_serial_->write(data, size);
    this->hw_serial_->write(&this->tx_header_[TX_CHECKSUM_IDX], 2);  // Footer: checksum and newline
  }
#endif
}

std::vector<uint8_t> ImprovSerialComponent::build_rpc_settings_response_(improv::Command command) {
  std::vector<std::string> urls;
#ifdef USE_IMPROV_SERIAL_NEXT_URL
  if (!this->next_url_.empty()) {
    urls.push_back(this->get_formatted_next_url_());
  }
#endif
#ifdef USE_WEBSERVER
  for (auto &ip : wifi::global_wifi_component->wifi_sta_ip_addresses()) {
    if (ip.is_ip4()) {
      std::string webserver_url = "http://" + ip.str() + ":" + to_string(USE_WEBSERVER_PORT);
      urls.push_back(webserver_url);
      break;
    }
  }
#endif
  std::vector<uint8_t> data = improv::build_rpc_response(command, urls, false);
  return data;
}

std::vector<uint8_t> ImprovSerialComponent::build_version_info_() {
#ifdef ESPHOME_PROJECT_NAME
  std::vector<std::string> infos = {ESPHOME_PROJECT_NAME, ESPHOME_PROJECT_VERSION, ESPHOME_VARIANT, App.get_name()};
#else
  std::vector<std::string> infos = {"ESPHome", ESPHOME_VERSION, ESPHOME_VARIANT, App.get_name()};
#endif
  std::vector<uint8_t> data = improv::build_rpc_response(improv::GET_DEVICE_INFO, infos, false);
  return data;
};

bool ImprovSerialComponent::parse_improv_serial_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  ESP_LOGV(TAG, "Byte: 0x%02X", byte);
  const uint8_t *raw = &this->rx_buffer_[0];

  return improv::parse_improv_serial_byte(
      at, byte, raw, [this](improv::ImprovCommand command) -> bool { return this->parse_improv_payload_(command); },
      [this](improv::Error error) -> void {
        ESP_LOGW(TAG, "Error decoding payload");
        this->set_error_(error);
      });
}

bool ImprovSerialComponent::parse_improv_payload_(improv::ImprovCommand &command) {
  switch (command.command) {
    case improv::WIFI_SETTINGS: {
      wifi::WiFiAP sta{};
      sta.set_ssid(command.ssid);
      sta.set_password(command.password);
      this->connecting_sta_ = sta;

      wifi::global_wifi_component->set_sta(sta);
      wifi::global_wifi_component->start_connecting(sta);
      this->set_state_(improv::STATE_PROVISIONING);
      ESP_LOGD(TAG, "Received settings: SSID=%s, password=" LOG_SECRET("%s"), command.ssid.c_str(),
               command.password.c_str());

      auto f = std::bind(&ImprovSerialComponent::on_wifi_connect_timeout_, this);
      this->set_timeout("wifi-connect-timeout", 30000, f);
      return true;
    }
    case improv::GET_CURRENT_STATE:
      this->set_state_(this->state_);
      if (this->state_ == improv::STATE_PROVISIONED) {
        std::vector<uint8_t> url = this->build_rpc_settings_response_(improv::GET_CURRENT_STATE);
        this->send_response_(url);
      }
      return true;
    case improv::GET_DEVICE_INFO: {
      std::vector<uint8_t> info = this->build_version_info_();
      this->send_response_(info);
      return true;
    }
    case improv::GET_WIFI_NETWORKS: {
      std::vector<std::string> networks;
      const auto &results = wifi::global_wifi_component->get_scan_result();
      for (auto &scan : results) {
        if (scan.get_is_hidden())
          continue;
        const std::string &ssid = scan.get_ssid();
        if (std::find(networks.begin(), networks.end(), ssid) != networks.end())
          continue;
        // Send each ssid separately to avoid overflowing the buffer
        std::vector<uint8_t> data = improv::build_rpc_response(
            improv::GET_WIFI_NETWORKS, {ssid, str_sprintf("%d", scan.get_rssi()), YESNO(scan.get_with_auth())}, false);
        this->send_response_(data);
        networks.push_back(ssid);
      }
      // Send empty response to signify the end of the list.
      std::vector<uint8_t> data =
          improv::build_rpc_response(improv::GET_WIFI_NETWORKS, std::vector<std::string>{}, false);
      this->send_response_(data);
      return true;
    }
    default: {
      ESP_LOGW(TAG, "Unknown payload");
      this->set_error_(improv::ERROR_UNKNOWN_RPC);
      return false;
    }
  }
}

void ImprovSerialComponent::set_state_(improv::State state) {
  this->state_ = state;
  this->tx_header_[TX_TYPE_IDX] = TYPE_CURRENT_STATE;
  this->tx_header_[TX_DATA_IDX] = state;
  this->write_data_();
}

void ImprovSerialComponent::set_error_(improv::Error error) {
  this->tx_header_[TX_TYPE_IDX] = TYPE_ERROR_STATE;
  this->tx_header_[TX_DATA_IDX] = error;
  this->write_data_();
}

void ImprovSerialComponent::send_response_(std::vector<uint8_t> &response) {
  this->tx_header_[TX_TYPE_IDX] = TYPE_RPC_RESPONSE;
  this->write_data_(response.data(), response.size());
}

void ImprovSerialComponent::on_wifi_connect_timeout_() {
  this->set_error_(improv::ERROR_UNABLE_TO_CONNECT);
  this->set_state_(improv::STATE_AUTHORIZED);
  ESP_LOGW(TAG, "Timed out while connecting to Wi-Fi network");
  wifi::global_wifi_component->clear_sta();
}

ImprovSerialComponent *global_improv_serial_component =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;                                             // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace improv_serial
}  // namespace esphome
#endif
