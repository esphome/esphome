#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ONE_WIRE_RMT

#include <atomic>

#include "esphome/components/one_wire/one_wire_bus.h"
#include "rmt_one_wire_codec.h"

namespace esphome::gpio::rmt_one_wire {

class Port {
 public:
  virtual bool line_is_high() = 0;
  /// Arms RX. False: hardware error.
  virtual bool arm_rx(Symbol *buffer, size_t capacity, uint32_t idle_us) = 0;
  /// Starts TX. count never exceeds max_tx_slots. False: hardware error.
  virtual bool start_tx(const Symbol *symbols, size_t count) = 0;
  /// Stops both channels; no further completion belongs to the aborted section. False: hardware error.
  virtual bool abort() = 0;
  virtual uint32_t now_ms() = 0;
  /// Sleeps, at most max_ms; may return early. Only used by the blocking methods, which then check the flags.
  virtual void wait_for_event(uint32_t max_ms) = 0;
};

class Engine {
 public:
  static constexpr size_t MAX_MEM_WORDS = 64;  // ESP32 and S2, the others have 48

  /// RX idle outside RESET: max(100, slot + 20) us. The RESET section keeps 600 us.
  Engine(Port *port, size_t mem_words, const Timing &timing = {});

  // Main loop only
  bool submit(one_wire::Transaction *transaction);
  /// Does nothing while is_busy(): a log handler inside the engine may call back into the bus.
  void poll();
  bool is_idle() const { return this->queue_.empty(); }
  /// True while poll() or a blocking method runs, except inside on_done.
  bool is_busy() const { return this->busy_; }
  void set_failed();  ///< All pending and future work ends in HARDWARE_ERROR

  // ISR safe: store and return. True if this was the last expected event of the section: wake the main loop.
  bool on_tx_done();
  bool on_rx_done(size_t num_symbols);

  // Blocking facade. Each first lets the transaction that is on the bus finish, then runs ahead of the rest of the
  // FIFO, and sleeps through Port::wait_for_event. Called while is_busy(), each fails at once without bus access.
  /// 1 present, 0 no device or any failure, -1 only when the bus is held Low (OneWireBus logs that case).
  int reset_blocking();
  /// False on any failure.
  bool write_blocking(const uint8_t *data, size_t num_bits);
  /// On failure, every bit of data is set to 1 and the result is false.
  bool read_blocking(uint8_t *data, size_t num_bits);
  void reset_search();
  uint64_t search_next();  ///< Same contract as OneWireBus::search_int(): 0 when finished or on failure

 protected:
  /// Where the transaction at the head of the FIFO stands.
  enum class Step : uint8_t { LINE_UNCHECKED, RESET, WRITE, READ };

  static constexpr uint32_t PENDING_TX = 1;
  static constexpr uint32_t PENDING_RX = 2;

  void poll_();
  void start_reset_();
  void section_done_();
  /// decode_read_slots() on the capture, shows at VERBOSE how far the bits were from read_threshold.
  bool decode_read_(size_t num_bits, uint8_t *out, size_t bit_offset);
  void finish_(one_wire::TransactionStatus status);
  /// Sends tx_[0, count), with RX unless idle_us is NO_RX. False: hardware error, nothing is running.
  bool start_section_(size_t count, uint32_t idle_us);
  /// False: permanent hardware error, the engine is failed from now on.
  bool abort_section_();

  /// Finishes the transaction on the bus, if any, but starts no other.
  void drain_();
  /// One whole section, sleeping. False on any failure, nothing is running then.
  bool run_section_(size_t count, uint32_t idle_us);

  Port *port_;
  size_t mem_words_;
  Timing timing_;
  uint32_t rx_idle_us_;
  bool failed_{false};
  /// Set by poll() and the blocking methods, cleared around on_done. Guards against calls from log handlers.
  bool busy_{false};

  one_wire::TransactionQueue queue_;

  Step step_{Step::LINE_UNCHECKED};
  uint32_t since_ms_{0};  ///< Start of the running section
  uint8_t rom_selection_[9]{};
  size_t rom_bits_{0};
  size_t write_pos_{0};   ///< Slots sent of ROM selection + write_data
  size_t read_pos_{0};    ///< Slots sent of read_data
  size_t read_slots_{0};  ///< Size of the running READ section

  /// Completions the running section still waits for. Whatever is not expected here is discarded.
  std::atomic<uint32_t> pending_{0};
  std::atomic<size_t> rx_count_{0};

  uint64_t search_address_{0};
  uint8_t last_discrepancy_{0};
  bool last_device_{false};

  Symbol tx_[MAX_MEM_WORDS];
  Symbol rx_[MAX_MEM_WORDS];
};

}  // namespace esphome::gpio::rmt_one_wire

#endif  // USE_ONE_WIRE_RMT
