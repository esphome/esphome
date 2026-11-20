#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "../common.h"

namespace esphome::gpio::rmt_one_wire::testing {

namespace {

constexpr uint8_t SKIP_ROM = 0xCC;
constexpr uint8_t SEARCH_ROM = 0xF0;
constexpr uint64_t MS = 1000;

std::vector<uint8_t> pattern_bytes(size_t count) {
  std::vector<uint8_t> bytes(count);
  for (size_t i = 0; i < count; i++)
    bytes[i] = static_cast<uint8_t>(0x35 + i * 7);
  return bytes;
}

/// reset + Skip ROM by the blocking methods, the device starts sending after that.
void select_blocking(Rig &rig) {
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SKIP_ROM, 8));
}

/// The search loop of OneWireBus::search(), without its CRC filter.
std::vector<uint64_t> search_all(Rig &rig) {
  std::vector<uint64_t> found;
  rig.engine.reset_search();
  for (int guard = 0; guard < 100; guard++) {
    if (rig.engine.reset_blocking() != 1)
      return found;
    if (!rig.engine.write_blocking(&SEARCH_ROM, 8))
      return found;
    const uint64_t address = rig.engine.search_next();
    if (address == 0)
      return found;
    found.push_back(address);
  }
  ADD_FAILURE() << "search does not end";
  return found;
}

// ---------------------------------------------------------------------------------------------
// I8: reference. search_int() and the loop of OneWireBus::search() from upstream
// (gpio_one_wire.cpp and one_wire_bus.cpp at ae54e8b696), on the same simulated bus model.
// ---------------------------------------------------------------------------------------------
class UpstreamSearch {
 public:
  explicit UpstreamSearch(SimBus *bus) : bus_(bus) {}

  std::vector<uint64_t> search() {
    std::vector<uint64_t> found;
    this->reset_search();
    while (true) {
      if (!this->reset_())
        return found;
      this->write8_(SEARCH_ROM);
      const uint64_t address = this->search_int();
      if (address == 0)
        break;
      found.push_back(address);
    }
    return found;
  }

  void reset_search() {
    this->last_discrepancy_ = 0;
    this->last_device_flag_ = false;
    this->address_ = 0;
  }

  uint64_t search_int() {
    if (this->last_device_flag_)
      return 0u;

    uint8_t last_zero = 0;
    uint64_t bit_mask = 1;
    uint64_t address = this->address_;

    // Initiate search
    for (int bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
      // read bit
      bool id_bit = this->read_bit_();
      // read its complement
      bool cmp_id_bit = this->read_bit_();

      if (id_bit && cmp_id_bit) {
        // No devices participating in search
        return 0;
      }

      bool branch;

      if (id_bit != cmp_id_bit) {
        // only chose one branch, the other one doesn't have any devices.
        branch = id_bit;
      } else {
        // there are devices with both 0s and 1s at this bit
        if (bit_number < this->last_discrepancy_) {
          branch = (address & bit_mask) > 0;
        } else {
          branch = bit_number == this->last_discrepancy_;
        }

        if (!branch) {
          last_zero = bit_number;
        }
      }

      if (branch) {
        address |= bit_mask;
      } else {
        address &= ~bit_mask;
      }

      // choose/announce branch
      this->write_bit_(branch);
    }

    this->last_discrepancy_ = last_zero;
    if (this->last_discrepancy_ == 0) {
      // we're at root and have no choices left, so this was the last one.
      this->last_device_flag_ = true;
    }

    this->address_ = address;
    return address;
  }

 protected:
  bool reset_() {
    std::vector<Phase> phases;
    this->bus_->drive(Symbol{480, 0, 480, 1}, &phases);
    return !this->bus_->devices.empty();
  }
  void write8_(uint8_t value) {
    for (int bit = 0; bit < 8; bit++)
      this->write_bit_((value >> bit) & 1);
  }
  void write_bit_(bool bit) {
    std::vector<Phase> phases;
    this->bus_->drive(bit ? Symbol{6, 0, 64, 1} : Symbol{60, 0, 10, 1}, &phases);
  }
  bool read_bit_() {
    std::vector<Phase> phases;
    this->bus_->drive(Symbol{5, 0, 58, 1}, &phases);
    return this->bus_->last_slot_read();
  }

  SimBus *bus_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t address_{0};
};

std::vector<uint64_t> reference_order(const std::vector<uint64_t> &roms) {
  SimBus bus;
  for (auto rom : roms)
    bus.devices.emplace_back(rom);
  return UpstreamSearch(&bus).search();
}

