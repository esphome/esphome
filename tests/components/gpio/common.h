#pragma once

// Simulated 1-Wire bus and mock Port for the RMT engine tests in one_wire/.
//
// Lives directly in tests/components/gpio/ on purpose: script/cpp_unit_test.py only picks up a
// component whose test folder has a .cpp or .h at its top level.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "esphome/components/gpio/one_wire/rmt_one_wire_codec.h"
#include "esphome/components/gpio/one_wire/rmt_one_wire_engine.h"
#include "esphome/components/one_wire/one_wire_bus.h"

namespace esphome::gpio::rmt_one_wire::testing {

using one_wire::Transaction;
using one_wire::TransactionStatus;

// ---------------------------------------------------------------------------------------------
// Own table of the timing windows of spec section 3 (TI TMP1826 table 7.6 intersected with the
// DS18B20 data sheet). Deliberately not taken from the constexpr values of the code (I12).
// ---------------------------------------------------------------------------------------------
struct UsWindow {
  uint32_t min;
  uint32_t max;
  bool contains(uint32_t us) const { return us >= this->min && us <= this->max; }
};
constexpr uint32_t NO_MAX = std::numeric_limits<uint32_t>::max();
constexpr UsWindow WINDOW_RESET_LOW{480, 560};      // tRSTL
constexpr UsWindow WINDOW_RESET_HIGH{480, NO_MAX};  // tRSTH
constexpr UsWindow WINDOW_WRITE0_LOW{60, 120};      // tWR0L, tLOW0
constexpr UsWindow WINDOW_WRITE1_LOW{2, 15};        // tWR1L 2-15 and tLOW1 1-15
constexpr UsWindow WINDOW_READ_LOW{3, 5};           // tRL 2.5-5 in whole microseconds
constexpr UsWindow WINDOW_SLOT{60, 120};            // tSLOT of the DS18B20
constexpr uint32_t BUDGET_TRC_US = 9;               // rise time budget of section 3
constexpr uint32_t MIN_RECOVERY_US = 2;             // tREC of the TI part
constexpr uint32_t SAMPLE_WRITE1_US = 15;           // 6 + tRC <= 15
constexpr uint32_t SAMPLE_READ_US = 13;             // 4 + tRC <= 13

// ---------------------------------------------------------------------------------------------
// Classification of transmitted symbols, again from section 3 only
// ---------------------------------------------------------------------------------------------
enum class Slot : uint8_t { READ, WRITE_1, WRITE_0, BAD };

inline Slot classify_slot(const Symbol &symbol) {
  if (symbol.level0 != 0 || symbol.level1 != 1)
    return Slot::BAD;
  if (WINDOW_READ_LOW.contains(symbol.duration0))
    return Slot::READ;
  if (WINDOW_WRITE1_LOW.contains(symbol.duration0))
    return Slot::WRITE_1;
  if (WINDOW_WRITE0_LOW.contains(symbol.duration0))
    return Slot::WRITE_0;
  return Slot::BAD;
}

/// A reset is told from a slot by its length; the shortest wrong reset is far above any slot.
inline bool is_reset_symbol(const Symbol &symbol) { return symbol.duration0 >= 300; }

inline std::string slots_to_string(const std::vector<Slot> &slots) {
  std::string out;
  for (auto slot : slots) {
    switch (slot) {
      case Slot::READ:
        out += 'R';
        break;
      case Slot::WRITE_1:
        out += '1';
        break;
      case Slot::WRITE_0:
        out += '0';
        break;
      case Slot::BAD:
        out += '?';
        break;
    }
  }
  return out;
}

inline void append_write_slots(std::vector<Slot> &slots, uint8_t byte) {
  for (int bit = 0; bit < 8; bit++)
    slots.push_back((byte >> bit) & 1 ? Slot::WRITE_1 : Slot::WRITE_0);
}

/// Slots of one transaction after its reset: ROM selection, write_data, read_data.
inline std::vector<Slot> expected_slots(uint64_t address, const std::vector<uint8_t> &write, size_t read_len) {
  std::vector<Slot> slots;
  if (address == 0) {
    append_write_slots(slots, 0xCC);
  } else {
    append_write_slots(slots, 0x55);
    for (int i = 0; i < 8; i++)
      append_write_slots(slots, static_cast<uint8_t>(address >> (8 * i)));
  }
  for (auto byte : write)
    append_write_slots(slots, byte);
  slots.insert(slots.end(), read_len * 8, Slot::READ);
  return slots;
}

// ---------------------------------------------------------------------------------------------
// Heap watch for "kein Heap" (rule set of section 4.4 and section 2). The counting operator new
// is defined in rmt_one_wire_engine_test.cpp. Mock and test code pause the watch, so only
// allocations of the code under test count.
// ---------------------------------------------------------------------------------------------
inline bool g_heap_watch{false};
inline size_t g_heap_allocs{0};

struct HeapWatch {
  HeapWatch() {
    g_heap_allocs = 0;
    g_heap_watch = true;
  }
  ~HeapWatch() { g_heap_watch = false; }
};

struct HeapPause {
  HeapPause() : previous(g_heap_watch) { g_heap_watch = false; }
  ~HeapPause() { g_heap_watch = this->previous; }
  bool previous;
};

// ---------------------------------------------------------------------------------------------
// Simulated devices and the wire
// ---------------------------------------------------------------------------------------------

/// One line phase of the wired-AND bus.
struct Phase {
  bool low;
  uint32_t us;
};

/// Behaves like a DS18B20 at the protocol level: ROM commands, Match/Skip ROM, Search ROM and,
/// once selected, function_bytes received bytes followed by sending payload LSB first.
class SimDevice {
 public:
  explicit SimDevice(uint64_t rom_id, std::vector<uint8_t> payload_bytes = {}, size_t function_byte_count = 0)
      : rom(rom_id), payload(std::move(payload_bytes)), function_bytes(function_byte_count) {}

