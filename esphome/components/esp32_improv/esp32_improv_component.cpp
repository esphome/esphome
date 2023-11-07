#include "esp32_improv_component.h"

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/esp32_ble_server/ble_2902.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_improv {

static const char *const TAG = "esp32_improv.component";
static const char *const ESPHOME_MY_LINK = "https://my.home-assistant.io/redirect/config_flow_start?domain=esphome";

ESP32ImprovComponent::ESP32ImprovComponent() { global_improv_component = this; }

void ESP32ImprovComponent::setup() {
#ifdef USE_BINARY_SENSOR
  if (this->authorizer_ != nullptr) {
    this->authorizer_->add_on_state_callback([this](bool state) {
      if (state) {
        this->authorized_start_ = millis();
        this->identify_start_ = 0;
      }
    });
  }
#endif
}

void ESP32ImprovComponent::setup_characteristics() {
  this->status_ = this->service_->create_characteristic(
      improv::STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *status_descriptor = new BLE2902();
  this->status_->add_descriptor(status_descriptor);

  this->error_ = this->service_->create_characteristic(
      improv::ERROR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *error_descriptor = new BLE2902();
  this->error_->add_descriptor(error_descriptor);

  this->rpc_ = this->service_->create_characteristic(improv::RPC_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  this->rpc_->on_write([this](const std::vector<uint8_t> &data) {
    if (!data.empty()) {
      this->incoming_data_.insert(this->incoming_data_.end(), data.begin(), data.end());
    }
  });
  BLEDescriptor *rpc_descriptor = new BLE2902();
  this->rpc_->add_descriptor(rpc_descriptor);

  this->rpc_response_ = this->service_->create_characteristic(
      improv::RPC_RESULT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *rpc_response_descriptor = new BLE2902();
  this->rpc_response_->add_descriptor(rpc_response_descriptor);

  this->capabilities_ =
      this->service_->create_characteristic(improv::CAPABILITIES_UUID, BLECharacteristic::PROPERTY_READ);
  BLEDescriptor *capabilities_descriptor = new BLE2902();
  this->capabilities_->add_descriptor(capabilities_descriptor);
  uint8_t capabilities = 0x00;
#ifdef USE_OUTPUT
  if (this->status_indicator_ != nullptr)
    capabilities |= improv::CAPABILITY_IDENTIFY;
#endif
  this->capabilities_->set_value(capabilities);
  this->setup_complete_ = true;
}

void ESP32ImprovComponent::loop() {
  if (!global_ble_server->is_running()) {
    this->state_ = improv::STATE_STOPPED;
    this->incoming_data_.clear();
    return;
  }
  if (this->service_ == nullptr) {
    // Setup the service
    ESP_LOGD(TAG, "Creating Improv service");
    global_ble_server->create_service(ESPBTUUID::from_raw(improv::SERVICE_UUID), true);
    this->service_ = global_ble_server->get_service(ESPBTUUID::from_raw(improv::SERVICE_UUID));
    this->setup_characteristics();
  }

  if (!this->incoming_data_.empty())
    this->process_incoming_data_();
  uint32_t now = millis();

  switch (this->state_) {
    case improv::STATE_STOPPED:
      this->set_status_indicator_state_(false);

      if (this->service_->is_created() && this->should_start_ && this->setup_complete_) {
        if (this->service_->is_running()) {
          esp32_ble::global_ble->advertising_start();

          this->set_state_(improv::STATE_AWAITING_AUTHORIZATION);
          this->set_error_(improv::ERROR_NONE);
          ESP_LOGD(TAG, "Service started!");
        } else {
          this->service_->start();
        }
      }
      break;
    case improv::STATE_AWAITING_AUTHORIZATION: {
#ifdef USE_BINARY_SENSOR
      if (this->authorizer_ == nullptr ||
          (this->authorized_start_ != 0 && ((now - this->authorized_start_) < this->authorized_duration_))) {
        this->set_state_(improv::STATE_AUTHORIZED);
      } else
#else
      this->set_state_(improv::STATE_AUTHORIZED);
#endif
      {
        if (!this->check_identify_())
          this->set_status_indicator_state_(true);
      }
      break;
    }
    case improv::STATE_AUTHORIZED: {
#ifdef USE_BINARY_SENSOR
      if (this->authorizer_ != nullptr) {
        if (now - this->authorized_start_ > this->authorized_duration_) {
          ESP_LOGD(TAG, "Authorization timeout");
          this->set_state_(improv::STATE_AWAITING_AUTHORIZATION);
          return;
        }
      }
#endif
      if (!this->check_identify_()) {
        this->set_status_indicator_state_((now % 1000) < 500);
      }
      break;
    }
    case improv::STATE_PROVISIONING: {
      this->set_status_indicator_state_((now % 200) < 100);
      if (wifi::global_wifi_component->is_connected()) {
        wifi::global_wifi_component->save_wifi_sta(this->connecting_sta_.get_ssid(),
                                                   this->connecting_sta_.get_password());
        this->connecting_sta_ = {};
        this->cancel_timeout("wifi-connect-timeout");
        this->set_state_(improv::STATE_PROVISIONED);

        std::vector<std::string> urls = {ESPHOME_MY_LINK};
#ifdef USE_WEBSERVER
        auto ip = wifi::global_wifi_component->wifi_sta_ip();
        std::string webserver_url = "http://" + ip.str() + ":" + to_string(USE_WEBSERVER_PORT);
        urls.push_back(webserver_url);
#endif
        std::vector<uint8_t> data = improv::build_rpc_response(improv::WIFI_SETTINGS, urls);
        this->send_response_(data);
        this->stop();
      }
      break;
    }
    case improv::STATE_PROVISIONED: {
      this->incoming_data_.clear();
      this->set_status_indicator_state_(false);
      break;
    }
  }
}

void ESP32ImprovComponent::set_status_indicator_state_(bool state) {
#ifdef USE_OUTPUT
  if (this->status_indicator_ == nullptr)
    return;
  if (this->status_indicator_state_ == state)
    return;
  this->status_indicator_state_ = state;
  if (state) {
    this->status_indicator_->turn_on();
  } else {
    this->status_indicator_->turn_off();
  }
#endif
}

bool ESP32ImprovComponent::check_identify_() {
  uint32_t now = millis();

  bool identify = this->identify_start_ != 0 && now - this->identify_start_ <= this->identify_duration_;

  if (identify) {
    uint32_t time = now % 1000;
    this->set_status_indicator_state_(time < 600 && time % 200 < 100);
  }
  return identify;
}

void ESP32ImprovComponent::set_state_(improv::State state) {
  ESP_LOGV(TAG, "Setting state: %d", state);
  this->state_ = state;
  if (this->status_->get_value().empty() || this->status_->get_value()[0] != state) {
    uint8_t data[1]{state};
    this->status_->set_value(data, 1);
    if (state != improv::STATE_STOPPED)
      this->status_->notify();
  }
  std::vector<uint8_t> service_data(8, 0);
  service_data[0] = 0x77;  // PR
  service_data[1] = 0x46;  // IM
  service_data[2] = static_cast<uint8_t>(state);

  uint8_t capabilities = 0x00;
#ifdef USE_OUTPUT
  if (this->status_indicator_ != nullptr)
    capabilities |= improv::CAPABILITY_IDENTIFY;
#endif

  service_data[3] = capabilities;
  service_data[4] = 0x00;  // Reserved
  service_data[5] = 0x00;  // Reserved
  service_data[6] = 0x00;  // Reserved
  service_data[7] = 0x00;  // Reserved

  esp32_ble::global_ble->advertising_set_service_data(service_data);
}

void ESP32ImprovComponent::set_error_(improv::Error error) {
  if (error != improv::ERROR_NONE) {
    ESP_LOGE(TAG, "Error: %d", error);
  }
  if (this->error_->get_value().empty() || this->error_->get_value()[0] != error) {
    uint8_t data[1]{error};
    this->error_->set_value(data, 1);
    if (this->state_ != improv::STATE_STOPPED)
      this->error_->notify();
  }
}

void ESP32ImprovComponent::send_response_(std::vector<uint8_t> &response) {
  this->rpc_response_->set_value(response);
  if (this->state_ != improv::STATE_STOPPED)
    this->rpc_response_->notify();
}

void ESP32ImprovComponent::start() {
  if (this->should_start_ || this->state_ != improv::STATE_STOPPED)
    return;

  ESP_LOGD(TAG, "Setting Improv to start");
  this->should_start_ = true;
}

void ESP32ImprovComponent::stop() {
  this->should_start_ = false;
  this->set_timeout("end-service", 1000, [this] {
    if (this->state_ == improv::STATE_STOPPED || this->service_ == nullptr)
      return;
    this->service_->stop();
    this->set_state_(improv::STATE_STOPPED);
  });
}

float ESP32ImprovComponent::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void ESP32ImprovComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 Improv:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Authorizer", this->authorizer_);
#endif
#ifdef USE_OUTPUT
  ESP_LOGCONFIG(TAG, "  Status Indicator: '%s'", YESNO(this->status_indicator_ != nullptr));
#endif
}

void ESP32ImprovComponent::process_incoming_data_() {
  uint8_t length = this->incoming_data_[1];

  ESP_LOGD(TAG, "Processing bytes - %s", format_hex_pretty(this->incoming_data_).c_str());
  if (this->incoming_data_.size() - 3 == length) {
    this->set_error_(improv::ERROR_NONE);
    improv::ImprovCommand command = improv::parse_improv_data(this->incoming_data_);
    switch (command.command) {
      case improv::BAD_CHECKSUM:
        ESP_LOGW(TAG, "Error decoding Improv payload");
        this->set_error_(improv::ERROR_INVALID_RPC);
        this->incoming_data_.clear();
        break;
      case improv::WIFI_SETTINGS: {
        if (this->state_ != improv::STATE_AUTHORIZED) {
          ESP_LOGW(TAG, "Settings received, but not authorized");
          this->set_error_(improv::ERROR_NOT_AUTHORIZED);
          this->incoming_data_.clear();
          return;
        }
        wifi::WiFiAP sta{};
        sta.set_ssid(command.ssid);
        sta.set_password(command.password);
        this->connecting_sta_ = sta;

        wifi::global_wifi_component->set_sta(sta);
        wifi::global_wifi_component->start_scanning();
        this->set_state_(improv::STATE_PROVISIONING);
        ESP_LOGD(TAG, "Received Improv wifi settings ssid=%s, password=" LOG_SECRET("%s"), command.ssid.c_str(),
                 command.password.c_str());

        auto f = std::bind(&ESP32ImprovComponent::on_wifi_connect_timeout_, this);
        this->set_timeout("wifi-connect-timeout", 30000, f);
        this->incoming_data_.clear();
        break;
      }
      case improv::IDENTIFY:
        this->incoming_data_.clear();
        this->identify_start_ = millis();
        break;
      default:
        ESP_LOGW(TAG, "Unknown Improv payload");
        this->set_error_(improv::ERROR_UNKNOWN_RPC);
        this->incoming_data_.clear();
    }
  } else if (this->incoming_data_.size() - 2 > length) {
    ESP_LOGV(TAG, "Too much data came in, or malformed resetting buffer...");
    this->incoming_data_.clear();
  } else {
    ESP_LOGV(TAG, "Waiting for split data packets...");
  }
}

void ESP32ImprovComponent::on_wifi_connect_timeout_() {
  this->set_error_(improv::ERROR_UNABLE_TO_CONNECT);
  this->set_state_(improv::STATE_AUTHORIZED);
#ifdef USE_BINARY_SENSOR
  if (this->authorizer_ != nullptr)
    this->authorized_start_ = millis();
#endif
  ESP_LOGW(TAG, "Timed out trying to connect to given WiFi network");
  wifi::global_wifi_component->clear_sta();
}

void ESP32ImprovComponent::on_client_disconnect() { this->set_error_(improv::ERROR_NONE); };

ESP32ImprovComponent *global_improv_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esp32_improv
}  // namespace esphome

#endif