std::vector<uint64_t> random_roms(size_t count, uint32_t seed) {
  std::mt19937_64 rng(seed);
  std::set<uint64_t> roms;
  while (roms.size() < count)
    roms.insert((rng() & ~0xFFULL) | 0x28);
  std::vector<uint64_t> shuffled(roms.begin(), roms.end());
  std::shuffle(shuffled.begin(), shuffled.end(), rng);
  return shuffled;
}

/// Discrepancies low in the ROM, as in a batch of sensors.
std::vector<uint64_t> counting_low_roms(size_t count) {
  std::vector<uint64_t> roms;
  for (uint64_t i = 0; i < count; i++)
    roms.push_back(0x28 | (i << 8));
  return roms;
}

/// Discrepancies only in the last bits, the deepest branches.
std::vector<uint64_t> counting_high_roms(size_t count) {
  std::vector<uint64_t> roms;
  for (uint64_t i = 0; i < count; i++)
    roms.push_back(0x28 | (i << 58));
  return roms;
}

/// Pairs that differ in exactly one bit, at every position.
std::vector<uint64_t> single_bit_pairs(size_t count) {
  std::vector<uint64_t> roms;
  for (size_t i = 0; roms.size() < count; i++) {
    const uint64_t base = 0x0A5A5A5A5A5A5A28ULL ^ (uint64_t(i) << 58);
    roms.push_back(base);
    if (roms.size() < count)
      roms.push_back(base ^ (1ULL << (8 + (i * 5) % 50)));
  }
  return roms;
}

void expect_search_matches_upstream(const std::vector<uint64_t> &roms, size_t mem_words) {
  const auto expected = reference_order(roms);
  ASSERT_EQ(expected.size(), roms.size()) << "the reference itself must find all devices";

  Rig rig(mem_words);
  for (auto rom : roms)
    rig.port.bus.devices.emplace_back(rom);
  EXPECT_EQ(search_all(rig), expected);
  // Again from the top
  EXPECT_EQ(search_all(rig), expected);
}

}  // namespace

// ---- reset_blocking: -1/0/1 ------------------------------------------------------------------

TEST(RmtOneWireFacade, ResetPresent) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  EXPECT_EQ(rig.port.summary(), "RESET");
}

TEST(RmtOneWireFacade, ResetNoDevice) {
  Rig rig;
  EXPECT_EQ(rig.engine.reset_blocking(), 0);
}

TEST(RmtOneWireFacade, ResetBusLow) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.port.stuck_low = true;
  EXPECT_EQ(rig.engine.reset_blocking(), -1);
}

TEST(RmtOneWireFacade, ResetBusLowDuringThePulse) {
  for (const Symbol &held : {Symbol{600, 0, 0, 0}, Symbol{0, 0, STALE_DURATION1, 1}, Symbol{0, 0, 0, 1}}) {
    Rig rig;
    rig.port.capture_mutator = [&](size_t, std::vector<Symbol> &capture) { capture = {held}; };
    EXPECT_EQ(rig.engine.reset_blocking(), -1) << held.duration0 << "/" << held.duration1;
  }
}

TEST(RmtOneWireFacade, ResetCaptureErrorIsFailure) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.port.capture_mutator = [](size_t, std::vector<Symbol> &capture) { capture.clear(); };
  EXPECT_EQ(rig.engine.reset_blocking(), -1);
}

TEST(RmtOneWireFacade, ResetTimeoutIsFailure) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.port.drop_rx_at = 0;
  EXPECT_EQ(rig.engine.reset_blocking(), -1);
  EXPECT_EQ(rig.port.abort_count, 1);
  EXPECT_LE(rig.port.now_us(), 110 * MS);
}

TEST(RmtOneWireFacade, ResetPortFailureIsFailure) {
  {
    Rig rig;
    rig.port.bus.devices.emplace_back(ROM_A);
    rig.port.fail_arm_at = 0;
    EXPECT_EQ(rig.engine.reset_blocking(), -1);
    EXPECT_EQ(rig.port.abort_count, 0);
    EXPECT_EQ(rig.engine.reset_blocking(), 1) << "not permanent";
  }
  {
    Rig rig;
    rig.port.bus.devices.emplace_back(ROM_A);
    rig.port.fail_tx_at = 0;
    EXPECT_EQ(rig.engine.reset_blocking(), -1);
    EXPECT_EQ(rig.port.abort_count, 1) << "RX was armed";
    EXPECT_EQ(rig.engine.reset_blocking(), 1) << "not permanent";
  }
}

