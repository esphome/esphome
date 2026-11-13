#pragma once
#include <cstdint>
namespace esphome::one_wire_device {
static constexpr uint8_t FAMILY_DS18B20 = 0x28;
static constexpr uint8_t FAMILY_DS1822 = 0x22;
static constexpr uint8_t FAMILY_DS18S20 = 0x10;
static constexpr uint8_t FAMILY_DS2401 = 0x01;
static constexpr uint8_t FAMILY_DS2413 = 0x3a;
}  // namespace esphome::one_wire_device