  uint64_t rom;
  std::vector<uint8_t> payload;
  size_t function_bytes;
  uint32_t presence_wait_us{30};  ///< tPDH 15-60
  uint32_t presence_low_us{120};  ///< tPDL 60-240
  uint32_t zero_low_us{30};       ///< how long this device holds a 0 from the falling edge

  void on_reset() {
    this->state_ = State::ROM_COMMAND;
    this->count_ = 0;
    this->accumulator_ = 0;
    this->received_ = {};
    this->sent_bits_ = 0;
  }

  bool drives_low() const {
    switch (this->state_) {
      case State::SEARCH_BIT:
        return !this->rom_bit_();
      case State::SEARCH_COMPLEMENT:
        return this->rom_bit_();
      case State::SENDING: {
        size_t index = this->sent_bits_ / 8;
        if (index >= this->payload.size())
          return false;
        return ((this->payload[index] >> (this->sent_bits_ % 8)) & 1) == 0;
      }
      default:
        return false;
    }
  }

  void on_slot(bool master_bit) {
    switch (this->state_) {
      case State::ROM_COMMAND:
        this->accumulator_ |= uint64_t(master_bit) << this->count_;
        if (++this->count_ == 8) {
          uint8_t command = static_cast<uint8_t>(this->accumulator_);
          this->count_ = 0;
          this->accumulator_ = 0;
          if (command == 0xCC) {
            this->enter_function_();
          } else if (command == 0x55) {
            this->state_ = State::MATCH;
          } else if (command == 0xF0) {
            this->state_ = State::SEARCH_BIT;
          } else {
            this->state_ = State::IDLE;
          }
        }
        break;
      case State::MATCH:
        this->accumulator_ |= uint64_t(master_bit) << this->count_;
        if (++this->count_ == 64) {
          if (this->accumulator_ == this->rom) {
            this->enter_function_();
          } else {
            this->state_ = State::IDLE;
          }
        }
        break;
      case State::SEARCH_BIT:
        this->state_ = State::SEARCH_COMPLEMENT;
        break;
      case State::SEARCH_COMPLEMENT:
        this->state_ = State::SEARCH_DIRECTION;
        break;
      case State::SEARCH_DIRECTION:
        if (master_bit != this->rom_bit_()) {
          this->state_ = State::IDLE;
        } else if (++this->count_ == 64) {
          this->state_ = State::IDLE;
        } else {
          this->state_ = State::SEARCH_BIT;
        }
        break;
      case State::FUNCTION:
        this->accumulator_ |= uint64_t(master_bit) << (this->count_ % 8);
        if (++this->count_ % 8 == 0) {
          this->received_.push_back(static_cast<uint8_t>(this->accumulator_));
          this->accumulator_ = 0;
          if (this->received_.size() == this->function_bytes)
            this->state_ = State::SENDING;
        }
        break;
      case State::SENDING:
        this->sent_bits_++;
        break;
      case State::IDLE:
        break;
    }
  }