TEST(RmtOneWireFacade, ResetFailedStartWhoseAbortFailsIsPermanent) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.port.fail_tx_at = 0;
  rig.port.fail_abort = true;
  EXPECT_EQ(rig.engine.reset_blocking(), -1);
  const int transmits = rig.port.tx_count;
  EXPECT_EQ(rig.engine.reset_blocking(), -1);
  EXPECT_EQ(rig.port.tx_count, transmits) << "no RMT call on a failed bus";
  EXPECT_EQ(rig.port.abort_count, 1);
}

// ---- write_blocking ---------------------------------------------------------------------------

TEST(RmtOneWireFacade, WriteArrivesAtTheDevice) {
  Rig rig;
  auto &device = rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{}, 200);
  const auto data = pattern_bytes(100);
  select_blocking(rig);
  EXPECT_TRUE(rig.engine.write_blocking(data.data(), data.size() * 8));
  EXPECT_EQ(device.received(), data);
}

TEST(RmtOneWireFacade, WriteTimeoutIsFalseAndRecovers) {
  Rig rig;
  auto &device = rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{}, 200);
  const auto data = pattern_bytes(4);
  rig.port.drop_tx_at = 0;
  EXPECT_FALSE(rig.engine.write_blocking(data.data(), 32));
  EXPECT_EQ(rig.port.abort_count, 1);
  select_blocking(rig);
  EXPECT_TRUE(rig.engine.write_blocking(data.data(), 32));
  EXPECT_EQ(device.received(), data);
}

TEST(RmtOneWireFacade, WritePortFailureIsFalse) {
  Rig rig;
  const auto data = pattern_bytes(40);
  rig.port.fail_tx_at = 1;  // the second section
  EXPECT_FALSE(rig.engine.write_blocking(data.data(), 320));
  EXPECT_EQ(rig.port.tx_count, 2) << "stops at the failure";
  EXPECT_EQ(rig.port.abort_count, 0) << "no RX armed in a WRITE";
}

// ---- read_blocking: all bits 1 on failure -----------------------------------------------------

TEST(RmtOneWireFacade, ReadReturnsTheBits) {
  for (size_t mem_words : {size_t{48}, size_t{64}}) {
    for (size_t bytes : {size_t{1}, size_t{8}, size_t{9}, size_t{100}}) {
      Rig rig(mem_words);
      const auto payload = pattern_bytes(bytes);
      rig.port.bus.devices.emplace_back(ROM_A, payload);
      select_blocking(rig);
      std::vector<uint8_t> data(bytes, 0);
      EXPECT_TRUE(rig.engine.read_blocking(data.data(), bytes * 8));
      EXPECT_EQ(data, payload);
    }
  }
}

TEST(RmtOneWireFacade, ReadOfAQuietBusIsAllOnes) {
  Rig rig;
  std::vector<uint8_t> data(9, 0);
  EXPECT_TRUE(rig.engine.read_blocking(data.data(), 72));
  EXPECT_EQ(data, std::vector<uint8_t>(9, 0xFF));
}

namespace {

enum class ReadFailure : uint8_t { CAPTURE, TIMEOUT_RX, TIMEOUT_TX, ARM, TX };

struct ReadFailureCase {
  const char *name;
  ReadFailure failure;
  int section;  ///< Which read section of the call fails
};

/// Bits of a device that only sends zeros: leftovers of a decoded section stay visible. The buffer
/// is larger than the request and filled with a pattern: only the requested bits may become 1.
void run_read_failure(const ReadFailureCase &param, size_t bits) {
  Rig rig;
  const std::vector<uint8_t> zeros(bits / 8 + 1, 0x00);
  rig.port.bus.devices.emplace_back(ROM_A, zeros);
  select_blocking(rig);
  const size_t first = rig.port.sections.size();
  const int fail_at = static_cast<int>(first) + param.section;
  switch (param.failure) {
    case ReadFailure::CAPTURE:
      rig.port.capture_mutator = [=](size_t section, std::vector<Symbol> &capture) {
        if (static_cast<int>(section) == fail_at)
          capture.pop_back();
      };
      break;
    case ReadFailure::TIMEOUT_RX:
      rig.port.drop_rx_at = fail_at;
      break;
    case ReadFailure::TIMEOUT_TX:
      rig.port.drop_tx_at = fail_at;
      break;
    case ReadFailure::ARM:
      rig.port.fail_arm_at = rig.port.arm_count + param.section;
      break;
    case ReadFailure::TX:
      rig.port.fail_tx_at = rig.port.tx_count + param.section;
      break;
  }
  std::vector<uint8_t> data(bits / 8 + 2, 0x5A);
  std::vector<uint8_t> expected = data;
  for (size_t bit = 0; bit < bits; bit++)
    expected[bit / 8] |= 1u << (bit % 8);
  EXPECT_FALSE(rig.engine.read_blocking(data.data(), bits)) << param.name;
  EXPECT_EQ(data, expected) << param.name << " " << bits << " bits";
  EXPECT_TRUE(rig.engine.is_idle());
  const bool stopped_by_abort = param.failure == ReadFailure::TIMEOUT_RX || param.failure == ReadFailure::TIMEOUT_TX ||
                                param.failure == ReadFailure::TX;
  EXPECT_EQ(rig.port.abort_count, stopped_by_abort ? 1 : 0) << param.name;
}

}  // namespace

