#include "improv_serial_component.h"

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
#ifdef USE_ARDUINO
  this->hw_serial_ = logger::global_logger->get_hw_serial();
#endif
#ifdef USE_ESP_IDF
  this->uart_num_ = logger::global_logger->get_uart_num();
#endif

  if (wifi::global_wifi_component->has_sta()) {
    this->state_ = improv::STATE_PROVISIONED;
  } else {
    wifi::global_wifi_component->start_scanning();
  }
}

void ImprovSerialComponent::dump_config() { ESP_LOGCONFIG(TAG, "Improv Serial:"); }

optional<uint8_t> ImprovSerialComponent::read_byte_() {
  optional<uint8_t> byte;
  uint8_t data = 0;
#ifdef USE_ARDUINO
  if (this->hw_serial_->available()) {
    this->hw_serial_->readBytes(&data, 1);
    byte = data;
  }
#endif
#ifdef USE_ESP_IDF
  switch (logger::global_logger->get_uart()) {
    case logger::UART_SELECTION_UART0:
    case logger::UART_SELECTION_UART1:
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6) && \
    !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32S3)
    case logger::UART_SELECTION_UART2:
#endif  // !USE_ESP32_VARIANT_ESP32C3 && !USE_ESP32_VARIANT_ESP32S2 && !USE_ESP32_VARIANT_ESP32S3
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
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
      if (esp_usb_console_available_for_read()) {
#else
      if (esp_usb_console_read_available()) {
#endif
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
#endif
  return byte;
}

void ImprovSerialComponent::write_data_(std::vector<uint8_t> &data) {
  data.push_back('\n');
#ifdef USE_ARDUINO
  this->hw_serial_->write(data.data(), data.size());
#endif
#ifdef USE_ESP_IDF
  switch (logger::global_logger->get_uart()) {
    case logger::UART_SELECTION_UART0:
    case logger::UART_SELECTION_UART1:
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32C6) && \
    !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32S3)
    case logger::UART_SELECTION_UART2:
#endif  // !USE_ESP32_VARIANT_ESP32C3 && !USE_ESP32_VARIANT_ESP32S2 && !USE_ESP32_VARIANT_ESP32S3
      uart_write_bytes(this->uart_num_, data.data(), data.size());
      break;
#if defined(USE_LOGGER_USB_CDC) && defined(CONFIG_ESP_CONSOLE_USB_CDC)
    case logger::UART_SELECTION_USB_CDC: {
      const char *msg = (char *) data.data();
      esp_usb_console_write_buf(msg, data.size());
      break;
    }
#endif  // USE_LOGGER_USB_CDC
#ifdef USE_LOGGER_USB_SERIAL_JTAG
    case logger::UART_SELECTION_USB_SERIAL_JTAG:
      usb_serial_jtag_write_bytes((char *) data.data(), data.size(), 20 / portTICK_PERIOD_MS);
      delay(10);
      usb_serial_jtag_ll_txfifo_flush();  // fixes for issue in IDF 4.4.7
      break;
#endif  // USE_LOGGER_USB_SERIAL_JTAG
    default:
      break;
  }
#endif
}

void ImprovSerialComponent::loop() {
  if (this->last_read_byte_ && (millis() - this->last_read_byte_ > IMPROV_SERIAL_TIMEOUT)) {
    this->last_read_byte_ = 0;
    this->rx_buffer_.clear();
    ESP_LOGV(TAG, "Improv Serial timeout");
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

std::vector<uint8_t> ImprovSerialComponent::build_rpc_settings_response_(improv::Command command) {
  std::vector<std::string> urls;
  if (!this->next_url_.empty()) {
    urls.push_back(this->get_formatted_next_url_());
  }
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
  ESP_LOGV(TAG, "Improv Serial byte: 0x%02X", byte);
  const uint8_t *raw = &this->rx_buffer_[0];

  return improv::parse_improv_serial_byte(
      at, byte, raw, [this](improv::ImprovCommand command) -> bool { return this->parse_improv_payload_(command); },
      [this](improv::Error error) -> void {
        ESP_LOGW(TAG, "Error decoding Improv payload");
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
      wifi::global_wifi_component->start_connecting(sta, false);
      this->set_state_(improv::STATE_PROVISIONING);
      ESP_LOGD(TAG, "Received Improv wifi settings ssid=%s, password=" LOG_SECRET("%s"), command.ssid.c_str(),
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
      auto results = wifi::global_wifi_component->get_scan_result();
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
      ESP_LOGW(TAG, "Unknown Improv payload");
      this->set_error_(improv::ERROR_UNKNOWN_RPC);
      return false;
    }
  }
}

void ImprovSerialComponent::set_state_(improv::State state) {
  this->state_ = state;

  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(11);
  data[6] = IMPROV_SERIAL_VERSION;
  data[7] = TYPE_CURRENT_STATE;
  data[8] = 1;
  data[9] = state;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[10] = checksum;

  this->write_data_(data);
}

void ImprovSerialComponent::set_error_(improv::Error error) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(11);
  data[6] = IMPROV_SERIAL_VERSION;
  data[7] = TYPE_ERROR_STATE;
  data[8] = 1;
  data[9] = error;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[10] = checksum;
  this->write_data_(data);
}

void ImprovSerialComponent::send_response_(std::vector<uint8_t> &response) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'R', 'O', 'V'};
  data.resize(9);
  data[6] = IMPROV_SERIAL_VERSION;
  data[7] = TYPE_RPC_RESPONSE;
  data[8] = response.size();
  data.insert(data.end(), response.begin(), response.end());

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data.push_back(checksum);

  this->write_data_(data);
}

void ImprovSerialComponent::on_wifi_connect_timeout_() {
  this->set_error_(improv::ERROR_UNABLE_TO_CONNECT);
  this->set_state_(improv::STATE_AUTHORIZED);
  ESP_LOGW(TAG, "Timed out trying to connect to given WiFi network");
  wifi::global_wifi_component->clear_sta();
}

ImprovSerialComponent *global_improv_serial_component =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;                                             // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace improv_serial
}  // namespace esphome
