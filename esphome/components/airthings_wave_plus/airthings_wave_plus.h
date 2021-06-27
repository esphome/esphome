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
  void set_update_interval(uint32_t update_interval);

  void set_temperature(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }
  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }
  void set_humidity(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_pressure(sensor::Sensor *pressure) { pressure_sensor_ = pressure; }
  void set_co2(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_sensor_ = tvoc; }

 protected:
  uint32_t connection_timeout_in_seconds_ = 30;
  uint32_t update_interval_in_seconds_ = 300;

  uint32_t update_count_ = 0;
  bool connected_ = false;
  bool connecting_ = false;
  int32_t last_value_time_;
  int32_t last_connect_time_ = connection_timeout_in_seconds_ * -1000;

  void update_();
  void enumerate_services_();
  void client_connected_();
  void client_disconnected_();
  void read_sensors_();
  boolean isValidRadonValue_(short radon);
  boolean isValidVocValue_(short voc);
  boolean isValidCo2Value_(short co2);

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
    WavePlusClientCallbacks(std::function<void()> &&on_connected, std::function<void()> &&on_disconnected);
    void onConnect(BLEClient *p_client) override;
    void onDisconnect(BLEClient *p_client) override;

   protected:
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;
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