class RmtOneWireReadFailure : public ::testing::TestWithParam<ReadFailureCase> {};

INSTANTIATE_TEST_SUITE_P(Facade, RmtOneWireReadFailure,
                         ::testing::Values(ReadFailureCase{"Capture", ReadFailure::CAPTURE, 0},
                                           ReadFailureCase{"TimeoutRx", ReadFailure::TIMEOUT_RX, 0},
                                           ReadFailureCase{"TimeoutTx", ReadFailure::TIMEOUT_TX, 0},
                                           ReadFailureCase{"Arm", ReadFailure::ARM, 0},
                                           ReadFailureCase{"Tx", ReadFailure::TX, 0}),
                         [](const ::testing::TestParamInfo<ReadFailureCase> &info) { return info.param.name; });

TEST_P(RmtOneWireReadFailure, Read8AllOnes) { run_read_failure(GetParam(), 8); }

TEST_P(RmtOneWireReadFailure, Read64AllOnes) { run_read_failure(GetParam(), 64); }

TEST_P(RmtOneWireReadFailure, FailureInTheFirstOfSeveralSectionsAllOnes) { run_read_failure(GetParam(), 800); }

TEST_P(RmtOneWireReadFailure, FailureInALaterSectionOverwritesWhatWasDecoded) {
  ReadFailureCase later = GetParam();
  later.section = 2;
  run_read_failure(later, 800);
}

TEST_P(RmtOneWireReadFailure, PartialByteOnlyRequestedBitsBecomeOnes) {
  run_read_failure(GetParam(), 3);
  run_read_failure(GetParam(), 12);
}

TEST_P(RmtOneWireReadFailure, PartialByteInALaterSection) {
  ReadFailureCase later = GetParam();
  later.section = 1;
  run_read_failure(later, 100);  // 62 + 38 slots
}

TEST(RmtOneWireFacade, ReadWithTheLineAtGndFailsAtAnyLength) {
  // Also a single slot, where the count alone would pass with a wrong 0
  for (size_t bits : {size_t{1}, size_t{8}, size_t{64}}) {
    Rig rig;
    rig.port.stuck_low = true;
    std::vector<uint8_t> data(bits / 8 + 1, 0x00);
    std::vector<uint8_t> expected = data;
    for (size_t bit = 0; bit < bits; bit++)
      expected[bit / 8] |= 1u << (bit % 8);
    EXPECT_FALSE(rig.engine.read_blocking(data.data(), bits)) << bits;
    EXPECT_EQ(data, expected) << bits;
  }
}

// ---- Facade and FIFO --------------------------------------------------------------------------

TEST(RmtOneWireFacade, I7_FacadeDrainsTheFifoFirst) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A, pattern_bytes(9), 1);
  DoneLog log;
  Job running(&log, 1, ROM_A, {0xBE}, 9);
  Job queued(&log, 2, 0, {0xBE}, 3);
  ASSERT_TRUE(rig.engine.submit(&running.t));
  ASSERT_TRUE(rig.engine.submit(&queued.t));
  rig.engine.poll();  // the first one is on the wire now, the fifo is not empty

  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}, {2, TransactionStatus::OK}}))
      << "both transactions ended before the blocking reset returned";
  EXPECT_EQ(running.read_bytes, pattern_bytes(9));

  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), 3u);
  EXPECT_EQ(slots_to_string(frames[0]), slots_to_string(running.slots()));
  EXPECT_EQ(slots_to_string(frames[1]), slots_to_string(queued.slots()));
  EXPECT_TRUE(frames[2].empty()) << "the reset of the facade comes last";
  EXPECT_TRUE(rig.engine.is_idle());
  EXPECT_FALSE(rig.port.loop_active());
}

