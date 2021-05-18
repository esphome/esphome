#include "esp32_improv_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_improv {

static const char *TAG = "esp32_improv.component";

void ESP32ImprovComponent::setup() {
  this->service_ = esp32_ble_server::global_ble_server->add_service(improv::SERVICE_UUID);

  this->status_ = this->service_->createCharacteristic(
      improv::STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *status_descriptor = new BLE2902();
  this->status_->addDescriptor(status_descriptor);

  this->error_ = this->service_->createCharacteristic(
      improv::ERROR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *error_descriptor = new BLE2902();
  this->error_->addDescriptor(error_descriptor);

  this->rpc_ = this->service_->createCharacteristic(improv::RPC_UUID, BLECharacteristic::PROPERTY_WRITE);
  this->rpc_->setCallbacks(this);
  BLEDescriptor *rpc_descriptor = new BLE2902();
  this->rpc_->addDescriptor(rpc_descriptor);

  this->rpc_response_ = this->service_->createCharacteristic(
      improv::RESPONSE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor *rpc_response_descriptor = new BLE2902();
  this->rpc_response_->addDescriptor(rpc_response_descriptor);

  global_improv_component = this;
  this->set_state_(improv::STATE_STOPPED);
}

void ESP32ImprovComponent::loop() {
  if (this->incoming_data_.length() > 0)
    this->process_incoming_data_();
  uint32_t now = millis();

  switch (this->state_) {
    case improv::STATE_NONE:
      break;
    case improv::STATE_STOPPED:
      if (this->status_indicator_ != nullptr)
        this->status_indicator_->turn_off();
      break;
    case improv::STATE_STARTED: {
      if (this->activator_ == nullptr || this->activator_->state) {
        this->set_state_(improv::STATE_ACTIVATED);
        this->activated_start_ = now;
      } else {
        if (this->status_indicator_ != nullptr) {
          if (!this->check_identify_())
            this->status_indicator_->turn_on();
        }
      }
      break;
    }
    case improv::STATE_ACTIVATED: {
      if (this->activator_ != nullptr) {
        if (now - this->activated_start_ > this->activated_duration_) {
          ESP_LOGD(TAG, "Activation timeout");
          this->set_state_(improv::STATE_STARTED);
          return;
        }
      }
      if (this->status_indicator_ != nullptr) {
        if (!this->check_identify_()) {
          if ((now % 1000) < 500) {
            this->status_indicator_->turn_on();
          } else {
            this->status_indicator_->turn_off();
          }
        }
      }
      break;
    }
    case improv::STATE_RECEIVED: {
      if (this->status_indicator_ != nullptr) {
        if ((now % 200) < 100) {
          this->status_indicator_->turn_on();
        } else {
          this->status_indicator_->turn_off();
        }
      }
      if (wifi::global_wifi_component->is_connected()) {
        wifi::global_wifi_component->save_wifi_sta(this->connecting_sta_.get_ssid(),
                                                   this->connecting_sta_.get_password());
        this->connecting_sta_ = {};
        this->cancel_timeout("wifi-connect-timeout");
        this->set_state_(improv::STATE_SAVED);
      }
      break;
    }
    case improv::STATE_SAVED: {
      this->incoming_data_ = "";
      if (this->status_indicator_ != nullptr)
        this->status_indicator_->turn_on();
      break;
    }
  }
}

bool ESP32ImprovComponent::check_identify_() {
  uint32_t now = millis();

  bool identify = this->identify_start_ != 0 && now - this->identify_start_ <= this->identify_duration_;

  if (identify) {
    uint32_t time = now % 1000;
    if (time < 600 && time % 200 < 100) {
      this->status_indicator_->turn_on();
    } else {
      this->status_indicator_->turn_off();
    }
  }
  return identify;
}

void ESP32ImprovComponent::set_state_(improv::State state) {
  ESP_LOGD(TAG, "Setting state: %d", state);
  this->state_ = state;
  if (this->status_->getData()[0] != state) {
    ESP_LOGD(TAG, "States: %d -> %d", this->status_->getData()[0], state);
    uint8_t data[1]{state};
    this->status_->setValue(data, 1);
    if (state != improv::STATE_STOPPED)
      this->status_->notify();
  }
}

void ESP32ImprovComponent::set_error_(improv::Error error) {
  ESP_LOGE(TAG, "Error: %d", error);
  if (this->error_->getData()[0] != error) {
    uint8_t data[1]{error};
    this->error_->setValue(data, 1);
    if (this->state_ != improv::STATE_STOPPED)
      this->error_->notify();
  }
}

void ESP32ImprovComponent::start() {
  if (this->state_ != improv::STATE_STOPPED)
    return;

  ESP_LOGD(TAG, "Starting Improv service...");

  this->service_->start();
  BLEDevice::startAdvertising();
  ESP_LOGD(TAG, "Service started!");

  this->set_state_(improv::STATE_STARTED);
  this->error_->setValue({improv::ERROR_NONE});
}

void ESP32ImprovComponent::end() {
  this->set_state_(improv::STATE_STOPPED);
  this->service_->stop();
  esp32_ble_server::global_ble_server->teardown();
}

float ESP32ImprovComponent::get_setup_priority() const {
  // Before WiFi
  return setup_priority::HARDWARE - 10.0f;
}

void ESP32ImprovComponent::dump_config() { ESP_LOGCONFIG(TAG, "ESP32 Improv:"); }

void ESP32ImprovComponent::process_incoming_data_() {
  uint8_t length = this->incoming_data_[1];

  ESP_LOGD(
      TAG, "Processing bytes - %s",
      hexencode(reinterpret_cast<const uint8_t *>(&(this->incoming_data_[0])), this->incoming_data_.length()).c_str());
  if (this->incoming_data_.length() - 3 == length) {
    improv::ImprovCommand command = improv::parse_improv_data(
        reinterpret_cast<const uint8_t *>(&(this->incoming_data_[0])), this->incoming_data_.length());
    switch (command.command) {
      case improv::BAD_CHECKSUM:
        ESP_LOGW(TAG, "Error decoding Improv payload");
        this->set_error_(improv::ERROR_INVALID_RPC);
        this->incoming_data_ = "";
        break;
      case improv::WIFI_SETTINGS: {
        if (this->state_ != improv::STATE_ACTIVATED) {
          ESP_LOGW(TAG, "Settings received, but not activated");
          this->set_error_(improv::ERROR_NOT_ACTIVATED);
          this->incoming_data_ = "";
          return;
        }
        wifi::WiFiAP sta{};
        sta.set_ssid(command.ssid);
        sta.set_password(command.password);
        this->connecting_sta_ = sta;

        wifi::global_wifi_component->set_sta(sta);
        wifi::global_wifi_component->start_scanning();
        this->set_state_(improv::STATE_RECEIVED);
        ESP_LOGD(TAG, "Received Improv wifi settings ssid=%s, password=" LOG_SECRET("%s"), command.ssid.c_str(),
                 command.password.c_str());

        auto f = std::bind(&ESP32ImprovComponent::on_wifi_connect_timeout_, this);
        this->set_timeout("wifi-connect-timeout", 30000, f);
        this->incoming_data_ = "";
        break;
      }
      case improv::IDENTIFY:
        this->incoming_data_ = "";
        this->identify_start_ = millis();
        break;
      default:
        ESP_LOGW(TAG, "Unknown Improv payload");
        this->set_error_(improv::ERROR_UNKNOWN_RPC);
        this->incoming_data_ = "";
    }
  } else if (this->incoming_data_.length() - 2 > length) {
    ESP_LOGD(TAG, "[CHANGE TO V] Too much data came in, or malformed resetting buffer...");
    this->incoming_data_ = "";
  } else {
    ESP_LOGD(TAG, "[CHANGE TO V] Waiting for split data packets...");
  }
}

void ESP32ImprovComponent::on_wifi_connect_timeout_() {
  this->set_error_(improv::ERROR_BAD_CREDENTIALS);
  this->set_state_(improv::STATE_STARTED);
  ESP_LOGW(TAG, "Timed out trying to connect to given WiFi network");
  wifi::global_wifi_component->clear_sta();
}

void ESP32ImprovComponent::onWrite(BLECharacteristic *characteristic) {
  std::string value = characteristic->getValue();
  if (value.length() > 0) {
    this->incoming_data_ += value;
  }
}

ESP32ImprovComponent *global_improv_component = nullptr;

}  // namespace esp32_improv
}  // namespace esphome

#endif
