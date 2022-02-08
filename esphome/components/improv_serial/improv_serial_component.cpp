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

int ImprovSerialComponent::available_() {
#ifdef USE_ARDUINO
  return this->hw_serial_->available();
#endif
#ifdef USE_ESP_IDF
  size_t available;
  uart_get_buffered_data_len(this->uart_num_, &available);
  return available;
#endif
}

uint8_t ImprovSerialComponent::read_byte_() {
  uint8_t data;
#ifdef USE_ARDUINO
  this->hw_serial_->readBytes(&data, 1);
#endif
#ifdef USE_ESP_IDF
  uart_read_bytes(this->uart_num_, &data, 1, 20 / portTICK_RATE_MS);
#endif
  return data;
}

void ImprovSerialComponent::write_data_(std::vector<uint8_t> &data) {
  data.push_back('\n');
#ifdef USE_ARDUINO
  this->hw_serial_->write(data.data(), data.size());
#endif
#ifdef USE_ESP_IDF
  uart_write_bytes(this->uart_num_, data.data(), data.size());
#endif
}

void ImprovSerialComponent::loop() {
  const uint32_t now = millis();
  if (now - this->last_read_byte_ > 50) {
    this->rx_buffer_.clear();
    this->last_read_byte_ = now;
  }

  while (this->available_()) {
    uint8_t byte = this->read_byte_();
    if (this->parse_improv_serial_byte_(byte)) {
      this->last_read_byte_ = now;
    } else {
      this->rx_buffer_.clear();
    }
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
#ifdef USE_WEBSERVER
  auto ip = wifi::global_wifi_component->wifi_sta_ip();
  std::string webserver_url = "http://" + ip.str() + ":" + to_string(USE_WEBSERVER_PORT);
  urls.push_back(webserver_url);
#endif
  std::vector<uint8_t> data = improv::build_rpc_response(command, urls, false);
  return data;
}

std::vector<uint8_t> ImprovSerialComponent::build_version_info_() {
  std::vector<std::string> infos = {"ESPHome", ESPHOME_VERSION, ESPHOME_VARIANT, App.get_name()};
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
      wifi::global_wifi_component->start_scanning();
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
