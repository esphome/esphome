#pragma once

#include <cstdint>
#include <vector>

#include "esphome/components/aranet/aranet.h"

namespace esphome::aranet::testing {

constexpr uint8_t MAC_ADDRESS[6] = {0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
constexpr uint64_t ADDRESS = 0x001122334455ULL;

inline const std::vector<uint8_t> ARANET4_PAYLOAD = {
    0x20,                                                  // Smart Home integrations enabled
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x02,  // CO2: 600 ppm
    0xA4, 0x01,                                            // Temperature: 21.0 C (420 / 20)
    0x94, 0x27,                                            // Pressure: 1013.2 hPa (10132 / 10)
    45,                                                    // Humidity: 45%
    87,                                                    // Battery: 87%
    0x00,                                                  // Display status
    0x2C, 0x01,                                            // Measurement interval: 300 s
    12,   0x00,                                            // Measurement age: 12 s
    7,                                                     // Measurement counter
};

inline const std::vector<uint8_t> ARANET2_PAYLOAD = {0x01, 0x21, 0x04, 0x04, 0x01, 0x00, 0x00, 0x00,
                                                     0x00, 0x00, 0x99, 0x01, 0x00, 0x00, 0x0A, 0x02,
                                                     0x00, 0x3B, 0x09, 0x78, 0x00, 0x52, 0x00, 0x64};

inline const std::vector<uint8_t> RADIATION_PAYLOAD = {0x02, 0x21, 0x26, 0x04, 0x01, 0x00, 0xD0, 0x33,
                                                       0x00, 0x00, 0x6C, 0x60, 0x06, 0x00, 0x82, 0x00,
                                                       0x00, 0x63, 0x00, 0x2C, 0x01, 0x58, 0x00, 0x72};

inline const std::vector<uint8_t> RADON_PAYLOAD = {0x03, 0x21, 0x04, 0x06, 0x01, 0x00, 0x00, 0x00,
                                                   0x07, 0x00, 0xFE, 0x01, 0xC9, 0x27, 0xCE, 0x01,
                                                   0x00, 0x64, 0x01, 0x58, 0x02, 0xF6, 0x01, 0x08};

inline ble_device_base::ESPBTDevice make_device(const std::vector<uint8_t> &payload, int rssi = -61,
                                                const uint8_t *mac_address = MAC_ADDRESS) {
  // Manufacturer specific data AD structure for company id 0x0702
  std::vector<uint8_t> advertisement{static_cast<uint8_t>(payload.size() + 3), 0xFF, 0x02, 0x07};
  advertisement.insert(advertisement.end(), payload.begin(), payload.end());

  ble_device_base::ESPBTDevice device;
  device.from_scan_result(mac_address, rssi, 0, advertisement.data(), advertisement.size());
  return device;
}

struct Harness {
  Harness() {
    this->aranet.set_co2_sensor(&this->co2);
    this->aranet.set_temperature_sensor(&this->temperature);
    this->aranet.set_humidity_sensor(&this->humidity);
    this->aranet.set_pressure_sensor(&this->pressure);
    this->aranet.set_battery_level_sensor(&this->battery);
    this->aranet.set_measurement_interval_sensor(&this->interval);
    this->aranet.set_measurement_age_sensor(&this->age);
    this->aranet.set_signal_strength_sensor(&this->signal_strength);
    this->aranet.set_radon_sensor(&this->radon);
    this->aranet.set_radiation_rate_sensor(&this->radiation_rate);
    this->aranet.set_radiation_total_sensor(&this->radiation_total);
    this->aranet.set_radiation_duration_sensor(&this->radiation_duration);
  }

  Aranet aranet{ADDRESS};
  sensor::Sensor co2, temperature, humidity, pressure, battery, interval, age, signal_strength, radon, radiation_rate,
      radiation_total, radiation_duration;
};

}  // namespace esphome::aranet::testing
