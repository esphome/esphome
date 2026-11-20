#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome::one_wire {

#ifdef USE_ONE_WIRE_TRANSACTION
enum class TransactionStatus : uint8_t {
  TRANSACTION_STATUS_OK,
  TRANSACTION_STATUS_NO_PRESENCE,
  TRANSACTION_STATUS_BUS_LOW,
  TRANSACTION_STATUS_CAPTURE_ERROR,
  TRANSACTION_STATUS_TIMEOUT,
  TRANSACTION_STATUS_HARDWARE_ERROR,
};

/// Owned by the caller, must stay valid until on_done has run.
/// Sequence on the bus: reset, ROM selection, write_data, read_data. Atomic on the bus.
class Transaction {
 public:
  uint64_t address{0};  ///< 0: Skip ROM (0xCC), otherwise Match ROM (0x55)
  const uint8_t *write_data{nullptr};
  uint8_t *read_data{nullptr};  ///< Contents are only defined when the status is TRANSACTION_STATUS_OK
  void (*on_done)(void *context, TransactionStatus status){nullptr};  ///< Runs in main loop context
  void *context{nullptr};
  uint8_t write_len{0};
  uint8_t read_len{0};

  /// True from a successful submit() until just before on_done runs.
  bool is_pending() const { return this->pending_; }

 private:
  friend class OneWireBus;
  friend class TransactionQueue;

  // The bus owns these two. Placed after the byte fields so the struct packs to 32 bytes on 32-bit targets.
  bool pending_{false};
  Transaction *next_{nullptr};  ///< Intrusive FIFO, no heap
};
static_assert(sizeof(void *) != 4 || sizeof(Transaction) == 32, "Transaction should pack to 32 bytes");

/// FIFO of pending transactions for a bus that runs them in the background. Uses no heap.
class TransactionQueue {
 public:
  /// Returns false, and changes nothing, if the transaction is already pending.
  bool push(Transaction *transaction) {
    if (transaction->pending_)
      return false;
    transaction->pending_ = true;
    transaction->next_ = nullptr;
    if (this->head_ == nullptr) {
      this->head_ = transaction;
    } else {
      this->tail_->next_ = transaction;
    }
    this->tail_ = transaction;
    return true;
  }
  bool empty() const { return this->head_ == nullptr; }
  Transaction *front() const { return this->head_; }
  /// Removes the front transaction and clears its pending flag, so on_done may submit it again.
  /// The queue must not be empty.
  Transaction *pop() {
    Transaction *transaction = this->head_;
    this->head_ = transaction->next_;
    transaction->next_ = nullptr;
    transaction->pending_ = false;
    return transaction;
  }

 private:
  // Must stay in sync with the next_ links
  Transaction *head_{nullptr};
  Transaction *tail_{nullptr};
};
#endif  // USE_ONE_WIRE_TRANSACTION

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

#ifdef USE_ONE_WIRE_TRANSACTION
  /// Main loop only. on_done runs in the main loop, possibly before submit() returns.
  /// Returns false only if this transaction is still pending; nothing changes in that case.
  /// Default: runs the transaction at once through the blocking primitives.
  virtual bool submit(Transaction *transaction);
#endif  // USE_ONE_WIRE_TRANSACTION

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
