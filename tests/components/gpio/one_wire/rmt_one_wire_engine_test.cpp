#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <new>
#include <random>
#include <vector>

#include "../common.h"

// Counts allocations for the no-heap tests (after setup the bus must not allocate). Forwards to malloc, so
// sanitizers keep seeing every block; only while a HeapWatch is active anything is counted. Test
// builds have no exceptions: the plain form aborts when out of memory, the nothrow forms return null.
namespace {
void *counted_alloc(std::size_t size) noexcept {
  if (esphome::gpio::rmt_one_wire::testing::g_heap_watch)
    esphome::gpio::rmt_one_wire::testing::g_heap_allocs++;
  return std::malloc(size == 0 ? 1 : size);  // NOLINT(cppcoreguidelines-no-malloc): replaces operator new
}
}  // namespace
void *operator new(std::size_t size) {
  void *block = counted_alloc(size);
  if (block == nullptr)
    std::abort();
  return block;
}
void *operator new(std::size_t size, const std::nothrow_t &) noexcept { return counted_alloc(size); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept { return counted_alloc(size); }
void operator delete(void *block) noexcept { std::free(block); }               // NOLINT(cppcoreguidelines-no-malloc)
void operator delete(void *block, std::size_t) noexcept { std::free(block); }  // NOLINT(cppcoreguidelines-no-malloc)

namespace esphome::gpio::rmt_one_wire::testing {

namespace {

constexpr uint8_t READ_SCRATCHPAD = 0xBE;
const std::vector<uint8_t> PAYLOAD{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

/// A rig with one device answering READ_SCRATCHPAD with PAYLOAD.
void add_scratchpad_device(Rig &rig, uint64_t rom = ROM_A) { rig.port.bus.devices.emplace_back(rom, PAYLOAD, 1); }

constexpr uint64_t MS = 1000;

void truncate_capture(std::vector<Symbol> &capture) {
  if (!capture.empty())
    capture.pop_back();
}

}  // namespace

// ---- Happy path ------------------------------------------------------------------------------

TEST(RmtOneWireEngine, ReadsScratchpad) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  EXPECT_TRUE(rig.engine.is_idle());
  ASSERT_TRUE(rig.engine.submit(&job.t));
  EXPECT_FALSE(rig.engine.is_idle());
  rig.pump();
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(job.read_bytes, PAYLOAD);
  EXPECT_EQ(rig.port.wait_calls, 0) << "wait_for_event is for the blocking methods only";
}

TEST(RmtOneWireEngine, SkipRomWithNoPayloadOnlySendsTheCommand) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  DoneLog log;
  Job job(&log, 1, 0, {}, 0);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:8");
}

TEST(RmtOneWireEngine, DeviceReceivesTheWrittenBytes) {
  Rig rig;
  auto &device = rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{}, 100);
  DoneLog log;
  Job job(&log, 1, ROM_A, {0x4E, 0x00, 0xFF, 0xA5}, 0);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(device.received(), job.write_bytes);
}

TEST(RmtOneWireEngine, MatchRomOnlyAddressesThatDevice) {
  Rig rig;
  add_scratchpad_device(rig, ROM_A);
  rig.port.bus.devices.emplace_back(ROM_B, std::vector<uint8_t>(9, 0x00), 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

// ---- Rule 1: the completion beats the timeout ------------------------------------------------

TEST(RmtOneWireEngine, CompletionWinsAfterLoopWasBlocked200ms) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.poll_times(1);
  for (int guard = 0; guard < 20 && log.empty(); guard++) {
    ASSERT_FALSE(rig.port.sections.empty());
    const bool armed = rig.port.sections.back().armed;
    rig.port.fire_tx();
    if (armed)
      rig.port.fire_rx();
    // Another component holds the main loop: the section ended long ago when the loop comes back
    rig.port.advance_us(200 * MS);
    rig.engine.poll();
  }
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, EachSectionHasItsOwnTimeout) {
  // Each section needs 40 ms, the whole transaction far more than 50 ms
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.section_delay_us = 40 * MS;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_GT(rig.port.now_us(), 150 * MS);
}

TEST(RmtOneWireEngine, NextSectionTimesFromItsOwnStartAfterBlockedLoop) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.poll_times(1);
  ASSERT_EQ(rig.port.sections.size(), 1u);
  rig.port.fire_tx();
  rig.port.fire_rx();
  rig.port.advance_us(200 * MS);
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 2u);
  // 45 ms into the second section nothing has run out
  rig.port.advance_us(45 * MS);
  rig.engine.poll();
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_TRUE(log.empty());
  // and past its 50 ms it does
  rig.port.advance_us(10 * MS);
  rig.engine.poll();
  EXPECT_EQ(rig.port.abort_count, 1);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}}));
}

// ---- Rule 4: on_done runs in the main loop and may submit ------------------------------------

TEST(RmtOneWireEngine, OnDoneMaySubmitTheSameTransactionAgain) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  int resubmits = 0;
  bool accepted = true;
  job.then = [&](TransactionStatus) {
    if (resubmits++ < 3)
      accepted = accepted && rig.engine.submit(&job.t);
  };
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_TRUE(accepted) << "inside on_done the transaction is no longer pending";
  EXPECT_EQ(log.size(), 4u);
  for (const auto &entry : log)
    EXPECT_EQ(entry.status, TransactionStatus::TRANSACTION_STATUS_OK);
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
  EXPECT_EQ(rig.port.frames().size(), 4u);
}

TEST(RmtOneWireEngine, OnDoneMaySubmitAnotherTransactionAndKeepsTheLoop) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job first(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job second(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  first.then = [&](TransactionStatus) { EXPECT_TRUE(rig.engine.submit(&second.t)); };
  ASSERT_TRUE(rig.engine.submit(&first.t));
  for (int guard = 0; guard < 2000 && log.empty(); guard++) {
    rig.engine.poll();
    rig.port.fire_next_event();
  }
  ASSERT_EQ(log.size(), 1u);
  // The classic bug: finish disables the loop after on_done enabled it again
  EXPECT_FALSE(rig.engine.is_idle());
  EXPECT_TRUE(rig.port.loop_active());
  rig.pump();
  EXPECT_EQ(log,
            (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}, {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(second.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, NullOnDoneIsAllowed) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job silent(&log, 1, 0, {0x44}, 0);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  silent.t.on_done = nullptr;
  ASSERT_TRUE(rig.engine.submit(&silent.t));
  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(next.read_bytes, PAYLOAD);
  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(slots_to_string(frames[0]), slots_to_string(silent.slots()));
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
  EXPECT_TRUE(rig.engine.submit(&silent.t)) << "no longer pending";
  rig.pump();
  EXPECT_EQ(rig.port.frames().size(), 3u);
}

TEST(RmtOneWireEngine, NullOnDoneOnEveryErrorEnd) {
  Rig rig;  // no device: NO_PRESENCE
  DoneLog log;
  Job silent(&log, 1, 0, {0x44}, 0);
  silent.t.on_done = nullptr;
  ASSERT_TRUE(rig.engine.submit(&silent.t));
  rig.pump();
  EXPECT_EQ(rig.port.summary(), "RESET");
  EXPECT_TRUE(rig.engine.is_idle());

  rig.port.drop_rx_at = 1;  // TIMEOUT
  ASSERT_TRUE(rig.engine.submit(&silent.t));
  rig.pump();
  EXPECT_EQ(rig.port.abort_count, 1);
  EXPECT_TRUE(rig.engine.is_idle());

  rig.port.line_low_until_us = rig.port.now_us() + 100 * MS;  // BUS_LOW
  ASSERT_TRUE(rig.engine.submit(&silent.t));
  rig.pump();
  EXPECT_EQ(rig.port.sections.size(), 2u);
  EXPECT_TRUE(rig.engine.is_idle());

  rig.engine.set_failed();  // HARDWARE_ERROR
  ASSERT_TRUE(rig.engine.submit(&silent.t));
  rig.pump();
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
  EXPECT_TRUE(log.empty());
}

// ---- PRECHECK --------------------------------------------------------------------------------

TEST(RmtOneWireEngine, LineLowAtTheStartIsBusLowAtOnce) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.line_low_until_us = 1000000;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.engine.poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW}}));
  EXPECT_EQ(rig.port.tx_count, 0);
  EXPECT_EQ(rig.port.arm_count, 0);
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, NextTransactionRunsOnceTheLineIsBack) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.line_low_until_us = 1000;
  DoneLog log;
  Job first(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job second(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&first.t));
  ASSERT_TRUE(rig.engine.submit(&second.t));
  rig.engine.poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW}}));
  EXPECT_TRUE(rig.port.sections.empty());
  rig.port.advance_us(2000);
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW},
                          {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(second.read_bytes, PAYLOAD);
}

