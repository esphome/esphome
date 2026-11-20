#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <new>
#include <random>
#include <vector>

#include "../common.h"

// Counts allocations for the "kein Heap" tests (section 2, section 4.1). Forwards to malloc, so
// sanitizers keep seeing every block; only while a HeapWatch is active anything is counted. Test
// builds have no exceptions: the plain form aborts when out of memory, the nothrow forms return null.
namespace {
void *counted_alloc(std::size_t size) noexcept {
  if (esphome::gpio::rmt_one_wire::testing::g_heap_watch)
    esphome::gpio::rmt_one_wire::testing::g_heap_allocs++;
  return std::malloc(size == 0 ? 1 : size);
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
void operator delete(void *block) noexcept { std::free(block); }
void operator delete(void *block, std::size_t) noexcept { std::free(block); }

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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:8");
}

TEST(RmtOneWireEngine, DeviceReceivesTheWrittenBytes) {
  Rig rig;
  auto &device = rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{}, 100);
  DoneLog log;
  Job job(&log, 1, ROM_A, {0x4E, 0x00, 0xFF, 0xA5}, 0);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

// ---- Rule 1: the completion beats the timeout ------------------------------------------------

TEST(RmtOneWireEngine, Rule1_CompletionWinsAfterLoopWasBlocked200ms) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, Rule1_EachSectionHasItsOwnTimeout) {
  // Each section needs 40 ms, the whole transaction far more than 50 ms
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.section_delay_us = 40 * MS;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_GT(rig.port.now_us(), 150 * MS);
}

TEST(RmtOneWireEngine, Rule1_NextSectionTimesFromItsOwnStartAfterBlockedLoop) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}}));
}

// ---- Rule 4: on_done runs in the main loop and may submit ------------------------------------

TEST(RmtOneWireEngine, Rule4_OnDoneMaySubmitTheSameTransactionAgain) {
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
    EXPECT_EQ(entry.status, TransactionStatus::OK);
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
  EXPECT_EQ(rig.port.frames().size(), 4u);
}

TEST(RmtOneWireEngine, Rule4_OnDoneMaySubmitAnotherTransactionAndKeepsTheLoop) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}, {2, TransactionStatus::OK}}));
  EXPECT_EQ(second.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, Rule4_NullOnDoneIsAllowed) {
  Rig rig;
  add_scratchpad_device(rig);
  DoneLog log;
  Job silent(&log, 1, 0, {0x44}, 0);
  Job next(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  silent.t.on_done = nullptr;
  ASSERT_TRUE(rig.engine.submit(&silent.t));
  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{2, TransactionStatus::OK}}));
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

TEST(RmtOneWireEngine, Rule4_NullOnDoneOnEveryErrorEnd) {
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

TEST(RmtOneWireEngine, Precheck_LineLowAtTheStartIsBusLowAtOnce) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.line_low_until_us = 1000000;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.engine.poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::BUS_LOW}}));
  EXPECT_EQ(rig.port.tx_count, 0);
  EXPECT_EQ(rig.port.arm_count, 0);
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, Precheck_NextTransactionRunsOnceTheLineIsBack) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.line_low_until_us = 1000;
  DoneLog log;
  Job first(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  Job second(&log, 2, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&first.t));
  ASSERT_TRUE(rig.engine.submit(&second.t));
  rig.engine.poll();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::BUS_LOW}}));
  EXPECT_TRUE(rig.port.sections.empty());
  rig.port.advance_us(2000);
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::BUS_LOW}, {2, TransactionStatus::OK}}));
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}, {2, TransactionStatus::OK}}));
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::NO_PRESENCE},
                          {2, TransactionStatus::TIMEOUT},
                          {1, TransactionStatus::HARDWARE_ERROR}}));
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::NO_PRESENCE}}));
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

TEST(RmtOneWireEngine, Rule2_CompletionsTouchNothingButFlags) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
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

