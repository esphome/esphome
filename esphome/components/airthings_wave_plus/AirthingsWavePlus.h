#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"

#include <BLEDevice.h>
#include <iterator>
#include <algorithm>

namespace esphome {
namespace airthings_wave_plus {

static const char *TAG = "airthings_wave_plus";

class AirthingsWavePlus : public Component {
 public:
  AirthingsWavePlus();

  void dump_config() override;
  void set_address(std::string address) { address_ = address; }

  void set_temperature(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }
  void set_humidity(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_pressure(sensor::Sensor *pressure) { pressure_sensor_ = pressure; }
  void set_co2(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_sensor_ = tvoc; }

 protected:
  int _connectionTimeoutInSeconds = 30;
  int _refreshIntervalInSeconds = 300;

  int _updateCount = 0;
  bool _connected = false;
  bool _connecting = false;
  int _lastTime = _refreshIntervalInSeconds * -1000;
  int _lastConnectTime = _connectionTimeoutInSeconds * -1000;

  void update();
  void enumerateServices();
  void clientConnected();
  void clientDisconnected();
  void readSensors();

  BLEClient *client_;
  std::string address_;
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};

  class WavePlusClientCallbacks : public BLEClientCallbacks {
   public:
    WavePlusClientCallbacks(std::function<void()> &&onConnected, std::function<void()> &&onDisconnected);
    void onConnect(BLEClient *pClient) override;
    void onDisconnect(BLEClient *pClient) override;

   protected:
    std::function<void()> onConnected_;
    std::function<void()> onDisconnected_;
  };

  struct WavePlusReadings {
    unsigned char version;
    unsigned char humidity;
    unsigned char ambientLight;
    unsigned char unused01;
    unsigned short radon;
    unsigned short radon_lt;
    unsigned short temperature;
    unsigned short pressure;
    unsigned short co2;
    unsigned short voc;
  };
};

}  // namespace airthings_wave_plus
}  // namespace esphome
