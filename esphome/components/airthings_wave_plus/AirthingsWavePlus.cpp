#include "AirthingsWavePlus.h"

namespace esphome {
namespace airthings_wave_plus {

void AirthingsWavePlus::clientConnected() {
  ESP_LOGD(TAG, "AirthingsWavePlus::clientConnected");

  _connected = true;
  _connecting = false;
  _lastTime = millis();

  // Schedule to avoid a deadlock
  set_timeout(0, [this] { this->readSensors(); });
}

void AirthingsWavePlus::clientDisconnected() {
  ESP_LOGD(TAG, "AirthingsWavePlus::clientDisconnected");

  _connected = false;
  _connecting = false;
}

void AirthingsWavePlus::readSensors() {
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

void AirthingsWavePlus::update() {
  _updateCount++;

  if (!client_->isConnected()) {
    auto currentTime = millis();

    auto valueDelta = currentTime - _lastTime;
    auto connectDelta = currentTime - _lastConnectTime;

    _connected = false;

    if (_updateCount > 1 && !_connecting && valueDelta >= _refreshIntervalInSeconds * 1000 &&
        connectDelta >= _connectionTimeoutInSeconds * 1000) {
      auto address = BLEAddress(address_);
      ESP_LOGD(TAG, "Connecting to %s", address.toString().c_str());
      client_->connect(address);
      _connecting = true;
      _lastConnectTime = currentTime;
    } else if (_connecting && connectDelta >= _connectionTimeoutInSeconds * 1000) {
      ESP_LOGD(TAG, "Stop trying to connect");
      _connecting = false;
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
      new WavePlusClientCallbacks([this] { this->clientConnected(); }, [this] { this->clientDisconnected(); });
  client_->setClientCallbacks(clientCallbacks);

  set_interval("connect", 10000, [this] { this->update(); });
}

void AirthingsWavePlus::enumerateServices() {
  //
  // May fail with corrupted heap
  //
  auto services = client_->getServices();
  ESP_LOGD(TAG, "Found %d services", services->size());
  for (auto service : *services) {
    ESP_LOGD(TAG, "Service %s", service.first.c_str());
  }
}

AirthingsWavePlus::WavePlusClientCallbacks::WavePlusClientCallbacks(std::function<void()> &&onConnected,
                                                                    std::function<void()> &&onDisconnected) {
  onConnected_ = std::move(onConnected);
  onDisconnected_ = std::move(onDisconnected);
}

void AirthingsWavePlus::WavePlusClientCallbacks::onConnect(BLEClient *pClient) {
  if (onConnected_ != nullptr) {
    onConnected_();
  }
}

void AirthingsWavePlus::WavePlusClientCallbacks::onDisconnect(BLEClient *pClient) {
  if (onDisconnected_ != nullptr) {
    onDisconnected_();
  }
}

}  // namespace airthings_wave_plus
}  // namespace esphome