TEST(RmtOneWireFacade, I7_ReadAndWriteAlsoDrainFirst) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{0x12, 0x34}, 0);
  DoneLog log;
  Job first(&log, 1, 0, {}, 0);
  ASSERT_TRUE(rig.engine.submit(&first.t));
  uint8_t data[2]{};
  EXPECT_TRUE(rig.engine.read_blocking(data, 16));
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(data[0], 0x12);
  EXPECT_EQ(data[1], 0x34);
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:8 READ:16");

  Job second(&log, 2, 0, {}, 0);
  ASSERT_TRUE(rig.engine.submit(&second.t));
  const uint8_t byte = 0x00;
  EXPECT_TRUE(rig.engine.write_blocking(&byte, 8));
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}, {2, TransactionStatus::OK}}));
  EXPECT_EQ(rig.port.summary(3), "RESET WRITE:8 WRITE:8");
}

TEST(RmtOneWireFacade, I7_DrainIncludesWhatOnDoneSubmits) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  DoneLog log;
  Job first(&log, 1, 0, {}, 0);
  Job second(&log, 2, 0, {}, 0);
  first.then = [&](TransactionStatus) { EXPECT_TRUE(rig.engine.submit(&second.t)); };
  ASSERT_TRUE(rig.engine.submit(&first.t));
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}, {2, TransactionStatus::OK}}));
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:8 RESET WRITE:8 RESET");
}

TEST(RmtOneWireFacade, I7_DrainWithNullOnDone) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  DoneLog log;
  Job silent(&log, 1, 0, {}, 0);
  silent.t.on_done = nullptr;
  ASSERT_TRUE(rig.engine.submit(&silent.t));
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:8 RESET");
  EXPECT_TRUE(log.empty());
}

TEST(RmtOneWireFacade, I7_FailingDrainDoesNotStopThePrimitive) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.port.drop_tx_at = 0;
  DoneLog log;
  Job lost(&log, 1, 0, {}, 0);
  ASSERT_TRUE(rig.engine.submit(&lost.t));
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::TIMEOUT}}));
}

TEST(RmtOneWireFacade, BlockingSequenceStaysInOnePiece) {
  Rig rig;
  const auto payload = pattern_bytes(9);
  rig.port.bus.devices.emplace_back(ROM_A, payload, 1);
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SKIP_ROM, 8));
  const uint8_t read_scratchpad = 0xBE;
  ASSERT_TRUE(rig.engine.write_blocking(&read_scratchpad, 8));
  std::vector<uint8_t> data(9);
  ASSERT_TRUE(rig.engine.read_blocking(data.data(), 72));
  EXPECT_EQ(data, payload);
  const auto frames = rig.port.frames();
  EXPECT_EQ(frames.size(), 1u) << "no reset between the steps";
}

// ---- Sleeping, not spinning -------------------------------------------------------------------

TEST(RmtOneWireFacade, SleepsThroughWaitForEvent) {
  // SimPort delivers completions only from wait_for_event() here, a busy wait ends in sim_hang()
  Rig rig;
  const auto payload = pattern_bytes(9);
  rig.port.bus.devices.emplace_back(ROM_A, payload);
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  const int after_reset = rig.port.wait_calls;
  EXPECT_GT(after_reset, 0);
  std::vector<uint8_t> data(9);
  ASSERT_TRUE(rig.engine.write_blocking(&SKIP_ROM, 8));
  ASSERT_TRUE(rig.engine.read_blocking(data.data(), 72));
  EXPECT_EQ(data, payload);
  EXPECT_GE(rig.port.wait_calls, after_reset + 3) << "at least one sleep per section";
}

TEST(RmtOneWireFacade, SpuriousWakeupsChangeNothing) {
  // A notification left over from an earlier section wakes the task early
  Rig rig;
  const auto payload = pattern_bytes(9);
  rig.port.bus.devices.emplace_back(ROM_A, payload);
  rig.port.spurious_wakeups = true;
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SKIP_ROM, 8));
  std::vector<uint8_t> data(9);
  ASSERT_TRUE(rig.engine.read_blocking(data.data(), 72));
  EXPECT_EQ(data, payload);
  EXPECT_EQ(rig.port.abort_count, 0);
}

TEST(RmtOneWireFacade, SlowSectionsInsideTheirTimeoutSucceed) {
  // Each section takes 40 ms, the call as a whole far longer than one timeout
  Rig rig;
  const auto payload = pattern_bytes(9);
  rig.port.bus.devices.emplace_back(ROM_A, payload);
  rig.port.section_delay_us = 40 * MS;
  std::vector<uint8_t> data(9);
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SKIP_ROM, 8));
  ASSERT_TRUE(rig.engine.read_blocking(data.data(), 72));
  EXPECT_EQ(data, payload);
  EXPECT_EQ(rig.port.abort_count, 0);
}