TEST(RmtOneWireEngine, Rule2_CompletionsReportTheLastExpectedEvent) {
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

// ---- I7: FIFO, atomic on the bus -------------------------------------------------------------

TEST(RmtOneWireEngine, I7_TransactionsRunInFifoOrderWithoutInterleaving) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK},
                          {2, TransactionStatus::OK},
                          {3, TransactionStatus::OK},
                          {4, TransactionStatus::OK}}));
  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), 4u);
  EXPECT_EQ(slots_to_string(frames[0]), slots_to_string(a.slots()));
  EXPECT_EQ(slots_to_string(frames[1]), slots_to_string(b.slots()));
  EXPECT_EQ(slots_to_string(frames[2]), slots_to_string(c.slots()));
  EXPECT_EQ(slots_to_string(frames[3]), slots_to_string(d.slots()));
  EXPECT_EQ(a.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, I7_SubmitDuringEverySectionDoesNotDisturbTheRunningOne) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}, {2, TransactionStatus::OK}, {3, TransactionStatus::OK}}));
  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), 3u);
  EXPECT_EQ(slots_to_string(frames[0]), slots_to_string(a.slots()));
  EXPECT_EQ(slots_to_string(frames[1]), slots_to_string(b.slots()));
  EXPECT_EQ(slots_to_string(frames[2]), slots_to_string(c.slots()));
  EXPECT_EQ(a.read_bytes, PAYLOAD);
  EXPECT_EQ(b.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, I7_OnDoneSubmitQueuesBehindWhatIsAlreadyWaiting) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK},
                          {2, TransactionStatus::OK},
                          {3, TransactionStatus::OK},
                          {4, TransactionStatus::OK}}));
}

TEST(RmtOneWireEngine, I7_SubmitOfAPendingTransactionIsRefusedAndChangesNothing) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}, {2, TransactionStatus::OK}}));
  EXPECT_EQ(rig.port.frames().size(), 2u);
  EXPECT_TRUE(rig.engine.submit(&a.t)) << "done, accepted again";
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
}

TEST(RmtOneWireEngine, I7_ReusedNodesInAnotherOrderDoNotLoseOrRepeat) {
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
  EXPECT_EQ(log, (DoneLog{{3, TransactionStatus::OK}, {1, TransactionStatus::OK}}));
  log.clear();
  ASSERT_TRUE(rig.engine.submit(&a.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_EQ(rig.port.frames().size(), 6u);
}

TEST(RmtOneWireEngine, I7_ManyTransactionsKeepTheOrder) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  DoneLog log;
  std::deque<Job> jobs;
  DoneLog expected;
  for (int i = 0; i < 40; i++) {
    jobs.emplace_back(&log, i, 0, std::vector<uint8_t>{static_cast<uint8_t>(i)}, 0);
    expected.push_back({i, TransactionStatus::OK});
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

// ---- I9: abort and recovery ------------------------------------------------------------------

struct LostCompletion {
  const char *name;
  bool lose_tx;
  bool lose_rx;
};

class RmtOneWireLostCompletion : public ::testing::TestWithParam<LostCompletion> {};

INSTANTIATE_TEST_SUITE_P(I9, RmtOneWireLostCompletion,
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
  EXPECT_EQ(log[0], (DoneEntry{1, TransactionStatus::TIMEOUT}));

  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}, {2, TransactionStatus::OK}}));
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}}));
  EXPECT_EQ(rig.port.sections.size(), 4u);
  EXPECT_EQ(rig.port.abort_count, 1);
}

TEST(RmtOneWireEngine, I9_TimeoutOfAWriteSectionOnlyNeedsTx) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.drop_tx_at = 1;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}}));
  EXPECT_EQ(rig.port.sections.size(), 2u);
}

TEST(RmtOneWireEngine, I9_TimeoutWorksAcrossTheClockWrap) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}}));
}

namespace {

/// Drives the next transaction by hand and checks that late messages did not advance it.
void expect_next_is_not_advanced_and_completes(Rig &rig, DoneLog &log, Job &next, size_t sections_before,
                                               TransactionStatus first_status = TransactionStatus::TIMEOUT) {
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
  EXPECT_EQ(log, (DoneLog{{1, first_status}, {2, TransactionStatus::OK}}));
  EXPECT_EQ(next.read_bytes, PAYLOAD);
}

}  // namespace