// ---- Heap ------------------------------------------------------------------------------------

TEST(RmtOneWireEngine, NoHeapInSubmitPollAndCompletions) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  log.reserve(16);
  Job first(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job second(&log, 2, 0, {READ_SCRATCHPAD}, 9);
  second.then = [&](TransactionStatus) {};
  {
    HeapWatch watch;
    EXPECT_TRUE(rig.engine.submit(&first.t));
    EXPECT_TRUE(rig.engine.submit(&second.t));
    rig.pump();
  }
  EXPECT_EQ(g_heap_allocs, 0u);
  EXPECT_EQ(log,
            (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}, {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(first.read_bytes, PAYLOAD);
  EXPECT_EQ(rig.port.frames().size(), 2u);
}

TEST(RmtOneWireEngine, NoHeapOnErrorPaths) {
  Rig rig;
  DoneLog log;
  log.reserve(16);
  Job no_device(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job timeout(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  rig.port.drop_tx_at = 1;
  {
    HeapWatch watch;
    EXPECT_TRUE(rig.engine.submit(&no_device.t));
    EXPECT_TRUE(rig.engine.submit(&timeout.t));
    rig.pump();
    rig.engine.set_failed();
    EXPECT_TRUE(rig.engine.submit(&no_device.t));
    rig.pump();
  }
  EXPECT_EQ(g_heap_allocs, 0u);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE},
                          {2, TransactionStatus::TRANSACTION_STATUS_TIMEOUT},
                          {1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
}

TEST(RmtOneWireEngine, NoHeapInBlockingMethods) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A, PAYLOAD);
  uint8_t data[9];
  const uint8_t skip = 0xCC;
  const uint8_t search = 0xF0;
  {
    HeapWatch watch;
    EXPECT_EQ(rig.engine.reset_blocking(), 1);
    EXPECT_TRUE(rig.engine.write_blocking(&skip, 8));
    EXPECT_TRUE(rig.engine.read_blocking(data, 72));
    EXPECT_EQ(rig.engine.reset_blocking(), 1);
    EXPECT_TRUE(rig.engine.write_blocking(&search, 8));
    rig.engine.reset_search();
    EXPECT_EQ(rig.engine.search_next(), ROM_A);
  }
  EXPECT_EQ(g_heap_allocs, 0u);
}

// ---- Rule 8: the loop runs until is_idle() ------------------------------------------------------

TEST(RmtOneWireEngine, LoopActiveFromSubmitUntilTheFifoIsEmpty) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job first(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job second(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  EXPECT_FALSE(rig.port.loop_active());
  ASSERT_TRUE(rig.engine.submit(&first.t));
  EXPECT_TRUE(rig.port.loop_active());
  ASSERT_TRUE(rig.engine.submit(&second.t));
  EXPECT_TRUE(rig.port.loop_active());
  rig.pump();
  EXPECT_EQ(log.size(), 2u);
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, LoopStopsAfterEveryErrorEnd) {
  Rig rig;  // no device
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE}}));
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, LoopOffAfterBlockingMethods) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  EXPECT_EQ(rig.port.summary(), "RESET");
  EXPECT_FALSE(rig.port.loop_active());
  EXPECT_TRUE(rig.engine.is_idle());
}

// ---- Rules 2 and 3: the ISR functions only store ---------------------------------------------

TEST(RmtOneWireEngine, CompletionsTouchNothingButFlags) {
  // SimPort counts every Port call from inside on_tx_done() and on_rx_done(), Rig checks the count
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.poll_times(1);
  ASSERT_EQ(rig.port.sections.size(), 1u);
  ASSERT_TRUE(log.empty());
  while (log.empty()) {
    ASSERT_LT(rig.port.sections.size(), 20u);
    const std::vector<uint8_t> untouched = job.read_bytes;
    const size_t started = rig.port.sections.size();
    const bool armed = rig.port.sections.back().armed;
    const std::vector<PortCall> calls_before = rig.port.calls;
    {
      HeapWatch watch;
      rig.port.fire_tx();
      if (armed)
        rig.port.fire_rx();
    }
    EXPECT_EQ(g_heap_allocs, 0u);
    // Nothing happens until the main loop runs: no start, no on_done, no decoding (rule 3)
    EXPECT_EQ(rig.port.sections.size(), started);
    EXPECT_TRUE(log.empty()) << "on_done ran inside a completion";
    EXPECT_EQ(rig.port.calls, calls_before);
    EXPECT_EQ(job.read_bytes, untouched) << "decoded inside a completion";
    rig.engine.poll();
  }
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
}

namespace {

/// Completes the sections before target through SimPort, target is running afterwards.
void run_to_section(Rig &rig, const DoneLog &log, Job &job, size_t target) {
  rig.port.manual = true;
  add_scratchpad_device(rig);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  for (int guard = 0; guard < 20 && rig.port.sections.size() <= target; guard++) {
    rig.port.fire_tx();
    if (rig.port.sections.back().armed)
      rig.port.fire_rx();
    rig.engine.poll();
  }
  ASSERT_EQ(rig.port.sections.size(), target + 1);
  ASSERT_TRUE(log.empty());
}

}  // namespace

TEST(RmtOneWireEngine, CompletionsReportTheLastExpectedEvent) {
  // The events under test come straight from here, SimPort ignores the result. The main loop does
  // not run after them, so the receive buffer does not matter.
  // RESET (section 0) and the first READ (section 3): both orders
  for (size_t target : {size_t{0}, size_t{3}}) {
    for (bool rx_first : {false, true}) {
      SCOPED_TRACE(target);
      Rig rig;
      DoneLog log;
      Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
      ASSERT_NO_FATAL_FAILURE(run_to_section(rig, log, job, target));
      ASSERT_TRUE(rig.port.sections.back().armed);
      if (rx_first) {
        EXPECT_FALSE(rig.engine.on_rx_done(1));
        EXPECT_TRUE(rig.engine.on_tx_done());
      } else {
        EXPECT_FALSE(rig.engine.on_tx_done());
        EXPECT_TRUE(rig.engine.on_rx_done(1));
      }
    }
  }

  // WRITE: TX is the only event
  {
    Rig rig;
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    ASSERT_NO_FATAL_FAILURE(run_to_section(rig, log, job, 1));
    ASSERT_FALSE(rig.port.sections.back().armed);
    EXPECT_TRUE(rig.engine.on_tx_done());
  }

  // IDLE
  {
    Rig rig;
    EXPECT_FALSE(rig.engine.on_tx_done());
    EXPECT_FALSE(rig.engine.on_rx_done(1));
  }

  // Inside abort(), in the on_done behind it, and idle after it
  for (bool queued : {false, true}) {
    SCOPED_TRACE(queued);
    Rig rig;
    add_scratchpad_device(rig);
    rig.port.manual = true;
    DoneLog log;
    Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
    const auto expect_no_wake = [&] {
      EXPECT_FALSE(rig.engine.on_tx_done());
      EXPECT_FALSE(rig.engine.on_rx_done(1));
    };
    rig.port.on_abort = expect_no_wake;
    lost.then = [&](TransactionStatus) { expect_no_wake(); };
    ASSERT_TRUE(rig.engine.submit(&lost.t));
    if (queued) {
      ASSERT_TRUE(rig.engine.submit(&next.t));
    }
    rig.engine.poll();
    ASSERT_EQ(rig.port.sections.size(), 1u);
    rig.port.advance_us(60 * MS);
    rig.engine.poll();
    ASSERT_EQ(rig.port.abort_count, 1);
    ASSERT_EQ(log.size(), 1u);
    if (!queued) {
      ASSERT_TRUE(rig.engine.is_idle());
      expect_no_wake();
    }
  }
}

