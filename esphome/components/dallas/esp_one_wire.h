#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"

namespace esphome {
namespace dallas {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class ESPOneWireBase {
 public:
  explicit ESPOneWireBase() {}

  /** Reset the bus, should be done before all write operations.
   *
   * Takes approximately 1ms.
   *
   * @return Whether the operation was successful.
   */
  virtual bool reset() = 0;

  /// Write a single bit to the bus, takes about 70µs.
  virtual void write_bit(bool bit) = 0;

  /// Read a single bit from the bus, takes about 70µs
  virtual bool read_bit();

  /// Write a word to the bus. LSB first.
  void write8(uint8_t val);

  /// Write a 64 bit unsigned integer to the bus. LSB first.
  void write64(uint64_t val);

  /// Write a command to the bus that addresses all devices by skipping the ROM.
  void skip();

  /// Read an 8 bit word from the bus.
  uint8_t read8();

  /// Read an 64-bit unsigned integer from the bus.
  uint64_t read64();

  /// Select a specific address on the bus for the following command.
  void select(uint64_t address);

  /// Reset the device search.
  void reset_search();

  /// Search for a 1-Wire device on the bus. Returns 0 if all devices have been found.
  uint64_t search();

  /// Helper that wraps search in a std::vector.
  std::vector<uint64_t> search_vec();

  // Removed check if it is usefull
  // GPIOPin *get_pin();

 protected:
  /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  inline uint8_t *rom_number8_();

  uint8_t last_discrepancy_{0};
  uint8_t last_family_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t rom_number_{0};
};

class ESPOneWire : public ESPOneWireBase {
 public:
  explicit ESPOneWire(GPIOPin *pin);

  bool reset() override;

  /// Write a single bit to the bus, takes about 70µs.
  void write_bit(bool bit) override;

  /// Read a single bit from the bus, takes about 70µs
  bool read_bit() override;

 protected:
  GPIOPin *pin_;
};

class ShellyOneWire : public ESPOneWireBase {
 public:
  explicit ShellyOneWire(GPIOPin *pin_a, GPIOPin *pin_b);

  bool reset() override;

  /// Write a single bit to the bus, takes about 70µs.
  void write_bit(bool bit) override;

  /// Read a single bit from the bus, takes about 70µs
  bool read_bit() override;

 protected:
  GPIOPin *in_pin_;
  GPIOPin *out_pin_;
};

}  // namespace dallas
}  // namespace esphome
