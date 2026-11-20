#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome::one_wire {

#ifdef USE_ONE_WIRE_RMT
enum class TransactionStatus : uint8_t { OK, NO_PRESENCE, BUS_LOW, CAPTURE_ERROR, TIMEOUT, HARDWARE_ERROR };

/// Owned by the caller, must stay valid until on_done has run.
/// Sequence on the bus: reset, ROM selection, write_data, read_data. Atomic on the bus.
struct Transaction {
  uint64_t address{0};  ///< 0: Skip ROM (0xCC), otherwise Match ROM (0x55)
  const uint8_t *write_data{nullptr};
  uint8_t write_len{0};
  uint8_t *read_data{nullptr};  ///< Contents are only defined when the status is OK
  uint8_t read_len{0};
  void (*on_done)(void *context, TransactionStatus status){nullptr};  ///< Runs in main loop context
  void *context{nullptr};
  Transaction *next_{nullptr};  ///< Intrusive FIFO, no heap
  bool pending_{false};
};
#endif  // USE_ONE_WIRE_RMT

class OneWireBus {
 public:
  /// Write a word to the bus. LSB first.
  virtual void write8(uint8_t val) = 0;

  /// Write a 64 bit unsigned integer to the bus. LSB first.
  virtual void write64(uint64_t val) = 0;

  /// Write a command to the bus that addresses all devices by skipping the ROM.
  /// Returns true if a device presence pulse is detected.
  bool skip();

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

#ifdef USE_ONE_WIRE_RMT
  /// Returns false only if this transaction is still pending; nothing changes in that case.
  /// May run on_done before returning. Default: runs it through the blocking primitives.
  virtual bool submit(Transaction *transaction);
#endif  // USE_ONE_WIRE_RMT

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

}  // namespace esphome::one_wire