// ---- FIFO, atomic on the bus -------------------------------------------------------------

TEST(RmtOneWireEngine, TransactionsRunInFifoOrderWithoutInterleaving) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job a(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job b(&log, 2, 0, {0x44}, 0);
  Job c(&log, 3, ROM_A, {READ_SCRATCHPAD, 0x00}, 0);
  Job d(&log, 4, 0, {READ_SCRATCHPAD}, 2);
  ASSERT_TRUE(rig.engine.submit(&a.t));
  ASSERT_TRUE(rig.engine.submit(&b.t));
  ASSERT_TRUE(rig.engine.submit(&c.t));
  ASSERT_TRUE(rig.engine.submit(&d.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK},
                          {2, TransactionStatus::TRANSACTION_STATUS_OK},
                          {3, TransactionStatus::TRANSACTION_STATUS_OK},
                          {4, TransactionStatus::TRANSACTION_STATUS_OK}}));
  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), 4u);
  EXPECT_EQ(slots_to_string(frames[0]), slots_to_string(a.slots()));
  EXPECT_EQ(slots_to_string(frames[1]), slots_to_string(b.slots()));
  EXPECT_EQ(slots_to_string(frames[2]), slots_to_string(c.slots()));
  EXPECT_EQ(slots_to_string(frames[3]), slots_to_string(d.slots()));
  EXPECT_EQ(a.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, ThePollThatEndsATransactionStartsTheNext) {
  // The completion of the last section wakes the loop only once. With a raised loop_interval, the next loop pass
  // can be seconds away, so the queued transaction must not wait for it.
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job first(&log, 1, 0, {0x44}, 0);
  Job second(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&first.t));
  ASSERT_TRUE(rig.engine.submit(&second.t));
  rig.engine.poll();
  // One poll() per completion, as the bus has no other loop pass
  const auto complete_section_and_poll = [&rig]() {
    ASSERT_FALSE(rig.port.sections.empty());
    const bool armed = rig.port.sections.back().armed;
    rig.port.fire_tx();
    if (armed)
      rig.port.fire_rx();
    rig.engine.poll();
  };
  for (int guard = 0; guard < 20 && log.empty(); guard++)
    complete_section_and_poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:16 RESET") << "the second one is on the bus without another poll()";
  for (int guard = 0; guard < 20 && log.size() < 2; guard++)
    complete_section_and_poll();
  EXPECT_EQ(log,
            (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}, {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(second.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, ATransactionThatEndsAtTheStartDoesNotStartTheNext) {
  // Only an end on the bus starts the next one at once, so a client that submits again from on_done while the
  // line is Low cannot keep poll() from returning
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.line_low_until_us = 1000;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  job.then = [&](TransactionStatus) { EXPECT_TRUE(rig.engine.submit(&job.t)); };
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.engine.poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW}}));
  EXPECT_FALSE(rig.engine.is_idle());
  EXPECT_TRUE(rig.port.sections.empty());
  job.then = nullptr;
  rig.port.advance_us(2000);
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW},
                          {1, TransactionStatus::TRANSACTION_STATUS_OK}}));
}

TEST(RmtOneWireEngine, SubmitDuringEverySectionDoesNotDisturbTheRunningOne) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job a(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job b(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  Job c(&log, 3, 0, {0x44}, 0);
  ASSERT_TRUE(rig.engine.submit(&a.t));
  int submitted = 0;
  for (int guard = 0; guard < 2000 && !rig.engine.is_idle(); guard++) {
    rig.engine.poll();
    if (rig.port.sections.size() == 2 && submitted == 0) {
      ASSERT_TRUE(rig.engine.submit(&b.t));
      submitted = 1;
    }
    if (rig.port.sections.size() == 4 && submitted == 1) {
      ASSERT_TRUE(rig.engine.submit(&c.t));
      submitted = 2;
    }
    if (!rig.port.fire_next_event())
      rig.port.advance_us(100);
  }
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK},
                          {2, TransactionStatus::TRANSACTION_STATUS_OK},
                          {3, TransactionStatus::TRANSACTION_STATUS_OK}}));
  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), 3u);
  EXPECT_EQ(slots_to_string(frames[0]), slots_to_string(a.slots()));
  EXPECT_EQ(slots_to_string(frames[1]), slots_to_string(b.slots()));
  EXPECT_EQ(slots_to_string(frames[2]), slots_to_string(c.slots()));
  EXPECT_EQ(a.read_bytes, PAYLOAD);
  EXPECT_EQ(b.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, OnDoneSubmitQueuesBehindWhatIsAlreadyWaiting) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job a(&log, 1, 0, {0x44}, 0);
  Job b(&log, 2, 0, {0x44}, 0);
  Job c(&log, 3, 0, {0x44}, 0);
  Job d(&log, 4, 0, {0x44}, 0);
  a.then = [&](TransactionStatus) { EXPECT_TRUE(rig.engine.submit(&d.t)); };
  ASSERT_TRUE(rig.engine.submit(&a.t));
  ASSERT_TRUE(rig.engine.submit(&b.t));
  ASSERT_TRUE(rig.engine.submit(&c.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK},
                          {2, TransactionStatus::TRANSACTION_STATUS_OK},
                          {3, TransactionStatus::TRANSACTION_STATUS_OK},
                          {4, TransactionStatus::TRANSACTION_STATUS_OK}}));
}