TEST(RmtOneWireEngine, I9_LateCompletionInsideAbortAfterFailedStartIsDiscarded) {
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
  expect_next_is_not_advanced_and_completes(rig, log, next, 0, TransactionStatus::HARDWARE_ERROR);
}

TEST(RmtOneWireEngine, I9_LateCompletionInsideAbortIsDiscarded) {
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

TEST(RmtOneWireEngine, I9_LateCompletionBetweenAbortAndNextStartIsDiscarded) {
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

TEST(RmtOneWireEngine, I9_LateCompletionWhileIdleIsDiscarded) {
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
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}}));
  ASSERT_TRUE(rig.engine.is_idle());

  rig.engine.on_tx_done();
  rig.engine.on_rx_done(1);
  EXPECT_TRUE(rig.engine.is_idle());

  ASSERT_TRUE(rig.engine.submit(&next.t));
  expect_next_is_not_advanced_and_completes(rig, log, next, 1);
}

TEST(RmtOneWireEngine, I9_PartialCompletionOfTheAbortedSectionDoesNotLeak) {
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
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}}));
  rig.poll_times(3);
  ASSERT_EQ(rig.port.sections.size(), 2u);

  rig.port.fire_rx();
  rig.poll_times(3);
  EXPECT_EQ(rig.port.sections.size(), 2u) << "stale TX completion was taken for the new section";
  rig.port.fire_tx();
  rig.poll_times(1);
  EXPECT_EQ(rig.port.sections.size(), 3u);
}

TEST(RmtOneWireEngine, I9_AbortThatFailsEndsAllWorkInHardwareError) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::HARDWARE_ERROR}, {2, TransactionStatus::HARDWARE_ERROR}}));
  EXPECT_EQ(rig.port.tx_count, 1);

  ASSERT_TRUE(rig.engine.submit(&later.t));
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
  EXPECT_EQ(log.back(), (DoneEntry{3, TransactionStatus::HARDWARE_ERROR}));
  EXPECT_EQ(rig.port.tx_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.arm_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.abort_count, 1) << "no RMT call on a failed bus";
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

// ---- Section 7: error table ------------------------------------------------------------------

TEST(RmtOneWireEngine, Errors_NoDeviceIsNoPresenceAndStopsAfterTheReset) {
  Rig rig;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::NO_PRESENCE}}));
  EXPECT_EQ(rig.port.summary(), "RESET");
  EXPECT_EQ(rig.port.abort_count, 0);
}

TEST(RmtOneWireEngine, Errors_WireToGndAfterPrecheckIsBusLow) {
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
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::BUS_LOW}})) << held.duration0 << "/" << held.duration1;
    EXPECT_EQ(rig.port.summary(), "RESET");
  }
}

TEST(RmtOneWireEngine, Errors_LineStuckBeforeTheStartIsBusLow) {
  Rig rig;
  add_scratchpad_device(rig);
  rig.port.stuck_low = true;
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::BUS_LOW}}));
  EXPECT_EQ(rig.port.tx_count, 0);
}

TEST(RmtOneWireEngine, Errors_WrongResetCaptureIsCaptureError) {
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
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::CAPTURE_ERROR}}));
    EXPECT_EQ(rig.port.summary(), "RESET");
  }
}

TEST(RmtOneWireEngine, Errors_BadReadCaptureStopsTheTransaction) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::CAPTURE_ERROR}}));
  EXPECT_EQ(rig.port.sections.size(), 4u) << "the second READ must not start";
  EXPECT_EQ(rig.port.abort_count, 0);
}

TEST(RmtOneWireEngine, Errors_LineToGndDuringReadIsCaptureError) {
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
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::CAPTURE_ERROR}}));
    EXPECT_EQ(rig.port.sections.size(), 4u);
  }
}

TEST(RmtOneWireEngine, Errors_LineToGndInTheLastSlotIsCaptureError) {
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
        EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::CAPTURE_ERROR}}))
            << "section " << failing_section << " stale " << stale << " behind " << behind;
        EXPECT_EQ(rig.port.sections.size(), failing_section + 1);
      }
    }
  }
}

TEST(RmtOneWireEngine, Errors_LowTimeLimitIn109Out110) {
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
        EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}})) << "109 us is a slow 0";
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
        EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::CAPTURE_ERROR}})) << "110 us is a held bus";
      }
    }
  }
}