  /// Bytes received after the ROM selection since the last reset.
  const std::vector<uint8_t> &received() const { return this->received_; }

 protected:
  enum class State : uint8_t {
    IDLE,
    ROM_COMMAND,
    MATCH,
    SEARCH_BIT,
    SEARCH_COMPLEMENT,
    SEARCH_DIRECTION,
    FUNCTION,
    SENDING
  };

  bool rom_bit_() const { return (this->rom >> this->count_) & 1; }
  void enter_function_() {
    this->count_ = 0;
    this->accumulator_ = 0;
    this->state_ = this->function_bytes == 0 ? State::SENDING : State::FUNCTION;
  }

  State state_{State::IDLE};
  uint32_t count_{0};
  uint64_t accumulator_{0};
  std::vector<uint8_t> received_;
  size_t sent_bits_{0};
};

/// Devices need at least tRSTL(min) to see a reset, sample a written bit at about 15 us.
constexpr uint32_t DEVICE_RESET_DETECT_US = 480;
constexpr uint32_t DEVICE_SAMPLE_US = 15;

class SimBus {
 public:
  std::deque<SimDevice> devices;
  uint32_t rise_us{3};  ///< tRC: how long the pullup takes after the master released the line

  /// Puts one master symbol on the wire. Appends the resulting line waveform.
  void drive(const Symbol &symbol, std::vector<Phase> *out) {
    const uint32_t low = symbol.duration0;
    const uint32_t high = symbol.duration1;
    if (low >= DEVICE_RESET_DETECT_US) {
      this->drive_reset_(low, high, out);
      return;
    }
    bool pulled = false;
    uint32_t line_low = low + this->rise_us;
    for (auto &device : this->devices) {
      if (device.drives_low()) {
        pulled = true;
        line_low = std::max(line_low, device.zero_low_us);
      }
    }
    const uint32_t total = low + high;
    line_low = std::min(line_low, total);
    out->push_back({true, line_low});
    out->push_back({false, total - line_low});
    const bool master_bit = low < DEVICE_SAMPLE_US;
    for (auto &device : this->devices)
      device.on_slot(master_bit);
    this->last_slot_read_ = !pulled;
  }

  /// Level of the last slot as a sampling master would read it.
  bool last_slot_read() const { return this->last_slot_read_; }

 protected:
  void drive_reset_(uint32_t low, uint32_t high, std::vector<Phase> *out) {
    for (auto &device : this->devices)
      device.on_reset();
    out->push_back({true, low + this->rise_us});
    // Presence pulses of all devices, wired-AND, relative to the release of the line
    std::vector<std::pair<uint32_t, uint32_t>> pulses;
    for (const auto &device : this->devices)
      pulses.emplace_back(device.presence_wait_us, device.presence_wait_us + device.presence_low_us);
    std::sort(pulses.begin(), pulses.end());
    uint32_t now = this->rise_us;
    for (size_t i = 0; i < pulses.size();) {
      uint32_t begin = pulses[i].first;
      uint32_t end = pulses[i].second;
      for (i++; i < pulses.size() && pulses[i].first <= end; i++)
        end = std::max(end, pulses[i].second);
      if (begin > now)
        out->push_back({false, begin - now});
      out->push_back({true, end - std::max(begin, now)});
      now = end;
    }
    if (high > now)
      out->push_back({false, high - now});
  }

