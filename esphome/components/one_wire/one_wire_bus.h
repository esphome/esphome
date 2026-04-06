#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace one_wire {

class OneWireBus {
 public:
  /// Write a word to the bus. LSB first.
  virtual void write8(uint8_t val) = 0;

  /// Write a 64 bit unsigned integer to the bus. LSB first.
  virtual void write64(uint64_t val) = 0;

  /// Write a command to the bus that addresses all devices by skipping the ROM.
  void skip();

  /// Read an 8 bit word from the bus.
  virtual uint8_t read8() = 0;

  /// Read an 64-bit unsigned integer from the bus.
  virtual uint64_t read64() = 0;

  /// Select a specific address on the bus for the following command.
  bool select(uint64_t address);

  /// Return the list of found devices.
  const std::vector<uint64_t> &get_devices();

  /// Search for 1-Wire devices on the bus.
  void search();

  /// Get the description string for this model.
  const LogString *get_model_str(uint8_t model);

 protected:
  std::vector<uint64_t> devices_;

  /// log the found devices
  void dump_devices_(const char *tag);

  /** Reset the bus, should be done before all write operations.
   *
   * Takes approximately 1ms.
   *
   * @return Whether the operation was successful.
   */
  bool reset_();

  /**
   * Bus Reset
   * @return -1: signal fail, 0: no device detected, 1: device detected
   */
  virtual int reset_int() = 0;

  /// Reset the device search.
  virtual void reset_search() = 0;

  /// Search for a 1-Wire device on the bus. Returns 0 if all devices have been found.
  virtual uint64_t search_int() = 0;
};

}  // namespace one_wire
}  // namespace esphome