TEST(RmtOneWireEngine, SubmitOfAPendingTransactionIsRefusedAndChangesNothing) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job a(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job b(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&a.t));
  ASSERT_TRUE(rig.engine.submit(&b.t));
  EXPECT_FALSE(rig.engine.submit(&a.t)) << "still queued";
  EXPECT_FALSE(rig.engine.submit(&b.t)) << "still queued";
  rig.poll_times(1);
  ASSERT_FALSE(rig.port.sections.empty());
  EXPECT_FALSE(rig.engine.submit(&a.t)) << "running";
  rig.pump();
  EXPECT_EQ(log,
            (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}, {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(rig.port.frames().size(), 2u);
  EXPECT_TRUE(rig.engine.submit(&a.t)) << "done, accepted again";
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
}

TEST(RmtOneWireEngine, ReusedNodesInAnotherOrderDoNotLoseOrRepeat) {
  // The FIFO is intrusive, a stale next_ from the previous round must not survive
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job a(&log, 1, 0, {0x44}, 0);
  Job b(&log, 2, 0, {0x44}, 0);
  Job c(&log, 3, 0, {0x44}, 0);
  ASSERT_TRUE(rig.engine.submit(&a.t));
  ASSERT_TRUE(rig.engine.submit(&b.t));
  ASSERT_TRUE(rig.engine.submit(&c.t));
  rig.pump();
  ASSERT_EQ(log.size(), 3u);
  log.clear();
  ASSERT_TRUE(rig.engine.submit(&c.t));
  ASSERT_TRUE(rig.engine.submit(&a.t));
  rig.pump();
  EXPECT_EQ(log,
            (DoneLog{{3, TransactionStatus::TRANSACTION_STATUS_OK}, {1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  log.clear();
  ASSERT_TRUE(rig.engine.submit(&a.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_EQ(rig.port.frames().size(), 6u);
}

TEST(RmtOneWireEngine, ManyTransactionsKeepTheOrder) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  DoneLog log;
  std::deque<Job> jobs;
  DoneLog expected;
  for (int i = 0; i < 40; i++) {
    jobs.emplace_back(&log, i, 0, std::vector<uint8_t>{static_cast<uint8_t>(i)}, 0);
    expected.push_back({i, TransactionStatus::TRANSACTION_STATUS_OK});
  }
  for (auto &job : jobs)
    ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, expected);
  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), jobs.size());
  for (size_t i = 0; i < jobs.size(); i++)
    EXPECT_EQ(slots_to_string(frames[i]), slots_to_string(jobs[i].slots())) << i;
}

// ---- Abort and recovery ------------------------------------------------------------------

struct LostCompletion {
  const char *name;
  bool lose_tx;
  bool lose_rx;
};

class RmtOneWireLostCompletion : public ::testing::TestWithParam<LostCompletion> {};

INSTANTIATE_TEST_SUITE_P(Abort, RmtOneWireLostCompletion,
                         ::testing::Values(LostCompletion{"Tx", true, false}, LostCompletion{"Rx", false, true},
                                           LostCompletion{"Both", true, true}),
                         [](const ::testing::TestParamInfo<LostCompletion> &info) { return info.param.name; });

TEST_P(RmtOneWireLostCompletion, TimeoutAt50msThenNextRunsNormally) {
  Rig rig;
  add_scratchpad_device(rig);
  if (GetParam().lose_tx)
    rig.port.drop_tx_at = 0;
  if (GetParam().lose_rx)
    rig.port.drop_rx_at = 0;
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  const uint64_t started = rig.port.sections[0].start_us;

  rig.port.advance_us(started + 49 * MS - rig.port.now_us());
  rig.engine.poll();
  EXPECT_EQ(rig.port.abort_count, 0) << "aborted before 50 ms";
  EXPECT_TRUE(log.empty());

  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.port.advance_us(2 * MS);
  rig.engine.poll();
  EXPECT_EQ(rig.port.abort_count, 1);
  ASSERT_FALSE(log.empty());
  EXPECT_EQ(log[0], (DoneEntry{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}));
  EXPECT_EQ(rig.port.sections.size(), 2u) << "the poll() of the timeout also starts the next one";

  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT},
                          {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(next.read_bytes, PAYLOAD);
  EXPECT_EQ(rig.port.abort_count, 1);
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST_P(RmtOneWireLostCompletion, TimeoutInALaterSection) {
  // Section 3 of RESET, WRITE, WRITE, READ, READ is the first READ
  Rig rig;
  add_scratchpad_device(rig);
  if (GetParam().lose_tx)
    rig.port.drop_tx_at = 3;
  if (GetParam().lose_rx)
    rig.port.drop_rx_at = 3;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}}));
  EXPECT_EQ(rig.port.sections.size(), 4u);
  EXPECT_EQ(rig.port.abort_count, 1);
}

TEST(RmtOneWireEngine, NextPollInMsIsTheTimeLeftOfTheRunningSection) {
  // The bus wakes the loop at this time, so a lost completion times out even with a raised loop_interval
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 0u) << "waits to start";
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 50u);
  rig.port.advance_us(20 * MS);
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 30u);

  // The next section has a deadline of its own
  rig.port.fire_tx();
  rig.port.fire_rx();
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 2u);
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 50u);
  rig.port.advance_us(49 * MS);
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 1u);
  rig.engine.poll();
  EXPECT_TRUE(log.empty());

  // At the deadline, the poll() at that time ends the section
  rig.port.advance_us(1 * MS);
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 0u);
  rig.port.advance_us(1000 * MS);
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 0u) << "stays 0 past the deadline";
  rig.engine.poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}}));
  EXPECT_TRUE(rig.engine.is_idle());
}

TEST(RmtOneWireEngine, NextPollInMsIsZeroWhileATransactionWaitsToStart) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.line_low_until_us = 1000;
  DoneLog log;
  Job first(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job second(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&first.t));
  ASSERT_TRUE(rig.engine.submit(&second.t));
  rig.engine.poll();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW}}));
  ASSERT_FALSE(rig.engine.is_idle());
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 0u);
}

TEST(RmtOneWireEngine, NextPollInMsWorksAcrossTheClockWrap) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  rig.port.set_time_base_ms(0xFFFFFFFFu - 10u);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.engine.poll();
  rig.port.advance_us(20 * MS);
  EXPECT_EQ(rig.engine.next_poll_in_ms(), 30u);
}

TEST(RmtOneWireEngine, TimeoutOfAWriteSectionOnlyNeedsTx) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.drop_tx_at = 1;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}}));
  EXPECT_EQ(rig.port.sections.size(), 2u);
}

TEST(RmtOneWireEngine, TimeoutWorksAcrossTheClockWrap) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.set_time_base_ms(0xFFFFFFFFu - 20u);
  rig.port.drop_rx_at = 0;
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  rig.port.advance_us(45 * MS);  // past the wrap, still inside the 50 ms
  rig.engine.poll();
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_TRUE(log.empty());
  rig.port.advance_us(10 * MS);
  rig.engine.poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}}));
}

namespace {

/// Drives the next transaction by hand and checks that late messages did not advance it.
void expect_next_is_not_advanced_and_completes(
    Rig &rig, DoneLog &log, Job &next, size_t sections_before,
    TransactionStatus first_status = TransactionStatus::TRANSACTION_STATUS_TIMEOUT) {
  rig.poll_times(5);
  ASSERT_EQ(rig.port.sections.size(), sections_before + 1) << "the next transaction starts its RESET, nothing more";
  EXPECT_TRUE(rig.port.sections.back().is_reset());
  EXPECT_EQ(log.size(), 1u) << "a late message completed the next transaction's RESET";

  // TX alone does not end the RESET: a stale RX flag from the aborted section would
  rig.port.fire_tx();
  rig.poll_times(3);
  EXPECT_EQ(rig.port.sections.size(), sections_before + 1) << "stale RX completion was taken for the new section";
  rig.port.fire_rx();
  for (int guard = 0; guard < 50 && log.size() == 1; guard++) {
    rig.poll_times(1);
    if (log.size() > 1)
      break;
    const bool armed = rig.port.sections.back().armed;
    rig.port.fire_tx();
    if (armed)
      rig.port.fire_rx();
  }
  EXPECT_EQ(log, (DoneLog{{1, first_status}, {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(next.read_bytes, PAYLOAD);
}

}  // namespace

TEST(RmtOneWireEngine, LateCompletionInsideAbortAfterFailedStartIsDiscarded) {
  // Rule 7 also holds for the abort() of rule 6
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  rig.port.fail_tx_at = 0;
  DoneLog log;
  Job failing(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  rig.port.on_abort = [&] {
    rig.engine.on_tx_done();
    rig.engine.on_rx_done(1);
  };
  ASSERT_TRUE(rig.engine.submit(&failing.t));
  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.abort_count, 1);
  expect_next_is_not_advanced_and_completes(rig, log, next, 0, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR);
}

TEST(RmtOneWireEngine, LateCompletionInsideAbortIsDiscarded) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  rig.port.on_abort = [&] {
    // An ISR that fires while the channels are being stopped
    rig.engine.on_tx_done();
    rig.engine.on_rx_done(1);
  };
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  rig.port.advance_us(60 * MS);
  rig.engine.poll();
  ASSERT_EQ(rig.port.abort_count, 1);
  expect_next_is_not_advanced_and_completes(rig, log, next, 1);
}

TEST(RmtOneWireEngine, LateCompletionBetweenAbortAndNextStartIsDiscarded) {
  // on_done runs after abort() and before the next start
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  lost.then = [&](TransactionStatus) {
    rig.engine.on_tx_done();
    rig.engine.on_rx_done(1);
  };
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  rig.port.advance_us(60 * MS);
  rig.engine.poll();
  ASSERT_EQ(rig.port.abort_count, 1);
  expect_next_is_not_advanced_and_completes(rig, log, next, 1);
}

TEST(RmtOneWireEngine, LateCompletionWhileIdleIsDiscarded) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  rig.port.advance_us(60 * MS);
  rig.engine.poll();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}}));
  ASSERT_TRUE(rig.engine.is_idle());

  rig.engine.on_tx_done();
  rig.engine.on_rx_done(1);
  EXPECT_TRUE(rig.engine.is_idle());

  ASSERT_TRUE(rig.engine.submit(&next.t));
  expect_next_is_not_advanced_and_completes(rig, log, next, 1);
}

