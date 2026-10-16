#pragma once
#include <cstdint>
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
  bool IRAM_ATTR is_thermometer() const {
    return this->family_ == 0x28 || this->family_ == 0x22 || this->family_ == 0x10;
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
  void IRAM_ATTR update_crc_();
  uint8_t IRAM_ATTR pio_status_(uint8_t inputs) const;
  uint8_t family_{0x28};
  uint8_t rom_[8]{};
  uint8_t pad_[9]{};
  uint8_t saved_[3]{};
  uint8_t pending_[3]{};
  Phase phase_{Phase::IGNORE};
  uint8_t rx_{0}, rx_bits_{0}, position_{0}, write_position_{0};
  uint8_t pio_latch_{3}, pio_reply_[2]{};
  bool alarm_{false};
};

}  // namespace esphome::one_wire_device
