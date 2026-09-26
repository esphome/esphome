#pragma once

#include <array>
#include <cstdint>

namespace esphome::aranet::testing {

constexpr uint8_t MAC_ADDRESS[6] = {0x55, 0x44, 0x33, 0x22, 0x11, 0x00};

inline std::array<uint8_t, 22> make_payload() {
  return {
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
}

}  // namespace esphome::aranet::testing