TEST(RmtOneWireEngine, PartialCompletionOfTheAbortedSectionDoesNotLeak) {
  // TX was reported before the timeout, RX never: the flag must not count for the next section
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  rig.port.fire_tx();
  rig.engine.poll();
  rig.port.advance_us(60 * MS);
  rig.engine.poll();
  ASSERT_EQ(rig.port.abort_count, 1);
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT}}));
  rig.poll_times(3);
  ASSERT_EQ(rig.port.sections.size(), 2u);

  rig.port.fire_rx();
  rig.poll_times(3);
  EXPECT_EQ(rig.port.sections.size(), 2u) << "stale TX completion was taken for the new section";
  rig.port.fire_tx();
  rig.poll_times(1);
  EXPECT_EQ(rig.port.sections.size(), 3u);
}

TEST(RmtOneWireEngine, AbortThatFailsEndsAllWorkInHardwareError) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.drop_tx_at = 0;
  rig.port.fail_abort = true;
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job queued(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  Job later(&log, 3, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  ASSERT_TRUE(rig.engine.submit(&queued.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR},
                          {2, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  EXPECT_EQ(rig.port.tx_count, 1);

  ASSERT_TRUE(rig.engine.submit(&later.t));
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
  EXPECT_EQ(log.back(), (DoneEntry{3, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}));
  EXPECT_EQ(rig.port.tx_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.arm_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.abort_count, 1) << "no RMT call on a failed bus";
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

// ---- Section 7: error table ------------------------------------------------------------------

TEST(RmtOneWireEngine, NoDeviceIsNoPresenceAndStopsAfterTheReset) {
  Rig rig;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE}}));
  EXPECT_EQ(rig.port.summary(), "RESET");
  EXPECT_EQ(rig.port.abort_count, 0);
}

TEST(RmtOneWireEngine, WireToGndAfterPrecheckIsBusLow) {
  // The precheck saw High, the wire goes to GND during the reset: the capture ends on Low
  for (const Symbol &held : {Symbol{600, 0, 0, 0}, Symbol{0, 0, STALE_DURATION1, 1}, Symbol{0, 0, 0, 1}}) {
    Rig rig;
    add_scratchpad_device(rig);
    rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
      if (section == 0)
        capture = {held};
    };
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    ASSERT_TRUE(rig.engine.submit(&job.t));
    rig.pump();
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW}}))
        << held.duration0 << "/" << held.duration1;
    EXPECT_EQ(rig.port.summary(), "RESET");
  }
}

TEST(RmtOneWireEngine, LineStuckBeforeTheStartIsBusLow) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.stuck_low = true;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW}}));
  EXPECT_EQ(rig.port.tx_count, 0);
}

TEST(RmtOneWireEngine, WrongResetCaptureIsCaptureError) {
  for (const auto &bad : {std::vector<Symbol>{}, std::vector<Symbol>{{503, 1, 27, 0}, {120, 1, 0, 0}}}) {
    Rig rig;
    add_scratchpad_device(rig);
    rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
      if (section == 0)
        capture = bad;
    };
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    ASSERT_TRUE(rig.engine.submit(&job.t));
    rig.pump();
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR}}));
    EXPECT_EQ(rig.port.summary(), "RESET");
  }
}

TEST(RmtOneWireEngine, BadReadCaptureStopsTheTransaction) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
    if (section == 3)
      truncate_capture(capture);
  };
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR}}));
  EXPECT_EQ(rig.port.sections.size(), 4u) << "the second READ must not start";
  EXPECT_EQ(rig.port.abort_count, 0);
}

TEST(RmtOneWireEngine, LineToGndDuringReadIsCaptureError) {
  // {100, 0, 0, 0} does not occur on the S3. The measured form is S2 of the probe, n = 3: two valid
  // slots, then the line stays Low.
  for (bool measured : {false, true}) {
    SCOPED_TRACE(measured);
    Rig rig;
    add_scratchpad_device(rig);
    rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
      if (section != 3)
        return;
      if (measured) {
        capture.resize(2);
        capture.push_back(Symbol{0, 0, STALE_DURATION1, 1});
      } else {
        capture = {Symbol{100, 0, 0, 0}};
      }
    };
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    ASSERT_TRUE(rig.engine.submit(&job.t));
    rig.pump();
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR}}));
    EXPECT_EQ(rig.port.sections.size(), 4u);
  }
}

TEST(RmtOneWireEngine, LineToGndInTheLastSlotIsCaptureError) {
  // Section 7: the symbol count is right and the first bits are fine, only the end is on Low. The
  // form behind a complete capture is the one that no slot rule can catch.
  for (size_t failing_section : {size_t{3}, size_t{4}}) {
    for (uint16_t stale : {STALE_DURATION1, uint16_t{0}}) {
      for (bool behind : {false, true}) {
        const Symbol ends_on_low{0, 0, stale, 1};
        Rig rig;
        add_scratchpad_device(rig);
        rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
          if (section != failing_section)
            return;
          if (behind) {
            capture.push_back(ends_on_low);
          } else {
            capture.back() = ends_on_low;
          }
        };
        DoneLog log;
        Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
        ASSERT_TRUE(rig.engine.submit(&job.t));
        rig.pump();
        EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR}}))
            << "section " << failing_section << " stale " << stale << " behind " << behind;
        EXPECT_EQ(rig.port.sections.size(), failing_section + 1);
      }
    }
  }
}

TEST(RmtOneWireEngine, LowTimeLimitIn109Out110) {
  const std::vector<uint8_t> ones(9, 0xFF);
  for (size_t failing_section : {size_t{3}, size_t{4}}) {
    for (bool last_slot : {false, true}) {
      auto patch = [&](std::vector<Symbol> &capture, uint16_t low) {
        capture[last_slot ? capture.size() - 1 : 5].duration0 = low;
      };
      {
        Rig rig;
        rig.port.bus.devices.emplace_back(ROM_A, ones, 1);
        rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
          if (section == failing_section)
            patch(capture, 109);
        };
        DoneLog log;
        Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
        ASSERT_TRUE(rig.engine.submit(&job.t));
        rig.pump();
        EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}})) << "109 us is a slow 0";
        int zeros = 0;
        for (auto byte : job.read_bytes) {
          for (int bit = 0; bit < 8; bit++)
            zeros += ((byte >> bit) & 1) == 0;
        }
        EXPECT_EQ(zeros, 1);
      }
      {
        Rig rig;
        rig.port.bus.devices.emplace_back(ROM_A, ones, 1);
        rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
          if (section == failing_section)
            patch(capture, 110);
        };
        DoneLog log;
        Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
        ASSERT_TRUE(rig.engine.submit(&job.t));
        rig.pump();
        EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR}})) << "110 us is a held bus";
      }
    }
  }
}

