#include "airthings_wave_plus.h"

namespace esphome {
namespace airthings_wave_plus {

void AirthingsWavePlus::client_connected_() {
  ESP_LOGD(TAG, "AirthingsWavePlus::client_connected_");

  connected_ = true;
  connecting_ = false;
  last_value_time_ = millis();

  // Schedule to avoid a deadlock
  set_timeout(0, [this] { this->read_sensors_(); });
}

void AirthingsWavePlus::client_disconnected_() {
  ESP_LOGD(TAG, "AirthingsWavePlus::client_disconnected_");

  connected_ = false;
  connecting_ = false;
}

void AirthingsWavePlus::read_sensors_() {
  auto serviceUUID = std::string("b42e1c08-ade7-11e4-89d3-123b93f75cba");
  auto sensorsDataCharacteristicUUID = std::string("b42e2a68-ade7-11e4-89d3-123b93f75cba");

  ESP_LOGD(TAG, "Getting service");

  // Obtain a reference to the service we are after in the remote BLE server.
  auto pRemoteService = client_->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    ESP_LOGE(TAG, "Failed to find service with UUID: %s", serviceUUID.c_str());
    return;
  }

  ESP_LOGD(TAG, "Found service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  auto pSensorsDataCharacteristic = pRemoteService->getCharacteristic(sensorsDataCharacteristicUUID);
  if (pSensorsDataCharacteristic == nullptr) {
    ESP_LOGE(TAG, "Failed to find characteristic UUID: %s", sensorsDataCharacteristicUUID.c_str());
    return;
  }

  ESP_LOGD(TAG, "Found characteristic");

  if (pSensorsDataCharacteristic->canRead()) {
    auto stringValue = pSensorsDataCharacteristic->readValue();
    auto value = (WavePlusReadings *) pSensorsDataCharacteristic->readRawData();

    if (sizeof(WavePlusReadings) <= stringValue.length()) {
      ESP_LOGD(TAG, "Values: %d (%d) (%p)", stringValue.length(), sizeof(WavePlusReadings),
               pSensorsDataCharacteristic->readRawData());
      ESP_LOGD(TAG, "version = %d", value->version);

      if (value->version == 1) {
        ESP_LOGD(TAG, "ambient light = %d", value->ambientLight);

        this->humidity_sensor_->publish_state(value->humidity / 2.0f);
        this->radon_sensor_->publish_state(value->radon);
        this->radon_long_term_sensor_->publish_state(value->radon_lt);
        this->temperature_sensor_->publish_state(value->temperature / 100.0f);
        this->pressure_sensor_->publish_state(value->pressure / 50.0f);
        this->co2_sensor_->publish_state(value->co2);
        this->tvoc_sensor_->publish_state(value->voc);
      } else {
        ESP_LOGE(TAG, "Characteristic UUID %s invalid version (%d != 1, newer version or not a Wave Plus?)",
                 sensorsDataCharacteristicUUID.c_str(), value->version);
      }
    } else {
      ESP_LOGE(TAG, "Characteristic UUID %s invalid data length (%d < %d), newer version or not a Wave Plus?)",
               sensorsDataCharacteristicUUID.c_str(), sizeof(WavePlusReadings), stringValue.length());
    }
  }

  client_->disconnect();
}

void AirthingsWavePlus::update_() {
  update_count_++;

  if (!client_->isConnected()) {
    auto currentTime = millis();

    auto valueDelta = currentTime - last_value_time_;
    auto connectDelta = currentTime - last_connect_time_;

    connected_ = false;

    if (update_count_ > 1 && !connecting_ && valueDelta >= refresh_interval_in_seconds_ * 1000 &&
        connectDelta >= connection_timeout_in_seconds_ * 1000) {
      auto address = BLEAddress(address_);
      ESP_LOGD(TAG, "Connecting to %s", address.toString().c_str());
      client_->connect(address);
      connecting_ = true;
      last_connect_time_ = currentTime;
    } else if (connecting_ && connectDelta >= connection_timeout_in_seconds_ * 1000) {
      ESP_LOGD(TAG, "Stop trying to connect");
      connecting_ = false;
      client_->disconnect();
    } else {
      ESP_LOGD(TAG, "Not connected (valueDelta:%lds connectDelta:%lds)", valueDelta / 1000, connectDelta / 1000);
    }
  }
}

void AirthingsWavePlus::dump_config() {
  ESP_LOGCONFIG(TAG, "AirThings Wave Plus");

  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
}

AirthingsWavePlus::AirthingsWavePlus() {
  ESP_LOGD(TAG, "AirthingsWavePlus()");

  BLEDevice::init("");

  client_ = BLEDevice::createClient();
  auto clientCallbacks =
      new WavePlusClientCallbacks([this] { this->client_connected_(); }, [this] { this->client_disconnected_(); });
  client_->setClientCallbacks(clientCallbacks);

  set_interval("connect", 10000, [this] { this->update_(); });
}

void AirthingsWavePlus::enumerate_services_() {
  //
  // May fail with corrupted heap
  //
  auto services = client_->getServices();
  ESP_LOGD(TAG, "Found %d services", services->size());
  for (auto service : *services) {
    ESP_LOGD(TAG, "Service %s", service.first.c_str());
  }
}

AirthingsWavePlus::WavePlusClientCallbacks::WavePlusClientCallbacks(std::function<void()> &&on_connected,
                                                                    std::function<void()> &&on_disconnected) {
  on_connected_ = std::move(on_connected);
  on_disconnected_ = std::move(on_disconnected);
}

void AirthingsWavePlus::WavePlusClientCallbacks::onConnect(BLEClient *p_client) {
  if (on_connected_ != nullptr) {
    on_connected_();
  }
}

void AirthingsWavePlus::WavePlusClientCallbacks::onDisconnect(BLEClient *p_client) {
  if (on_disconnected_ != nullptr) {
    on_disconnected_();
  }
}

}  // namespace airthings_wave_plus
}  // namespace esphome