// ---- Failed bus: immediately, without the RMT API ----------------------------------------------

TEST(RmtOneWireFacade, FailedBusReturnsImmediately) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.engine.set_failed();
  const uint8_t byte = 0x00;
  uint8_t data[8];
  std::fill(std::begin(data), std::end(data), 0x00);

  EXPECT_EQ(rig.engine.reset_blocking(), -1);
  EXPECT_FALSE(rig.engine.write_blocking(&byte, 8));
  EXPECT_FALSE(rig.engine.read_blocking(data, 64));
  rig.engine.reset_search();
  EXPECT_EQ(rig.engine.search_next(), 0u);

  for (auto value : data)
    EXPECT_EQ(value, 0xFF);
  EXPECT_EQ(rig.port.arm_count, 0);
  EXPECT_EQ(rig.port.tx_count, 0);
  EXPECT_EQ(rig.port.abort_count, 0);
  EXPECT_EQ(rig.port.wait_calls, 0);
  EXPECT_EQ(rig.port.now_us(), 0u);
}

TEST(RmtOneWireFacade, FailedBusSetsOnlyTheRequestedBits) {
  Rig rig;
  rig.engine.set_failed();
  uint8_t data[2] = {0x00, 0x00};
  EXPECT_FALSE(rig.engine.read_blocking(data, 3));
  EXPECT_EQ(data[0], 0x07);
  EXPECT_EQ(data[1], 0x00);
}

TEST(RmtOneWireFacade, FailedAbortMakesTheFacadeFail) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.port.drop_rx_at = 0;
  rig.port.fail_abort = true;
  EXPECT_EQ(rig.engine.reset_blocking(), -1);
  const int transmits = rig.port.tx_count;
  EXPECT_EQ(rig.engine.reset_blocking(), -1);
  EXPECT_EQ(rig.port.tx_count, transmits);
  EXPECT_EQ(rig.port.abort_count, 1);
}

// ---- I8: search ------------------------------------------------------------------------------

class RmtOneWireSearch : public ::testing::TestWithParam<size_t> {};

INSTANTIATE_TEST_SUITE_P(W, RmtOneWireSearch, ::testing::Values(size_t{48}, size_t{64}),
                         [](const ::testing::TestParamInfo<size_t> &info) {
                           return std::to_string(info.param);  // NOLINT
                         });

TEST_P(RmtOneWireSearch, I8_RandomRomsZeroToFortyDevices) {
  for (size_t count = 0; count <= 40; count++) {
    SCOPED_TRACE("devices " + std::to_string(count));  // NOLINT
    expect_search_matches_upstream(random_roms(count, static_cast<uint32_t>(count) + 100), GetParam());
  }
}

TEST_P(RmtOneWireSearch, I8_CountingRomsLowDiscrepancies) {
  for (size_t count = 1; count <= 40; count += 3) {
    SCOPED_TRACE("devices " + std::to_string(count));  // NOLINT
    expect_search_matches_upstream(counting_low_roms(count), GetParam());
  }
}

TEST_P(RmtOneWireSearch, I8_CountingRomsHighDiscrepancies) {
  for (size_t count = 1; count <= 40; count += 3) {
    SCOPED_TRACE("devices " + std::to_string(count));  // NOLINT
    expect_search_matches_upstream(counting_high_roms(count), GetParam());
  }
}

TEST_P(RmtOneWireSearch, I8_PairsThatDifferInOneBit) {
  for (size_t count = 2; count <= 40; count += 2) {
    SCOPED_TRACE("devices " + std::to_string(count));  // NOLINT
    expect_search_matches_upstream(single_bit_pairs(count), GetParam());
  }
}

TEST_P(RmtOneWireSearch, I8_DiscrepancyInTheVeryLastBit) {
  expect_search_matches_upstream({0x0123456789ABCD28ULL, 0x8123456789ABCD28ULL}, GetParam());
}

TEST(RmtOneWireSearchContract, I8_OneDeviceAndAllBits) {
  expect_search_matches_upstream({0xFFFFFFFFFFFFFFFFULL}, 64);
  expect_search_matches_upstream({0x0000000000000001ULL}, 64);
  expect_search_matches_upstream({0xAAAAAAAAAAAAAAAAULL}, 64);
  expect_search_matches_upstream({0x5555555555555555ULL}, 64);
}