TEST(RmtOneWireEngine, BadCrcIsStillOk) {
  // The engine does not know about CRCs, that is check_scratch_pad_() of the consumer
  Rig rig;
  std::vector<uint8_t> broken = PAYLOAD;
  broken[8] ^= 0xFF;
  rig.port.bus.devices.emplace_back(ROM_A, broken, 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(job.read_bytes, broken);
}

struct PortFailure {
  const char *name;
  int fail_arm_at;
  int fail_tx_at;
  size_t sections_started;  ///< Started before the failure
  int tx_calls;             ///< start_tx() calls of the failing transaction
  int aborts;               ///< Rule 6: only a failed start_tx() behind an armed RX needs abort()
};

class RmtOneWirePortFailure : public ::testing::TestWithParam<PortFailure> {};

// Sections of the transaction: RESET, WRITE, WRITE, READ, READ
INSTANTIATE_TEST_SUITE_P(Errors, RmtOneWirePortFailure,
                         ::testing::Values(PortFailure{"ArmReset", 0, SimPort::NEVER, 0, 0, 0},
                                           PortFailure{"ArmRead", 1, SimPort::NEVER, 3, 3, 0},
                                           PortFailure{"ArmSecondRead", 2, SimPort::NEVER, 4, 4, 0},
                                           PortFailure{"TxReset", SimPort::NEVER, 0, 0, 1, 1},
                                           PortFailure{"TxWrite", SimPort::NEVER, 1, 1, 2, 0},
                                           PortFailure{"TxRead", SimPort::NEVER, 3, 3, 4, 1}),
                         [](const ::testing::TestParamInfo<PortFailure> &info) { return info.param.name; });

TEST_P(RmtOneWirePortFailure, HardwareErrorThenNextRunsNormally) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.fail_arm_at = GetParam().fail_arm_at;
  rig.port.fail_tx_at = GetParam().fail_tx_at;
  DoneLog log;
  Job failing(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  int aborts_at_on_done = -1;
  failing.then = [&](TransactionStatus) { aborts_at_on_done = rig.port.abort_count; };
  ASSERT_TRUE(rig.engine.submit(&failing.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  EXPECT_EQ(rig.port.sections.size(), GetParam().sections_started);
  EXPECT_EQ(rig.port.tx_count, GetParam().tx_calls) << "TX must not start after arm_rx() failed";
  EXPECT_EQ(rig.port.abort_count, GetParam().aborts);
  EXPECT_EQ(aborts_at_on_done, GetParam().aborts) << "abort() comes before the transaction ends";

  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR},
                          {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(next.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, FailedStartWhoseAbortFailsIsPermanent) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.fail_tx_at = 0;
  rig.port.fail_abort = true;
  DoneLog log;
  Job failing(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job queued(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  Job later(&log, 3, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&failing.t));
  ASSERT_TRUE(rig.engine.submit(&queued.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR},
                          {2, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  ASSERT_TRUE(rig.engine.submit(&later.t));
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
  EXPECT_EQ(log.back(), (DoneEntry{3, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}));
  EXPECT_EQ(rig.port.tx_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.arm_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.abort_count, 1) << "no RMT call on a failed bus";
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, SetFailedEndsRunningQueuedAndFutureWork) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.manual = true;
  DoneLog log;
  Job running(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job queued(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  Job future(&log, 3, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&running.t));
  ASSERT_TRUE(rig.engine.submit(&queued.t));
  rig.engine.poll();
  ASSERT_EQ(rig.port.sections.size(), 1u);
  const int arms = rig.port.arm_count;
  const int transmits = rig.port.tx_count;

  rig.engine.set_failed();
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR},
                          {2, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());

  // Completions of the abandoned section change nothing
  rig.engine.on_tx_done();
  rig.engine.on_rx_done(1);

  ASSERT_TRUE(rig.engine.submit(&future.t));
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
  EXPECT_EQ(log.back(), (DoneEntry{3, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}));
  EXPECT_EQ(rig.port.arm_count, arms);
  EXPECT_EQ(rig.port.tx_count, transmits);
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, FailedBusStillRefusesAPendingTransactionOnly) {
  Rig rig;
  rig.engine.set_failed();
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  ASSERT_TRUE(rig.engine.submit(&job.t)) << "the transaction is no longer pending";
  rig.pump();
  EXPECT_EQ(log.size(), 2u);
}

TEST(RmtOneWireEngine, FailedBusEndsASubmitBeforeSubmitReturns) {
  // The bus component is out of the main loop after mark_failed(), so nothing may wait for poll()
  Rig rig;
  rig.engine.set_failed();
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  EXPECT_TRUE(rig.engine.is_idle());
}

TEST(RmtOneWireEngine, AbortThatFailsInABlockingMethodEndsTheQueuedWork) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.drop_tx_at = 0;
  rig.port.fail_abort = true;
  DoneLog log;
  Job first(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job second(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&first.t));
  ASSERT_TRUE(rig.engine.submit(&second.t));
  const uint8_t skip_rom = 0xCC;
  EXPECT_FALSE(rig.engine.write_blocking(&skip_rom, 8));
  // Without a single poll()
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR},
                          {2, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.engine.is_busy());
  EXPECT_EQ(rig.port.tx_count, 1) << "the queued transactions never reached the bus";
}

TEST(RmtOneWireEngine, SubmitInsideTheFailingCallEndsWhenTheCallReturns) {
  // The bus logs while it fails inside abort(), and a log handler may submit at that point
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.drop_tx_at = 0;
  rig.port.fail_abort = true;
  DoneLog log;
  Job late(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  bool submitted = false;
  size_t done_inside = 0;
  rig.port.on_abort = [&]() {
    rig.engine.set_failed();
    submitted = rig.engine.submit(&late.t);
    done_inside = log.size();
  };
  std::vector<uint8_t> data(9);
  EXPECT_FALSE(rig.engine.read_blocking(data.data(), 72));
  EXPECT_TRUE(submitted);
  EXPECT_EQ(done_inside, 0u) << "on_done must not run inside the engine call";
  EXPECT_EQ(log, (DoneLog{{2, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  EXPECT_TRUE(rig.engine.is_idle());
}

TEST(RmtOneWireEngine, SubmitInsideTheFailingPollEndsWhenPollReturns) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.drop_tx_at = 0;
  rig.port.fail_abort = true;
  DoneLog log;
  Job running(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job late(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  rig.port.on_abort = [&]() {
    rig.engine.set_failed();
    EXPECT_TRUE(rig.engine.submit(&late.t));
    EXPECT_TRUE(log.empty());
  };
  ASSERT_TRUE(rig.engine.submit(&running.t));
  rig.engine.poll();
  rig.port.advance_us(60 * MS);
  rig.engine.poll();  // times out, the abort fails
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR},
                          {2, TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}}));
  EXPECT_TRUE(rig.engine.is_idle());
}

// ---- On engine level: OK only with a complete decode --------------------------------------

namespace {

bool section_capture_is_valid(const Section &section) {
  const size_t expected = section.tx.size();
  if (section.capture.size() < expected)
    return false;
  if (!section.capture.empty()) {
    const Symbol &last = section.capture.back();
    if ((last.duration0 == 0 && last.level0 == 0) || last.level1 == 0)
      return false;
  }
  for (size_t i = 0; i < expected; i++) {
    if (section.capture[i].level0 != 0 || section.capture[i].duration0 == 0 ||
        section.capture[i].duration0 >= Timing{}.slot_us)
      return false;
  }
  for (size_t i = expected; i < section.capture.size(); i++) {
    if (section.capture[i].duration0 != 0 || section.capture[i].duration1 != 0 || section.capture[i].level0 != 1)
      return false;
  }
  return true;
}

}  // namespace

class RmtOneWireFuzz : public ::testing::TestWithParam<size_t> {};

INSTANTIATE_TEST_SUITE_P(W, RmtOneWireFuzz, ::testing::Values(size_t{48}, size_t{64}),
                         [](const ::testing::TestParamInfo<size_t> &info) {
                           return std::to_string(info.param);  // NOLINT
                         });

TEST_P(RmtOneWireFuzz, OkOnlyWithACompleteDecode) {
  std::mt19937 rng(0xCAFE);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  int ok = 0;
  int errors = 0;
  for (int round = 0; round < 3000; round++) {
    Rig rig(GetParam());
    const size_t read_len = 1 + rng() % 40;
    std::vector<uint8_t> payload(read_len);
    for (auto &byte : payload)
      byte = static_cast<uint8_t>(rng());
    auto &device = rig.port.bus.devices.emplace_back(ROM_A, payload, 1);
    device.zero_low_us = 15 + rng() % 46;              // tRDV..end of slot
    rig.port.bus.rise_us = 1 + rng() % BUDGET_TRC_US;  // up to the full budget of 9 us
    rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
      if (section == 0 || rig.port.sections[section].tx.empty())
        return;
      switch (rng() % 14) {
        case 0:
          capture.resize(rng() % (capture.size() + 1));
          break;
        case 1:
          capture.push_back(Symbol{static_cast<uint16_t>(1 + rng() % 90), 0, static_cast<uint16_t>(rng() % 90), 1});
          break;
        case 2:
          if (!capture.empty())
            capture[rng() % capture.size()].level0 = 1;
          break;
        case 3:
          capture.push_back(Symbol{0, 0, 0, 1});
          break;
        case 4:
          if (!capture.empty())
            capture.back().level1 = 0;
          break;
        case 5:
          if (!capture.empty())
            capture[rng() % capture.size()].duration0 = Timing{}.slot_us + rng() % 100;
          break;
        case 6:
          if (!capture.empty())
            capture.back() = Symbol{0, 0, static_cast<uint16_t>(rng() % 3 == 0 ? 0 : rng() % 32768), 1};
          break;
        case 7:
          capture.push_back(Symbol{0, 0, static_cast<uint16_t>(rng() % 3 == 0 ? 0 : rng() % 32768), 1});
          break;
        case 8:
          capture.push_back(Symbol{0, 1, 0, 1});
          break;
        default:
          break;
      }
    };
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, read_len);
    ASSERT_TRUE(rig.engine.submit(&job.t));
    rig.pump();
    ASSERT_EQ(log.size(), 1u) << "round " << round;

    bool all_valid = true;
    for (size_t i = 1; i < rig.port.sections.size(); i++) {
      const auto &section = rig.port.sections[i];
      if (!section.armed)
        continue;
      if (!section_capture_is_valid(section)) {
        all_valid = false;
        EXPECT_EQ(i, rig.port.sections.size() - 1) << "round " << round << ": ran on after a bad capture";
        break;
      }
    }
    if (all_valid) {
      ok++;
      ASSERT_EQ(log[0].status, TransactionStatus::TRANSACTION_STATUS_OK) << "round " << round;
      ASSERT_EQ(job.read_bytes, payload) << "round " << round;
    } else {
      errors++;
      ASSERT_EQ(log[0].status, TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR) << "round " << round;
    }
  }
  EXPECT_GT(ok, 100);
  EXPECT_GT(errors, 100);
}

// ---- Timing (rmt_timing option) --------------------------------------------------------

namespace {

/// Every armed section has its RX idle (max(100, slot + 20) us), the RESET section stays at 600 us.
void expect_rx_idle(const SimPort &port, uint32_t idle_us) {
  int resets = 0;
  int others = 0;
  for (size_t i = 0; i < port.sections.size(); i++) {
    const auto &section = port.sections[i];
    if (!section.armed)
      continue;
    if (section.is_reset()) {
      resets++;
      EXPECT_EQ(section.idle_us, 600u) << "section " << i;
    } else {
      others++;
      EXPECT_EQ(section.idle_us, idle_us) << "section " << i;
    }
  }
  EXPECT_GT(resets, 0);
  EXPECT_GT(others, 0);
}

/// Runs READ_SCRATCHPAD against one device, the result must be OK with the right data.
void read_scratchpad_ok(Rig &rig) {
  add_scratchpad_device(rig);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

}  // namespace

TEST(RmtOneWireEngine, DefaultRxIdleIs130) {
  Rig rig;
  read_scratchpad_ok(rig);
  expect_rx_idle(rig.port, 130);
}

TEST(RmtOneWireEngine, RxIdleIsSlotPlus20WithAFloorOf100) {
  // The floor of 100 holds up to slot 80 and ends at slot 81
  const struct {
    uint8_t write0_low;
    uint8_t slot;
    uint32_t idle;
  } cases[] = {{60, 71, 100}, {65, 80, 100}, {65, 81, 101}, {65, 100, 120}, {90, 120, 140}};
  for (const auto &entry : cases) {
    SCOPED_TRACE(static_cast<int>(entry.slot));
    Rig rig(64, Timing{entry.write0_low, entry.slot, 14});
    read_scratchpad_ok(rig);
    expect_rx_idle(rig.port, entry.idle);
  }
}

TEST(RmtOneWireEngine, GetTimingReturnsTheTimingOfTheConstructor) {
  // dump_config() of the bus reads the timing from here
  Rig rig(64, Timing{60, 100, 12});
  EXPECT_EQ(rig.engine.get_timing().write0_low_us, 60);
  EXPECT_EQ(rig.engine.get_timing().slot_us, 100);
  EXPECT_EQ(rig.engine.get_timing().read_threshold_us, 12);
  Rig defaults;
  EXPECT_EQ(defaults.engine.get_timing().write0_low_us, Timing{}.write0_low_us);
  EXPECT_EQ(defaults.engine.get_timing().slot_us, Timing{}.slot_us);
  EXPECT_EQ(defaults.engine.get_timing().read_threshold_us, Timing{}.read_threshold_us);
}

TEST(RmtOneWireEngine, ReachesTheWire) {
  Rig rig(64, Timing{90, 120, 14});
  read_scratchpad_ok(rig);
  int zeros = 0;
  int ones = 0;
  int reads = 0;
  for (const auto &section : rig.port.sections) {
    if (section.is_reset()) {
      EXPECT_EQ(section.tx[0].duration0, 520);
      EXPECT_EQ(section.tx[0].duration1, 500);
      continue;
    }
    for (const auto &symbol : section.tx) {
      EXPECT_EQ(symbol.duration0 + symbol.duration1, 120);
      if (section.armed) {
        reads++;
        EXPECT_EQ(symbol.duration0, 4);
      } else if (symbol.duration0 == 90) {
        zeros++;
      } else {
        ones++;
        EXPECT_EQ(symbol.duration0, 6);
      }
    }
  }
  EXPECT_GT(zeros, 0);
  EXPECT_GT(ones, 0);
  EXPECT_EQ(reads, 72);
}

TEST(RmtOneWireEngine, ThresholdReachesTheDecoder) {
  // The slowest rise the bus budget allows: a 1 is Low for 4 + 9 = 13 us
  auto read_with = [](const Timing &timing, uint32_t zero_low_us) {
    Rig rig(64, timing);
    rig.port.bus.rise_us = BUDGET_TRC_US;
    auto &device = rig.port.bus.devices.emplace_back(ROM_A, PAYLOAD, 1);
    device.zero_low_us = zero_low_us;
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    EXPECT_TRUE(rig.engine.submit(&job.t));
    rig.pump();
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
    return job.read_bytes;
  };
  EXPECT_EQ(read_with(Timing{}, 15), PAYLOAD) << "the default threshold reads a 13 us Low as 1";
  EXPECT_EQ(read_with(Timing{90, 110, 13}, 15), std::vector<uint8_t>(9, 0x00)) << "a threshold of 13 reads it as 0";
  EXPECT_EQ(read_with(Timing{65, 80, 25}, 20), std::vector<uint8_t>(9, 0xFF)) << "a threshold of 25 reads 20 us as 1";
}

TEST(RmtOneWireEngine, ReadDataIsClearedBeforeTheFirstReadSection) {
  // The decoder sets and clears single bits, so read_data must not need to be initialized by the caller
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  int read_sections = 0;
  rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &) {
    const auto &tx = rig.port.sections[section].tx;
    if (rig.port.sections[section].is_reset() || tx.empty() || classify_slot(tx[0]) != Slot::READ)
      return;
    if (read_sections++ == 0) {
      EXPECT_EQ(job.read_bytes, std::vector<uint8_t>(9, 0x00));
    }
  };
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(read_sections, 2);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, ReadDataMayShareTheMemoryOfWriteData) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job job(&log, 1, ROM_A, {}, 9);
  job.read_bytes[0] = READ_SCRATCHPAD;
  job.t.write_data = job.read_bytes.data();
  job.t.write_len = 1;
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  ASSERT_EQ(rig.port.bus.devices[0].received().size(), 1u);
  EXPECT_EQ(rig.port.bus.devices[0].received()[0], READ_SCRATCHPAD);
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, SlotLimitReachesTheDecoder) {
  // Errors_LowTimeLimitIn109Out110 with slot 120
  const std::vector<uint8_t> ones(9, 0xFF);
  for (size_t patched_section : {size_t{3}, size_t{4}}) {
    for (uint16_t low : {uint16_t{119}, uint16_t{120}}) {
      Rig rig(64, Timing{90, 120, 14});
      rig.port.bus.devices.emplace_back(ROM_A, ones, 1);
      rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
        if (section == patched_section)
          capture[5].duration0 = low;
      };
      DoneLog log;
      Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
      ASSERT_TRUE(rig.engine.submit(&job.t));
      rig.pump();
      const TransactionStatus expected =
          low < 120 ? TransactionStatus::TRANSACTION_STATUS_OK : TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR;
      EXPECT_EQ(log, (DoneLog{{1, expected}})) << "section " << patched_section << " low " << low;
    }
  }
}

TEST(RmtOneWireEngine, BlockingMethodsAndSearchUseIt) {
  constexpr uint8_t skip_rom = 0xCC;
  constexpr uint8_t search_rom = 0xF0;
  Rig rig(64, Timing{90, 120, 14});
  rig.port.bus.devices.emplace_back(ROM_A, PAYLOAD, 0);

  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&skip_rom, 8));
  uint8_t data = 0;
  ASSERT_TRUE(rig.engine.read_blocking(&data, 8));
  EXPECT_EQ(data, PAYLOAD[0]);

  rig.engine.reset_search();
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&search_rom, 8));
  EXPECT_EQ(rig.engine.search_next(), ROM_A);

  expect_rx_idle(rig.port, 140);
  for (const auto &section : rig.port.sections) {
    if (section.is_reset())
      continue;
    for (const auto &symbol : section.tx)
      EXPECT_EQ(symbol.duration0 + symbol.duration1, 120);
  }
}

