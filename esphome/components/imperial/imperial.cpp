#include "imperial.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esphome/components/logger/logger.h"

namespace esphome {
namespace imperial {

static const char *const TAG = "imperial";

void ImperialComponent::setup() {
  global_imperial_component = this;
#ifdef USE_ARDUINO
  this->hw_serial_ = logger::global_logger->get_hw_serial();
#endif
#ifdef USE_ESP_IDF
  this->uart_num_ = logger::global_logger->get_uart_num();
#endif
}

void ImperialComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Imperial:");
}

int ImperialComponent::available_() {
#ifdef USE_ARDUINO
  return this->hw_serial_->available();
#endif
#ifdef USE_ESP_IDF
  size_t available;
  uart_get_buffered_data_len(this->uart_num_, &available);
  return available;
#endif
}

uint8_t ImperialComponent::read_byte_() {
  uint8_t data;
#ifdef USE_ARDUINO
  this->hw_serial_->readBytes(&data, 1);
#endif
#ifdef USE_ESP_IDF
  uart_read_bytes(this->uart_num_, &data, 1, 20 / portTICK_RATE_MS);
#endif
  return data;
}

void ImperialComponent::write_data_(std::vector<uint8_t> &data) {
  data.push_back('\n');
#ifdef USE_ARDUINO
  data this->hw_serial_->write(data.data(), data.size());
#endif
#ifdef USE_ESP_IDF
  uart_write_bytes(this->uart_num_, data.data(), data.size());
#endif
}

void ImperialComponent::loop() {
  const uint32_t now = millis();
  if (now - this->last_read_byte_ > 50) {
    this->rx_buffer_.clear();
    this->last_read_byte_ = now;
  }

  while (this->available_()) {
    uint8_t byte = this->read_byte_();
    if (this->parse_imperial_byte_(byte)) {
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

      std::string url = "https://my.home-assistant.io/redirect/config_flow_start?domain=esphome";
      std::vector<uint8_t> data = improv::build_rpc_response(improv::WIFI_SETTINGS, {url});
      this->send_response_(data);
    }
  }
}

bool ImperialComponent::parse_imperial_byte_(uint8_t byte) {
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  ESP_LOGD(TAG, "Imperial byte: 0x%02X", byte);
  const uint8_t *raw = &this->rx_buffer_[0];
  if (at == 0)
    return byte == 'I';
  if (at == 1)
    return byte == 'M';
  if (at == 2)
    return byte == 'P';
  if (at == 3)
    return byte == 'E';
  if (at == 4)
    return byte == 'R';
  if (at == 5)
    return byte == 'I';
  if (at == 6)
    return byte == 'A';
  if (at == 7)
    return byte == 'L';

  if (at == 8)
    return true;
  uint8_t version = raw[8];

  if (at == 9)
    return true;
  uint8_t type = raw[9];

  if (at == 10)
    return true;
  uint8_t data_len = raw[10];

  if (at < 10 + data_len)
    return true;

  if (at == 10 + data_len) {
    if (type == TYPE_RPC) {
      this->set_error_(improv::ERROR_NONE);
      auto command = improv::parse_improv_data(&raw[11], data_len);
      return this->parse_improv_payload_(command);
    }
  }
  return true;
}

bool ImperialComponent::parse_improv_payload_(improv::ImprovCommand command) {
  switch (command.command) {
    case improv::BAD_CHECKSUM:
      ESP_LOGW(TAG, "Error decoding Improv payload");
      this->set_error_(improv::ERROR_INVALID_RPC);
      return false;
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

      auto f = std::bind(&ImperialComponent::on_wifi_connect_timeout_, this);
      this->set_timeout("wifi-connect-timeout", 30000, f);
      return true;
    }
    case improv::GET_CURRENT_STATE:
      this->set_state_(this->state_);
      return true;
    default: {
      ESP_LOGW(TAG, "Unknown Improv payload");
      this->set_error_(improv::ERROR_UNKNOWN_RPC);
      return false;
    }
  }
}

void ImperialComponent::set_state_(improv::State state) {
  this->state_ = state;

  std::vector<uint8_t> data = {'I', 'M', 'P', 'E', 'R', 'I', 'A', 'L'};
  data.resize(13);
  data[8] = VERSION;
  data[9] = TYPE_CURRENT_STATE;
  data[10] = 1;
  data[11] = state;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[12] = checksum;

  this->write_data_(data);
}

void ImperialComponent::set_error_(improv::Error error) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'E', 'R', 'I', 'A', 'L'};
  data.resize(13);
  data[8] = VERSION;
  data[9] = TYPE_ERROR_STATE;
  data[10] = 1;
  data[11] = error;

  uint8_t checksum = 0x00;
  for (uint8_t d : data)
    checksum += d;
  data[12] = checksum;
  this->write_data_(data);
}

void ImperialComponent::send_response_(std::vector<uint8_t> &response) {
  std::vector<uint8_t> data = {'I', 'M', 'P', 'E', 'R', 'I', 'A', 'L'};
  data.resize(11);
  data[8] = VERSION;
  data[9] = TYPE_RPC;
  data[10] = response.size();
  data.insert(data.end(), response.begin(), response.end());
  this->write_data_(data);
}

void ImperialComponent::on_wifi_connect_timeout_() {
  this->set_error_(improv::ERROR_UNABLE_TO_CONNECT);
  this->set_state_(improv::STATE_AUTHORIZED);
  ESP_LOGW(TAG, "Timed out trying to connect to given WiFi network");
  wifi::global_wifi_component->clear_sta();
}

ImperialComponent *global_imperial_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace imperial
}  // namespace esphome
