#pragma once
#include <cstdint>
#include "families.h"
#ifdef ESP_PLATFORM
#include "esphome/core/defines.h"
#elif !defined(ONE_WIRE_DEVICE_SELECTED_MODEL)
// Standalone native protocol checks exercise every model. Firmware defines only its selection.
#define USE_ONE_WIRE_DEVICE_DS18B20
#define USE_ONE_WIRE_DEVICE_DS1822
#define USE_ONE_WIRE_DEVICE_DS18S20
#define USE_ONE_WIRE_DEVICE_DS2401
#define USE_ONE_WIRE_DEVICE_DS2413
#endif
#if defined(USE_ONE_WIRE_DEVICE_DS18B20) || defined(USE_ONE_WIRE_DEVICE_DS1822) || defined(USE_ONE_WIRE_DEVICE_DS18S20)
#define USE_ONE_WIRE_DEVICE_THERMOMETER
#endif
#ifdef USE_ESP32
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#endif

namespace esphome::one_wire_device {

// GPIO-independent bit-slot model. Only the protocol ISR accesses this state.
// Copy/Recall use a RAM shadow, not persistent EEPROM. No allocation or callbacks.
class DeviceModel {
 public:
  void configure(uint8_t family);
  void set_rom(uint64_t address);
  void IRAM_ATTR reset();
  bool IRAM_ATTR is_transmitting() const;
  bool IRAM_ATTR transmit_bit(uint8_t inputs);
  void IRAM_ATTR receive_bit(bool bit, int16_t temperature, uint8_t inputs);
  bool IRAM_ATTR needs_pio_sample() const {
#ifdef USE_ONE_WIRE_DEVICE_DS2413
    return this->phase_ == Phase::PIO_READ && this->position_ == 0;
#else
    return false;
#endif
  }
  bool IRAM_ATTR is_thermometer() const {
    switch (this->family_) {
#ifdef USE_ONE_WIRE_DEVICE_DS18B20
      case FAMILY_DS18B20:
#endif
#ifdef USE_ONE_WIRE_DEVICE_DS1822
      case FAMILY_DS1822:
#endif
#ifdef USE_ONE_WIRE_DEVICE_DS18S20
      case FAMILY_DS18S20:
#endif
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
        return true;
#endif
      default:
        return false;
    }
  }

 protected:
  enum class Phase : uint8_t {
    ROM,
    FUNCTION,
    MATCH,
    SEARCH,
    COMPLEMENT,
    BRANCH,
    READ_ROM,
    READ_PAD,
    WRITE_PAD,
    READY,
    IGNORE,
    PIO_READ,
    PIO_WRITE,
    PIO_REPLY
  };
  void IRAM_ATTR command_(uint8_t command, int16_t temperature, uint8_t inputs);
  bool IRAM_ATTR has_alarm_() const {
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
    return this->is_thermometer() && this->alarm_;
#else
    return false;
#endif
  }
  bool IRAM_ATTR is_ds18s20_() const {
#ifdef USE_ONE_WIRE_DEVICE_DS18S20
    return this->family_ == FAMILY_DS18S20;
#else
    return false;
#endif
  }
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
  void IRAM_ATTR update_crc_();
#endif
#ifdef USE_ONE_WIRE_DEVICE_DS2413
  uint8_t IRAM_ATTR pio_status_(uint8_t inputs) const;
#endif
  uint8_t family_{FAMILY_DS18B20};
  uint8_t rom_[8]{};
#ifdef USE_ONE_WIRE_DEVICE_THERMOMETER
  uint8_t pad_[9]{};
  uint8_t saved_[3]{};
  bool alarm_{false};
#endif
  uint8_t pending_[3]{};
  Phase phase_{Phase::IGNORE};
  uint8_t rx_{0}, rx_bits_{0}, position_{0}, write_position_{0};
#ifdef USE_ONE_WIRE_DEVICE_DS2413
  uint8_t pio_latch_{3}, pio_reply_[2]{};
#endif
};

}  // namespace esphome::one_wire_device