// ---- Calls from log handlers: logging runs handlers at once, inside the engine ------------------

namespace {

/// Calls into the engine like a log handler that uses the bus. While the engine is busy, all of this must fail at
/// once and touch neither the bus nor the clock. Returns how many port calls the nested calls made.
int call_everything_nested(Rig &rig) {
  const size_t calls = rig.port.calls.size();
  const uint64_t now = rig.port.now_us();
  EXPECT_TRUE(rig.engine.is_busy());
  EXPECT_EQ(rig.engine.reset_blocking(), 0);
  const uint8_t byte = 0xCC;
  EXPECT_FALSE(rig.engine.write_blocking(&byte, 8));
  uint8_t data[2] = {0x00, 0x00};
  EXPECT_FALSE(rig.engine.read_blocking(data, 16));
  EXPECT_EQ(data[0], 0xFF);
  EXPECT_EQ(data[1], 0xFF);
  uint8_t partial[2] = {0x00, 0x50};
  EXPECT_FALSE(rig.engine.read_blocking(partial, 12));
  EXPECT_EQ(partial[0], 0xFF);
  EXPECT_EQ(partial[1], 0x5F) << "the 4 bits that were not requested stay";
  EXPECT_EQ(rig.engine.search_next(), 0u);
  rig.engine.poll();
  EXPECT_EQ(rig.port.now_us(), now);
  return static_cast<int>(rig.port.calls.size() - calls);
}

/// Runs call_everything_nested() at every start_tx(), once per start.
void nest_at_every_start(Rig &rig, int *nested, int *port_calls) {
  rig.port.on_start_tx = [&rig, nested, port_calls]() {
    static bool inside = false;
    if (inside)
      return;
    inside = true;
    (*nested)++;
    *port_calls += call_everything_nested(rig);
    inside = false;
  };
}

}  // namespace