TEST(RmtOneWireSearchContract, I8_ZeroAfterTheLastDeviceWithoutTouchingTheBus) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.engine.reset_search();
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
  ASSERT_EQ(rig.engine.search_next(), ROM_A);
  const int transmits = rig.port.tx_count;
  EXPECT_EQ(rig.engine.search_next(), 0u);
  EXPECT_EQ(rig.engine.search_next(), 0u);
  EXPECT_EQ(rig.port.tx_count, transmits);
}

TEST(RmtOneWireSearchContract, I8_ZeroWhenNoDeviceTakesPart) {
  // Both read slots come back as 1: search_int() stops after these two without a branch
  Rig rig;
  rig.engine.reset_search();
  EXPECT_EQ(rig.engine.search_next(), 0u);
  size_t slots = 0;
  for (const auto &section : rig.port.sections)
    slots += section.tx.size();
  EXPECT_EQ(slots, 2u);
}

TEST(RmtOneWireSearchContract, I8_ResetSearchStartsOver) {
  Rig rig;
  const auto roms = random_roms(5, 7);
  for (auto rom : roms)
    rig.port.bus.devices.emplace_back(rom);
  const auto expected = reference_order(roms);

  rig.engine.reset_search();
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
  ASSERT_EQ(rig.engine.search_next(), expected[0]);
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
  ASSERT_EQ(rig.engine.search_next(), expected[1]);

  EXPECT_EQ(search_all(rig), expected);
}

TEST(RmtOneWireSearchContract, I4_SearchIsTwoThenThreeSlotsThenClosingWrite) {
  // Section 4.2: SEARCH with 2 slots, 63 x SEARCH with 3 slots, closing WRITE with the last branch.
  for (size_t mem_words : {size_t{48}, size_t{64}}) {
    Rig rig(mem_words);
    rig.port.bus.devices.emplace_back(ROM_A);
    rig.engine.reset_search();
    ASSERT_EQ(rig.engine.reset_blocking(), 1);
    ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
    const size_t first = rig.port.sections.size();
    ASSERT_EQ(rig.engine.search_next(), ROM_A);

    ASSERT_EQ(rig.port.sections.size(), first + 65);
    std::string expected = "READ:2";
    for (int i = 0; i < 63; i++)
      expected += " READ:3";
    expected += " WRITE:1";
    EXPECT_EQ(rig.port.summary(first), expected);
    const size_t last = rig.port.sections.size() - 1;
    EXPECT_FALSE(rig.port.sections[last].armed) << "the closing WRITE has no RX";
    EXPECT_EQ(rig.port.calls.back(), PortCall::TX);
    EXPECT_EQ(rig.port.calls[rig.port.calls.size() - 2], PortCall::TX) << "no arm_rx() before the closing WRITE";

    size_t slots = 0;
    for (size_t i = first; i < rig.port.sections.size(); i++) {
      const auto &section = rig.port.sections[i];
      EXPECT_LE(section.tx.size(), 3u);
      slots += section.tx.size();
    }
    EXPECT_EQ(slots, 192u);

    EXPECT_EQ(rig.port.sections[first].tx.size(), 2u) << "no previous branch bit in the first";
    EXPECT_TRUE(rig.port.sections[first].armed);
    for (size_t i = first + 1; i < first + 64; i++) {
      EXPECT_EQ(rig.port.sections[i].tx.size(), 3u) << "section " << i - first;
      EXPECT_TRUE(rig.port.sections[i].armed) << "section " << i - first;
    }
  }
}

TEST(RmtOneWireSearchContract, I8_SearchOnTheWireIsBitComplementBranch) {
  Rig rig;
  rig.port.bus.devices.emplace_back(0xF00000000000AB28ULL);
  rig.engine.reset_search();
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
  const size_t first = rig.port.sections.size();
  ASSERT_EQ(rig.engine.search_next(), 0xF00000000000AB28ULL);

  std::vector<Slot> slots;
  for (size_t i = first; i < rig.port.sections.size(); i++) {
    for (const auto &symbol : rig.port.sections[i].tx)
      slots.push_back(classify_slot(symbol));
  }
  std::vector<Slot> expected;
  for (int bit = 0; bit < 64; bit++) {
    expected.push_back(Slot::READ);
    expected.push_back(Slot::READ);
    expected.push_back((0xF00000000000AB28ULL >> bit) & 1 ? Slot::WRITE_1 : Slot::WRITE_0);
  }
  EXPECT_EQ(slots_to_string(slots), slots_to_string(expected));
}

