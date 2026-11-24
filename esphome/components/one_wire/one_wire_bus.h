#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome::one_wire {

enum class TransactionStatus : uint8_t {
  TRANSACTION_STATUS_OK,
  TRANSACTION_STATUS_NO_PRESENCE,
  TRANSACTION_STATUS_BUS_LOW,  ///< Reset failed: bus held low, or a bridge such as DS248x could not run the reset
  TRANSACTION_STATUS_CAPTURE_ERROR,
  TRANSACTION_STATUS_TIMEOUT,
  TRANSACTION_STATUS_HARDWARE_ERROR,
};

struct Transaction;

/// Queue state inside a Transaction. Only the bus and TransactionQueue may change it.
class TransactionLink {
 public:
  TransactionLink() = default;
  /// A copy is never queued.
  TransactionLink(const TransactionLink & /*other*/) {}
  /// Assigning a whole Transaction keeps its own queue state, so a queued one stays in its queue.
  // NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp): there is nothing to copy
  TransactionLink &operator=(const TransactionLink & /*other*/) { return *this; }

  bool is_pending() const { return this->next_ != nullptr; }

 private:
  friend class OneWireBus;
  friend class TransactionQueue;

  /// nullptr: not pending. Points to the transaction itself while it is pending and last in its queue.
  /// Keeping the pending flag in this pointer lets Transaction pack to 32 bytes on 32-bit targets.
  Transaction *next_{nullptr};
};

/// Owned by the caller, must stay valid until on_done has run.
/// Sequence on the bus: reset, ROM selection, write_data, read_data. Atomic on the bus.
/// An aggregate, so callers can use designated initializers. Leave link out of them.
/// The caller owns the Transaction and its buffers. Keep all of them valid until on_done has run, so do not use
/// buffers on the stack of the function that calls submit(). Do not change any field or the write data while
/// is_pending() is true: a bus that runs in the background reads them later.
struct Transaction {
  uint64_t address{0};  ///< 0: Skip ROM (0xCC), otherwise Match ROM (0x55)
  const uint8_t *write_data{nullptr};
  uint8_t *read_data{nullptr};  ///< Contents are only defined when the status is TRANSACTION_STATUS_OK
  void (*on_done)(void *context, TransactionStatus status){nullptr};  ///< Runs in main loop context
  void *context{nullptr};
  uint8_t write_len{0};
  uint8_t read_len{0};
  TransactionLink link;  ///< Owned by the bus

  /// True from a successful submit() until just before on_done runs.
  bool is_pending() const { return this->link.is_pending(); }
};
static_assert(sizeof(void *) != 4 || sizeof(Transaction) == 32, "Transaction should pack to 32 bytes");

/// FIFO of pending transactions for a bus that runs them in the background. Uses no heap.
class TransactionQueue {
 public:
  /// Returns false, and changes nothing, if the transaction is already pending.
  bool push(Transaction *transaction) {
    if (transaction->is_pending())
      return false;
    transaction->link.next_ = transaction;  // Last in the queue
    if (this->head_ == nullptr) {
      this->head_ = transaction;
    } else {
      this->tail_->link.next_ = transaction;
    }
    this->tail_ = transaction;
    return true;
  }
  bool empty() const { return this->head_ == nullptr; }
  Transaction *front() const { return this->head_; }
  /// Removes the front transaction and clears its pending state, so on_done may submit it again.
  /// The queue must not be empty.
  Transaction *pop() {
    Transaction *transaction = this->head_;
    Transaction *next = transaction->link.next_;
    this->head_ = next == transaction ? nullptr : next;
    transaction->link.next_ = nullptr;
    return transaction;
  }

 private:
  // Must stay in sync with the link chain
  Transaction *head_{nullptr};
  Transaction *tail_{nullptr};
};

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

  /// Main loop only. on_done runs in the main loop, possibly before submit() returns.
  /// Returns false only if this transaction is still pending. Nothing changes in that case.
  ///
  /// The default runs the transaction at once through reset_int(), write8(), write64() and read8().
  /// It detects a failed reset and reports TRANSACTION_STATUS_NO_PRESENCE or TRANSACTION_STATUS_BUS_LOW.
  /// The blocking write and read calls cannot report an error. So after a good reset the default always
  /// reports TRANSACTION_STATUS_OK, even if a byte was lost on the bus. Check read data with its CRC.
  /// A bus that can detect more errors overrides submit().
  virtual bool submit(Transaction *transaction);

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