TEST(RmtOneWireEngine, NestedCallsDuringPollFailWithoutBusAccess) {
  Rig rig;
  add_scratchpad_device(rig);
  int nested = 0;
  int port_calls = 0;
  nest_at_every_start(rig, &nested, &port_calls);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(nested, 5) << "RESET WRITE WRITE READ READ";
  EXPECT_EQ(port_calls, 0);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(job.read_bytes, PAYLOAD);
  EXPECT_FALSE(rig.engine.is_busy());
}

TEST(RmtOneWireEngine, NestedCallsDuringABlockingMethodFailWithoutBusAccess) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A, PAYLOAD, 0);
  int nested = 0;
  int port_calls = 0;
  nest_at_every_start(rig, &nested, &port_calls);
  const uint8_t skip_rom = 0xCC;
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&skip_rom, 8));
  std::vector<uint8_t> data(9);
  ASSERT_TRUE(rig.engine.read_blocking(data.data(), 72));
  EXPECT_EQ(data, PAYLOAD);
  EXPECT_EQ(nested, 4);
  EXPECT_EQ(port_calls, 0);
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:8 READ:62 READ:10");
  EXPECT_FALSE(rig.engine.is_busy());
}

TEST(RmtOneWireEngine, NestedCallsInsideAnAbortFailWithoutBusAccess) {
  // abort_section_() logs a warning just before the abort
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.drop_rx_at = 3;
  int nested = 0;
  int port_calls = 0;
  rig.port.on_abort = [&]() {
    nested++;
    port_calls += call_everything_nested(rig);
  };
  DoneLog log;
  Job lost(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.pump();
  EXPECT_EQ(nested, 1);
  EXPECT_EQ(port_calls, 0);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_TIMEOUT},
                          {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_EQ(next.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, OnDoneMayCallBlockingMethods) {
  // The queue and the step are consistent inside on_done, so the guard is lifted there, from poll() and from the
  // wait of a blocking method alike
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A, PAYLOAD, 0);
  DoneLog log;
  Job first(&log, 1, 0, {}, 0);
  Job second(&log, 2, 0, {}, 0);
  std::vector<uint8_t> data(9);
  const uint8_t skip_rom = 0xCC;
  auto read_in_on_done = [&](TransactionStatus) {
    EXPECT_FALSE(rig.engine.is_busy());
    EXPECT_EQ(rig.engine.reset_blocking(), 1);
    EXPECT_TRUE(rig.engine.write_blocking(&skip_rom, 8));
    EXPECT_TRUE(rig.engine.read_blocking(data.data(), 72));
  };
  first.then = read_in_on_done;
  second.then = read_in_on_done;

  ASSERT_TRUE(rig.engine.submit(&first.t));
  rig.pump();
  EXPECT_EQ(data, PAYLOAD);
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:8 RESET WRITE:8 READ:62 READ:10");

  data.assign(9, 0);
  ASSERT_TRUE(rig.engine.submit(&second.t));
  rig.engine.poll();
  EXPECT_EQ(rig.engine.reset_blocking(), 1) << "second ends inside this wait, its on_done reads first";
  EXPECT_EQ(data, PAYLOAD);
  EXPECT_EQ(rig.port.summary(6), "RESET WRITE:8 RESET WRITE:8 READ:62 READ:10 RESET");
  EXPECT_EQ(log,
            (DoneLog{{1, TransactionStatus::TRANSACTION_STATUS_OK}, {2, TransactionStatus::TRANSACTION_STATUS_OK}}));
  EXPECT_FALSE(rig.engine.is_busy());
  EXPECT_TRUE(rig.engine.is_idle());
}

}  // namespace esphome::gpio::rmt_one_wire::testing