  bool last_slot_read_{true};
};

/// A receiver that stops on Low leaves duration1 of its last symbol unwritten. Whatever is in the
/// memory stays there: 0 and 75 on the S3 at GPIO36, 74 and 91 in the earlier probe. Nothing may
/// depend on this value.
constexpr uint16_t STALE_DURATION1 = 74;

/// What an RMT receiver makes of a waveform: starts at the first falling edge, stops when the
/// line has not changed for idle_us. A stop on High stores a zero High duration with level 1, a
/// stop on Low stores {0, 0, stale, 1} (section 7), the length of that Low is not readable.
/// end_us: time of the stop.
inline std::vector<Symbol> build_capture(const std::vector<Phase> &phases, uint32_t idle_us, uint64_t *end_us) {
  std::vector<Phase> merged;
  for (const auto &phase : phases) {
    if (phase.us == 0)
      continue;
    if (!merged.empty() && merged.back().low == phase.low) {
      merged.back().us += phase.us;
    } else {
      merged.push_back(phase);
    }
  }
  std::vector<Symbol> capture;
  uint64_t now = 0;
  size_t i = 0;
  if (!merged.empty() && !merged[0].low) {
    now += merged[0].us;  // RX waits for the first edge
    i++;
  }
  for (; i < merged.size(); i += 2) {
    const Phase &low = merged[i];
    if (low.us >= idle_us) {
      capture.push_back(Symbol{0, 0, STALE_DURATION1, 1});
      *end_us = now + idle_us;
      return capture;
    }
    now += low.us;
    // The line stays High after the last phase of the section, so the last High always idles out
    const bool last = i + 2 >= merged.size();
    if (last || merged[i + 1].us >= idle_us) {
      capture.push_back(Symbol{static_cast<uint16_t>(low.us), 0, 0, 1});
      *end_us = now + idle_us;
      return capture;
    }
    capture.push_back(Symbol{static_cast<uint16_t>(low.us), 0, static_cast<uint16_t>(merged[i + 1].us), 1});
    now += merged[i + 1].us;
  }
  *end_us = now + idle_us;
  return capture;
}

// ---------------------------------------------------------------------------------------------
// Mock Port
// ---------------------------------------------------------------------------------------------

/// An implementation that never returns from a blocking method or spins on the clock ends here.
/// Test builds have no exceptions, so this ends the run, naming the test.
[[noreturn]] inline void sim_hang(const char *reason) {
  const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
  fprintf(stderr, "\nFAILED %s.%s: %s\n", info != nullptr ? info->test_suite_name() : "?",
          info != nullptr ? info->name() : "?", reason);
  abort();
}

struct Section {
  bool armed{false};
  uint32_t idle_us{0};
  size_t capacity{0};
  std::vector<Symbol> tx;
  std::vector<Symbol> capture;  ///< What the receiver delivers, after capture_mutator and capacity
  uint64_t start_us{0};

  bool is_reset() const { return this->tx.size() == 1 && is_reset_symbol(this->tx[0]); }
};

enum class PortCall : uint8_t { ARM, TX, ABORT };

class SimPort : public Port {
 public:
  static constexpr int NEVER = -1;

  explicit SimPort(size_t mem_words) : mem_words_(mem_words) {}
  void attach(Engine *engine) { this->engine_ = engine; }

  // ---- Wire and scenario ----
  SimBus bus;
  bool stuck_low{false};           ///< Line held at GND, the receiver idles out on Low
  uint64_t line_low_until_us{0};   ///< line_is_high() is false until then
  bool manual{false};              ///< Completions are only delivered through fire_tx() and fire_rx()
  bool rx_first{false};            ///< The RX completion arrives before the TX completion
  bool spurious_wakeups{false};    ///< wait_for_event() returns early every other call
  int fail_arm_at{NEVER};          ///< arm_rx() number n returns false
  int fail_tx_at{NEVER};           ///< start_tx() number n returns false
  bool fail_abort{false};          ///< abort() returns false
  int drop_tx_at{NEVER};           ///< Section n never reports TX done
  int drop_rx_at{NEVER};           ///< Section n never reports RX done
  uint32_t section_delay_us{0};    ///< Extra time before a section reports completion
  std::function<void()> on_abort;  ///< Runs inside abort(), a completion racing the abort
  std::function<void(size_t section, std::vector<Symbol> &capture)> capture_mutator;