TEST(RmtOneWireEngine, Errors_BadCrcIsStillOk) {
  // The engine does not know about CRCs, that is check_scratch_pad_() of the consumer
  Rig rig;
  std::vector<uint8_t> broken = PAYLOAD;
  broken[8] ^= 0xFF;
  rig.port.bus.devices.emplace_back(ROM_A, broken, 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::HARDWARE_ERROR}}));
  EXPECT_EQ(rig.port.sections.size(), GetParam().sections_started);
  EXPECT_EQ(rig.port.tx_count, GetParam().tx_calls) << "TX must not start after arm_rx() failed";
  EXPECT_EQ(rig.port.abort_count, GetParam().aborts);
  EXPECT_EQ(aborts_at_on_done, GetParam().aborts) << "abort() comes before the transaction ends";

  ASSERT_TRUE(rig.engine.submit(&next.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::HARDWARE_ERROR}, {2, TransactionStatus::OK}}));
  EXPECT_EQ(next.read_bytes, PAYLOAD);
}

TEST(RmtOneWireEngine, Errors_FailedStartWhoseAbortFailsIsPermanent) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::HARDWARE_ERROR}, {2, TransactionStatus::HARDWARE_ERROR}}));
  ASSERT_TRUE(rig.engine.submit(&later.t));
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
  EXPECT_EQ(log.back(), (DoneEntry{3, TransactionStatus::HARDWARE_ERROR}));
  EXPECT_EQ(rig.port.tx_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.arm_count, 1) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.abort_count, 1) << "no RMT call on a failed bus";
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, Errors_SetFailedEndsRunningQueuedAndFutureWork) {
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::HARDWARE_ERROR}, {2, TransactionStatus::HARDWARE_ERROR}}));
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());

  // Completions of the abandoned section change nothing
  rig.engine.on_tx_done();
  rig.engine.on_rx_done(1);

  ASSERT_TRUE(rig.engine.submit(&future.t));
  rig.pump();
  EXPECT_EQ(log.size(), 3u);
  EXPECT_EQ(log.back(), (DoneEntry{3, TransactionStatus::HARDWARE_ERROR}));
  EXPECT_EQ(rig.port.arm_count, arms);
  EXPECT_EQ(rig.port.tx_count, transmits);
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireEngine, Errors_FailedBusStillRefusesAPendingTransactionOnly) {
  Rig rig;
  rig.engine.set_failed();
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::HARDWARE_ERROR}}));
  ASSERT_TRUE(rig.engine.submit(&job.t)) << "the transaction is no longer pending";
  rig.pump();
  EXPECT_EQ(log.size(), 2u);
}

// ---- I5 on engine level: OK only with a complete decode --------------------------------------

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

TEST_P(RmtOneWireFuzz, I5_OkOnlyWithACompleteDecode) {
  std::mt19937 rng(0xCAFE);
  int ok = 0;
  int errors = 0;
  for (int round = 0; round < 3000; round++) {
    Rig rig(GetParam());
    const size_t read_len = 1 + rng() % 40;
    std::vector<uint8_t> payload(read_len);
    for (auto &byte : payload)
      byte = static_cast<uint8_t>(rng());
    auto &device = rig.port.bus.devices.emplace_back(ROM_A, payload, 1);
    device.zero_low_us = 15 + rng() % 46;  // tRDV..end of slot
    rig.port.bus.rise_us = 1 + rng() % 8;
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
      ASSERT_EQ(log[0].status, TransactionStatus::OK) << "round " << round;
      ASSERT_EQ(job.read_bytes, payload) << "round " << round;
    } else {
      errors++;
      ASSERT_EQ(log[0].status, TransactionStatus::CAPTURE_ERROR) << "round " << round;
    }
  }
  EXPECT_GT(ok, 100);
  EXPECT_GT(errors, 100);
}

// ---- Timing, section 4.6 and section 15 --------------------------------------------------------

namespace {

/// Every armed section has the RX idle of section 4.3, the RESET section stays at 600 us.
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
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(job.read_bytes, PAYLOAD);
}

}  // namespace