namespace {

enum class SearchFailure : uint8_t { CAPTURE, ENDS_ON_LOW, LOW_OF_A_SLOT, TIMEOUT, ARM, TX };

void run_search_failure(SearchFailure failure) {
  Rig rig;
  const auto roms = random_roms(3, 21);
  for (auto rom : roms)
    rig.port.bus.devices.emplace_back(rom);
  const auto expected = reference_order(roms);

  rig.engine.reset_search();
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
  const int fail_at = static_cast<int>(rig.port.sections.size()) + 20;
  switch (failure) {
    case SearchFailure::CAPTURE:
      rig.port.capture_mutator = [=](size_t section, std::vector<Symbol> &capture) {
        if (static_cast<int>(section) == fail_at)
          capture.pop_back();
      };
      break;
    case SearchFailure::ENDS_ON_LOW:
      rig.port.capture_mutator = [=](size_t section, std::vector<Symbol> &capture) {
        if (static_cast<int>(section) == fail_at)
          capture.back() = Symbol{0, 0, STALE_DURATION1, 1};
      };
      break;
    case SearchFailure::LOW_OF_A_SLOT:
      rig.port.capture_mutator = [=](size_t section, std::vector<Symbol> &capture) {
        if (static_cast<int>(section) == fail_at)
          capture[1].duration0 = 110;
      };
      break;
    case SearchFailure::TIMEOUT:
      rig.port.drop_rx_at = fail_at;
      break;
    case SearchFailure::ARM:
      rig.port.fail_arm_at = rig.port.arm_count + 20;
      break;
    case SearchFailure::TX:
      rig.port.fail_tx_at = rig.port.tx_count + 20;
      break;
  }
  EXPECT_EQ(rig.engine.search_next(), 0u);
  EXPECT_TRUE(rig.engine.is_idle());

  rig.port.capture_mutator = nullptr;
  rig.port.drop_rx_at = SimPort::NEVER;
  rig.port.fail_arm_at = SimPort::NEVER;
  rig.port.fail_tx_at = SimPort::NEVER;
  EXPECT_EQ(search_all(rig), expected) << "after reset_search() the search works again";
}

}  // namespace

TEST(RmtOneWireSearchContract, I8_ZeroOnCaptureErrorThenRecovers) { run_search_failure(SearchFailure::CAPTURE); }
TEST(RmtOneWireSearchContract, I8_ZeroOnCaptureEndingLowThenRecovers) {
  run_search_failure(SearchFailure::ENDS_ON_LOW);
}
TEST(RmtOneWireSearchContract, I8_ZeroOnLowOfASlotThenRecovers) { run_search_failure(SearchFailure::LOW_OF_A_SLOT); }
TEST(RmtOneWireSearchContract, I8_ZeroOnTimeoutThenRecovers) { run_search_failure(SearchFailure::TIMEOUT); }
TEST(RmtOneWireSearchContract, I8_ZeroOnArmFailureThenRecovers) { run_search_failure(SearchFailure::ARM); }
TEST(RmtOneWireSearchContract, I8_ZeroOnTxFailureThenRecovers) { run_search_failure(SearchFailure::TX); }

TEST(RmtOneWireSearchContract, I8_ZeroWhenTheClosingWriteFails) {
  for (bool timeout : {false, true}) {
    Rig rig;
    rig.port.bus.devices.emplace_back(ROM_A);
    rig.engine.reset_search();
    ASSERT_EQ(rig.engine.reset_blocking(), 1);
    ASSERT_TRUE(rig.engine.write_blocking(&SEARCH_ROM, 8));
    const int closing = static_cast<int>(rig.port.sections.size()) + 64;
    if (timeout) {
      rig.port.drop_tx_at = closing;
    } else {
      rig.port.fail_tx_at = rig.port.tx_count + 64;
    }
    EXPECT_EQ(rig.engine.search_next(), 0u) << "timeout " << timeout;
    EXPECT_EQ(rig.port.abort_count, timeout ? 1 : 0) << "no RX armed in the closing WRITE";
  }
}

TEST(RmtOneWireSearchContract, I8_SearchDrainsTheFifoFirst) {
  Rig rig;
  rig.port.bus.devices.emplace_back(ROM_A);
  DoneLog log;
  Job job(&log, 1, 0, {}, 0);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.engine.reset_search();
  // On the wire this is a nonsense sequence, only the order counts
  rig.engine.search_next();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  ASSERT_GE(rig.port.sections.size(), 3u);
  EXPECT_TRUE(rig.port.sections[0].is_reset());
  EXPECT_EQ(rig.port.sections[1].tx.size(), 8u);
  EXPECT_EQ(rig.port.sections[2].tx.size(), 2u) << "the search starts behind the transaction";
}

}  // namespace esphome::gpio::rmt_one_wire::testing