  // ---- Recorded ----
  std::vector<Section> sections;
  std::vector<PortCall> calls;
  int arm_count{0};
  int tx_count{0};
  int abort_count{0};
  int wait_calls{0};
  int isr_violations{0};  ///< Port methods called from inside on_tx_done() or on_rx_done()
  int double_arms{0};     ///< arm_rx() while RX was still armed, it fails like the real driver
  bool loop_active() const { return !this->engine_->is_idle(); }

  // ---- Port ----
  bool line_is_high() override {
    HeapPause pause;
    this->note_main_call_();
    this->note_spin_();
    return !this->stuck_low && this->now_us_ >= this->line_low_until_us;
  }

  bool arm_rx(Symbol *buffer, size_t capacity, uint32_t idle_us) override {
    HeapPause pause;
    this->note_main_call_();
    this->calls.push_back(PortCall::ARM);
    if (this->arm_count++ == this->fail_arm_at)
      return false;
    if (this->rx_armed_ || this->rx_outstanding_) {
      this->double_arms++;
      return false;
    }
    this->rx_buffer_ = buffer;
    this->rx_capacity_ = capacity;
    this->rx_idle_us_ = idle_us;
    this->rx_armed_ = true;
    return true;
  }

  bool start_tx(const Symbol *symbols, size_t count) override {
    HeapPause pause;
    this->note_main_call_();
    this->calls.push_back(PortCall::TX);
    if (this->tx_count++ == this->fail_tx_at)
      return false;
    this->waits_since_start_ = 0;
    const size_t index = this->sections.size();
    this->sections.emplace_back();
    Section &section = this->sections.back();
    section.armed = this->rx_armed_;
    section.idle_us = this->rx_idle_us_;
    section.capacity = this->rx_capacity_;
    section.tx.assign(symbols, symbols + count);
    section.start_us = this->now_us_;

    std::vector<Phase> phases;
    uint64_t tx_end = 0;
    for (size_t i = 0; i < count; i++) {
      tx_end += symbols[i].duration0 + symbols[i].duration1;
      if (!this->stuck_low)
        this->bus.drive(symbols[i], &phases);
    }

    uint64_t rx_end = 0;
    if (this->rx_armed_) {
      if (this->stuck_low) {
        section.capture = {Symbol{0, 0, STALE_DURATION1, 1}};
        rx_end = this->rx_idle_us_;
      } else {
        section.capture = build_capture(phases, this->rx_idle_us_, &rx_end);
      }
      if (this->capture_mutator)
        this->capture_mutator(index, section.capture);
      if (section.capture.size() > this->rx_capacity_)
        section.capture.resize(this->rx_capacity_);
    }

    this->tx_outstanding_ = true;
    this->rx_outstanding_ = this->rx_armed_;
    this->rx_armed_ = false;
    this->current_section_ = index;
    if (!this->manual) {
      const uint64_t base = this->now_us_ + this->section_delay_us;
      uint64_t tx_at = base + tx_end;
      uint64_t rx_at = base + rx_end;
      if (this->rx_first) {
        rx_at = base;
        tx_at = base + 1;
      }
      if (static_cast<int>(index) != this->drop_tx_at)
        this->events_.push_back({tx_at, true});
      if (this->rx_outstanding_ && static_cast<int>(index) != this->drop_rx_at)
        this->events_.push_back({rx_at, false});
      std::stable_sort(this->events_.begin(), this->events_.end(),
                       [](const Event &a, const Event &b) { return a.at_us < b.at_us; });
    }
    return true;
  }

  bool abort() override {
    HeapPause pause;
    this->note_main_call_();
    this->calls.push_back(PortCall::ABORT);
    this->abort_count++;
    if (this->on_abort)
      this->on_abort();
    this->events_.clear();
    this->tx_outstanding_ = false;
    this->rx_outstanding_ = false;
    this->rx_armed_ = false;
    return !this->fail_abort;
  }