TEST(RmtOneWireEngine, Timing_DefaultRxIdleIs130) {
  Rig rig;
  read_scratchpad_ok(rig);
  expect_rx_idle(rig.port, 130);
}

TEST(RmtOneWireEngine, Timing_RxIdleIsSlotPlus20WithAFloorOf100) {
  // The floor of 100 holds up to slot 80 and ends at slot 81
  const struct {
    uint8_t write0_low;
    uint8_t slot;
    uint32_t idle;
  } cases[] = {{60, 71, 100}, {65, 80, 100}, {65, 81, 101}, {65, 100, 120}, {90, 120, 140}};
  for (const auto &entry : cases) {
    SCOPED_TRACE(static_cast<int>(entry.slot));
    Rig rig(64, Timing{entry.write0_low, entry.slot, 13});
    read_scratchpad_ok(rig);
    expect_rx_idle(rig.port, entry.idle);
  }
}

TEST(RmtOneWireEngine, Timing_ReachesTheWire) {
  Rig rig(64, Timing{90, 120, 13});
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

TEST(RmtOneWireEngine, Timing_ThresholdReachesTheDecoder) {
  // A slow rise: the Low time of a 1 is 4 + 18 = 22 us, a 0 of the device 40 us
  auto setup_bus = [](Rig &rig) {
    rig.port.bus.rise_us = 18;
    auto &device = rig.port.bus.devices.emplace_back(ROM_A, PAYLOAD, 1);
    device.zero_low_us = 40;
  };
  {
    Rig rig(64, Timing{65, 80, 25});
    setup_bus(rig);
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    ASSERT_TRUE(rig.engine.submit(&job.t));
    rig.pump();
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
    EXPECT_EQ(job.read_bytes, PAYLOAD);
  }
  {
    // Counter-check on the same bus: with the default threshold every bit reads as 0
    Rig rig;
    setup_bus(rig);
    DoneLog log;
    Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
    ASSERT_TRUE(rig.engine.submit(&job.t));
    rig.pump();
    EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
    EXPECT_EQ(job.read_bytes, std::vector<uint8_t>(9, 0x00));
  }
}

TEST(RmtOneWireEngine, Timing_SlotLimitReachesTheDecoder) {
  // Errors_LowTimeLimitIn109Out110 with slot 120
  const std::vector<uint8_t> ones(9, 0xFF);
  for (size_t patched_section : {size_t{3}, size_t{4}}) {
    for (uint16_t low : {uint16_t{119}, uint16_t{120}}) {
      Rig rig(64, Timing{90, 120, 13});
      rig.port.bus.devices.emplace_back(ROM_A, ones, 1);
      rig.port.capture_mutator = [&](size_t section, std::vector<Symbol> &capture) {
        if (section == patched_section)
          capture[5].duration0 = low;
      };
      DoneLog log;
      Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
      ASSERT_TRUE(rig.engine.submit(&job.t));
      rig.pump();
      const TransactionStatus expected = low < 120 ? TransactionStatus::OK : TransactionStatus::CAPTURE_ERROR;
      EXPECT_EQ(log, (DoneLog{{1, expected}})) << "section " << patched_section << " low " << low;
    }
  }
}

TEST(RmtOneWireEngine, Timing_BlockingMethodsAndSearchUseIt) {
  constexpr uint8_t SKIP_ROM = 0xCC;
  constexpr uint8_t SEARCH_ROM = 0xF0;
  Rig rig(64, Timing{90, 120, 13});
  rig.port.bus.devices.emplace_back(ROM_A, PAYLOAD, 0);

  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SKIP_ROM, 8));
  uint8_t data = 0;
  ASSERT_TRUE(rig.engine.read_blocking(&data, 8));
  EXPECT_EQ(data, PAYLOAD[0]);

  rig.engine.reset_search();
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
  EXPECT_EQ(rig.engine.search_next(), ROM_A);

  expect_rx_idle(rig.port, 140);
  for (const auto &section : rig.port.sections) {
    if (section.is_reset())
      continue;
    for (const auto &symbol : section.tx)
      EXPECT_EQ(symbol.duration0 + symbol.duration1, 120);
  }
}

}  // namespace esphome::gpio::rmt_one_wire::testing