  uint32_t now_ms() override {
    HeapPause pause;
    this->note_main_call_();
    this->note_spin_();
    return static_cast<uint32_t>(this->now_us_ / 1000) + this->time_base_ms_;
  }

  void wait_for_event(uint32_t max_ms) override {
    HeapPause pause;
    this->note_main_call_();
    this->wait_calls++;
    this->spin_ = 0;
    if (++this->waits_since_start_ > 100000)
      sim_hang("wait_for_event() called 100000 times without starting a section");
    if (this->spurious_wakeups) {
      this->spurious_toggle_ = !this->spurious_toggle_;
      if (this->spurious_toggle_) {
        this->advance_us(10);
        return;
      }
    }
    const uint64_t limit = this->now_us_ + std::max<uint64_t>(max_ms, 1) * 1000;
    if (!this->events_.empty() && this->events_.front().at_us <= limit) {
      this->fire_next_event();
    } else {
      this->advance_us(limit - this->now_us_);
    }
  }

  // ---- Driving the simulation, called from test code ----
  uint64_t now_us() const { return this->now_us_; }

  /// Starts now_ms() at this value, for clock wrap tests.
  void set_time_base_ms(uint32_t base) { this->time_base_ms_ = base; }

  /// Time passes, completions that are due arrive like an ISR would deliver them.
  void advance_us(uint64_t us) {
    const uint64_t target = this->now_us_ + us;
    while (!this->events_.empty() && this->events_.front().at_us <= target)
      this->fire_next_event();
    this->now_us_ = target;
    this->spin_ = 0;
  }

  /// Advances to the next completion and delivers it. False if none is scheduled.
  bool fire_next_event() {
    if (this->events_.empty())
      return false;
    Event event{};
    {
      HeapPause pause;
      event = this->events_.front();
      this->events_.pop_front();
    }
    this->now_us_ = std::max(this->now_us_, event.at_us);
    this->spin_ = 0;
    if (event.is_tx) {
      this->fire_tx();
    } else {
      this->fire_rx();
    }
    return true;
  }

  /// Delivers the TX completion of the running section.
  void fire_tx() {
    this->tx_outstanding_ = false;
    this->in_isr_ = true;
    this->engine_->on_tx_done();
    this->in_isr_ = false;
  }

  /// The receiver stores the capture of the running section and reports it.
  void fire_rx() {
    if (this->sections.empty() || !this->sections[this->current_section_].armed || this->rx_buffer_ == nullptr)
      return;
    const auto &capture = this->sections[this->current_section_].capture;
    if (!capture.empty())
      std::memcpy(this->rx_buffer_, capture.data(), capture.size() * sizeof(Symbol));
    this->rx_outstanding_ = false;
    this->in_isr_ = true;
    this->engine_->on_rx_done(capture.size());
    this->in_isr_ = false;
  }

  /// "RESET WRITE:63 WRITE:17 READ:62 READ:10", the notation of the table in section 4.2.
  std::string summary(size_t from = 0) const {
    std::string out;
    for (size_t i = from; i < this->sections.size(); i++) {
      const auto &section = this->sections[i];
      if (!out.empty())
        out += ' ';
      if (section.is_reset()) {
        out += "RESET";
      } else {
        out += section.armed ? "READ:" : "WRITE:";
        out += std::to_string(section.tx.size());  // NOLINT
      }
    }
    return out;
  }

  /// The slots of every reset-delimited frame on the wire, in order.
  std::vector<std::vector<Slot>> frames(size_t from = 0) const {
    std::vector<std::vector<Slot>> frames;
    for (size_t i = from; i < this->sections.size(); i++) {
      const auto &section = this->sections[i];
      if (section.is_reset()) {
        frames.emplace_back();
        continue;
      }
      if (frames.empty())
        frames.emplace_back();
      for (const auto &symbol : section.tx)
        frames.back().push_back(classify_slot(symbol));
    }
    return frames;
  }

 protected:
  struct Event {
    uint64_t at_us;
    bool is_tx;
  };

  void note_main_call_() {
    if (this->in_isr_)
      this->isr_violations++;
  }
  void note_spin_() {
    if (++this->spin_ > 200000)
      sim_hang("busy wait: clock or line polled 200000 times while no time passed");
  }

  size_t mem_words_;
  Engine *engine_{nullptr};
  uint64_t now_us_{0};
  uint32_t time_base_ms_{0};
  std::deque<Event> events_;
  Symbol *rx_buffer_{nullptr};
  size_t rx_capacity_{0};
  uint32_t rx_idle_us_{0};
  bool rx_armed_{false};
  bool tx_outstanding_{false};
  bool rx_outstanding_{false};
  size_t current_section_{0};
  bool in_isr_{false};
  bool spurious_toggle_{false};
  uint32_t spin_{0};
  uint32_t waits_since_start_{0};
};

// ---------------------------------------------------------------------------------------------
// Transactions and the simulated main loop
// ---------------------------------------------------------------------------------------------
struct DoneEntry {
  int id;
  TransactionStatus status;
  bool operator==(const DoneEntry &other) const { return this->id == other.id && this->status == other.status; }
};
using DoneLog = std::vector<DoneEntry>;

inline void PrintTo(const DoneEntry &entry, std::ostream *os) {
  *os << "{job " << entry.id << ", status " << static_cast<int>(entry.status) << "}";
}

class Job {
 public:
  Job(DoneLog *log, int id, uint64_t address, std::vector<uint8_t> write, size_t read_len)
      : log_(log), id_(id), write_bytes(std::move(write)), read_bytes(read_len, 0xA5) {
    this->t.address = address;
    this->t.write_data = this->write_bytes.empty() ? nullptr : this->write_bytes.data();
    this->t.write_len = static_cast<uint8_t>(this->write_bytes.size());
    this->t.read_data = this->read_bytes.empty() ? nullptr : this->read_bytes.data();
    this->t.read_len = static_cast<uint8_t>(read_len);
    this->t.on_done = &Job::done_;
    this->t.context = this;
  }
  Job(const Job &) = delete;
  Job &operator=(const Job &) = delete;

  Transaction t;
  std::vector<uint8_t> write_bytes;
  std::vector<uint8_t> read_bytes;
  std::function<void(TransactionStatus)> then;  ///< Runs inside on_done

  std::vector<Slot> slots() const { return expected_slots(this->t.address, this->write_bytes, this->t.read_len); }

 protected:
  static void done_(void *context, TransactionStatus status) {
    HeapPause pause;
    auto *job = static_cast<Job *>(context);
    job->log_->push_back({job->id_, status});
    if (job->then) {
      auto then = job->then;
      then(status);
    }
  }

  DoneLog *log_;
  int id_;
};

constexpr uint64_t ROM_A = 0x1100000000ABCD28ULL;
constexpr uint64_t ROM_B = 0x2200000000123428ULL;

struct Rig {
  explicit Rig(size_t mem_words = 64) : port(mem_words), engine(&port, mem_words), mem_words_(mem_words) {
    this->port.attach(&this->engine);
  }
  Rig(size_t mem_words, const Timing &timing)
      : port(mem_words), engine(&port, mem_words, timing), mem_words_(mem_words) {
    this->port.attach(&this->engine);
  }
  ~Rig() {
    EXPECT_EQ(this->port.isr_violations, 0) << "Port called from inside on_tx_done() or on_rx_done()";
    EXPECT_EQ(this->port.double_arms, 0) << "arm_rx() while RX was still armed";
  }

  SimPort port;
  Engine engine;
  size_t mem_words() const { return this->mem_words_; }

  /// The main loop: poll() as long as the component is asked to, completions arrive in between.
  void pump(uint32_t step_limit = 200000) {
    for (uint32_t step = 0; step < step_limit; step++) {
      if (this->port.loop_active())
        this->engine.poll();
      if (this->port.fire_next_event())
        continue;
      if (!this->port.loop_active())
        return;
      this->port.advance_us(500);
    }
    ADD_FAILURE() << "pump(): engine still wants the loop after " << step_limit << " steps";
  }

  void poll_times(int count) {
    for (int i = 0; i < count; i++)
      this->engine.poll();
  }

 protected:
  size_t mem_words_;
};

}  // namespace esphome::gpio::rmt_one_wire::testing
