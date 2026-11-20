#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "../common.h"

namespace esphome::gpio::rmt_one_wire::testing {

namespace {

constexpr uint8_t READ_SCRATCHPAD = 0xBE;

std::vector<uint8_t> random_bytes(std::mt19937 &rng, size_t count) {
  std::vector<uint8_t> bytes(count);
  for (auto &byte : bytes)
    byte = static_cast<uint8_t>(rng());
  return bytes;
}

/// Every transmitted symbol of every section lies in the windows of section 3.
void expect_all_symbols_in_windows(const SimPort &port) {
  ASSERT_FALSE(port.sections.empty());
  EXPECT_TRUE(port.sections[0].is_reset());
  for (size_t i = 0; i < port.sections.size(); i++) {
    const auto &section = port.sections[i];
    for (const auto &symbol : section.tx) {
      EXPECT_EQ(symbol.level0, 0) << "section " << i;
      EXPECT_EQ(symbol.level1, 1) << "section " << i;
      if (section.is_reset()) {
        EXPECT_TRUE(WINDOW_RESET_LOW.contains(symbol.duration0)) << "reset low " << symbol.duration0;
        EXPECT_TRUE(WINDOW_RESET_HIGH.contains(symbol.duration1)) << "reset high " << symbol.duration1;
        continue;
      }
      EXPECT_NE(classify_slot(symbol), Slot::BAD) << "section " << i << " low " << symbol.duration0;
      EXPECT_TRUE(WINDOW_SLOT.contains(symbol.duration0 + symbol.duration1))
          << "section " << i << " slot " << symbol.duration0 + symbol.duration1;
    }
  }
}

void expect_sections_fit(const SimPort &port, size_t mem_words, const char *context) {
  for (size_t i = 0; i < port.sections.size(); i++) {
    const auto &section = port.sections[i];
    EXPECT_LE(section.tx.size(), mem_words - 1) << context << " section " << i;
    if (section.armed && !section.is_reset())
      EXPECT_LE(section.tx.size(), mem_words - 2) << context << " section " << i;
  }
}

}  // namespace

class RmtOneWireSections : public ::testing::TestWithParam<size_t> {};

INSTANTIATE_TEST_SUITE_P(W, RmtOneWireSections, ::testing::Values(size_t{48}, size_t{64}),
                         [](const ::testing::TestParamInfo<size_t> &info) {
                           return std::to_string(info.param);  // NOLINT
                         });

// ---- I4: the table of section 4.2 as an exact expectation ------------------------------------

TEST(RmtOneWireSectionTable, I4_ReadScratchpadW64) {
  Rig rig(64);
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9}, 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:63 WRITE:17 READ:62 READ:10");
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(job.read_bytes, (std::vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(RmtOneWireSectionTable, I4_ReadScratchpadW48) {
  Rig rig(48);
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9}, 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(rig.port.summary(), "RESET WRITE:47 WRITE:33 READ:46 READ:26");
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(job.read_bytes, (std::vector<uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

// ---- I4: all lengths -------------------------------------------------------------------------

namespace {

void check_one_length(Rig &rig, std::mt19937 &rng, uint64_t address, size_t write_len, size_t read_len) {
  const std::vector<uint8_t> payload = random_bytes(rng, read_len);
  rig.port.bus.devices.clear();
  rig.port.bus.devices.emplace_back(ROM_A, payload, write_len);
  rig.port.sections.clear();

  DoneLog log;
  Job job(&log, 1, address, random_bytes(rng, write_len), read_len);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();

  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::OK}})) << "write " << write_len << " read " << read_len;
  ASSERT_TRUE(rig.engine.is_idle());
  EXPECT_EQ(job.read_bytes, payload) << "write " << write_len << " read " << read_len;

  const std::string context = "write " + std::to_string(write_len) + " read " + std::to_string(read_len);  // NOLINT
  expect_sections_fit(rig.port, rig.mem_words(), context.c_str());

  const auto frames = rig.port.frames();
  ASSERT_EQ(frames.size(), 1u) << context;
  EXPECT_EQ(slots_to_string(frames[0]), slots_to_string(job.slots())) << context;
  ASSERT_FALSE(rig.port.sections.empty());
  EXPECT_TRUE(rig.port.sections[0].is_reset()) << context;
}

}  // namespace

TEST_P(RmtOneWireSections, I4_AllWriteLengthsSkipRom) {
  Rig rig(GetParam());
  std::mt19937 rng(11);
  for (size_t length = 0; length <= 255; length++)
    check_one_length(rig, rng, 0, length, 0);
}

TEST_P(RmtOneWireSections, I4_AllWriteLengthsMatchRom) {
  Rig rig(GetParam());
  std::mt19937 rng(12);
  for (size_t length = 0; length <= 255; length++)
    check_one_length(rig, rng, ROM_A, length, 0);
}

TEST_P(RmtOneWireSections, I4_AllReadLengthsSkipRom) {
  Rig rig(GetParam());
  std::mt19937 rng(13);
  for (size_t length = 0; length <= 255; length++)
    check_one_length(rig, rng, 0, 0, length);
}

TEST_P(RmtOneWireSections, I4_AllReadLengthsMatchRom) {
  Rig rig(GetParam());
  std::mt19937 rng(14);
  for (size_t length = 0; length <= 255; length++)
    check_one_length(rig, rng, ROM_A, 0, length);
}

TEST_P(RmtOneWireSections, I4_WriteAndReadCombined) {
  Rig rig(GetParam());
  std::mt19937 rng(15);
  for (size_t write_len = 0; write_len <= 255; write_len += 15) {
    for (size_t read_len = 0; read_len <= 255; read_len += 15) {
      check_one_length(rig, rng, ROM_A, write_len, read_len);
      check_one_length(rig, rng, 0, write_len, read_len);
    }
  }
}

TEST_P(RmtOneWireSections, I4_LengthsAroundTheBlockBoundary) {
  // The bytes around W-1 and W-2 slots, with the ROM selection in front
  Rig rig(GetParam());
  std::mt19937 rng(16);
  const size_t words_in_bytes = GetParam() / 8;
  for (size_t bytes = 0; bytes <= 3 * words_in_bytes + 1; bytes++) {
    check_one_length(rig, rng, 0, bytes, bytes);
    check_one_length(rig, rng, ROM_A, bytes, bytes);
  }
}

TEST_P(RmtOneWireSections, I4_FacadeSectionsFit) {
  Rig rig(GetParam());
  std::mt19937 rng(17);
  const auto data = random_bytes(rng, 255);
  std::vector<uint8_t> read(255);
  for (size_t bits : {size_t{1}, size_t{8}, size_t{46}, size_t{47}, size_t{48}, size_t{62}, size_t{63}, size_t{64},
                      size_t{65}, size_t{94}, size_t{126}, size_t{2040}}) {
    rig.port.sections.clear();
    EXPECT_TRUE(rig.engine.write_blocking(data.data(), bits)) << bits;
    size_t sent = 0;
    for (const auto &section : rig.port.sections) {
      EXPECT_FALSE(section.armed);
      for (const auto &symbol : section.tx) {
        const bool bit = (data[sent / 8] >> (sent % 8)) & 1;
        EXPECT_EQ(classify_slot(symbol), bit ? Slot::WRITE_1 : Slot::WRITE_0) << "slot " << sent;
        sent++;
      }
    }
    EXPECT_EQ(sent, bits);
    expect_sections_fit(rig.port, rig.mem_words(), "write_blocking");

    rig.port.sections.clear();
    EXPECT_TRUE(rig.engine.read_blocking(read.data(), bits)) << bits;
    sent = 0;
    for (const auto &section : rig.port.sections) {
      EXPECT_TRUE(section.armed);
      for (const auto &symbol : section.tx) {
        EXPECT_EQ(classify_slot(symbol), Slot::READ);
        sent++;
      }
    }
    EXPECT_EQ(sent, bits);
    expect_sections_fit(rig.port, rig.mem_words(), "read_blocking");
  }
}

// ---- I6: order of RX and TX, a section ends only with all of its completions ------------------

TEST_P(RmtOneWireSections, I6_RxIsArmedBeforeTxInEveryRxSection) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>(9, 0x3C), 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 9);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));

  // RESET, WRITE, WRITE, READ, READ
  const std::vector<PortCall> expected{PortCall::ARM, PortCall::TX, PortCall::TX,  PortCall::TX,
                                       PortCall::ARM, PortCall::TX, PortCall::ARM, PortCall::TX};
  EXPECT_EQ(rig.port.calls, expected);

  ASSERT_EQ(rig.port.sections.size(), 5u);
  EXPECT_TRUE(rig.port.sections[0].armed);
  EXPECT_FALSE(rig.port.sections[1].armed);
  EXPECT_FALSE(rig.port.sections[2].armed);
  EXPECT_TRUE(rig.port.sections[3].armed);
  EXPECT_TRUE(rig.port.sections[4].armed);
}

TEST_P(RmtOneWireSections, I6_FacadeArmsRxBeforeTx) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>(9, 0x3C));
  EXPECT_EQ(rig.engine.reset_blocking(), 1);
  const uint8_t skip = 0xCC;
  EXPECT_TRUE(rig.engine.write_blocking(&skip, 8));
  uint8_t data[9];
  EXPECT_TRUE(rig.engine.read_blocking(data, 72));
  const auto &calls = rig.port.calls;
  ASSERT_GE(calls.size(), 4u);
  EXPECT_EQ(calls[0], PortCall::ARM);
  EXPECT_EQ(calls[1], PortCall::TX);
  EXPECT_EQ(calls[2], PortCall::TX);
  EXPECT_EQ(calls[3], PortCall::ARM);
  for (size_t i = 3; i < calls.size(); i += 2) {
    ASSERT_LT(i + 1, calls.size());
    EXPECT_EQ(calls[i], PortCall::ARM);
    EXPECT_EQ(calls[i + 1], PortCall::TX);
  }
}

namespace {

/// Steps a transaction by hand. fire_first and fire_second deliver the completions of an RX
/// section, in between nothing may start and the transaction may not finish.
void step_through_sections(Rig &rig, DoneLog &log, const std::function<void()> &fire_first,
                           const std::function<void()> &fire_second) {
  rig.poll_times(3);
  size_t started = rig.port.sections.size();
  ASSERT_EQ(started, 1u) << "the first section must be running";
  for (int guard = 0; guard < 2000 && log.empty(); guard++) {
    ASSERT_EQ(rig.port.sections.size(), started);
    if (rig.port.sections.back().armed) {
      rig.poll_times(3);
      EXPECT_EQ(rig.port.sections.size(), started) << "started without any completion";
      fire_first();
      rig.poll_times(5);
      EXPECT_EQ(rig.port.sections.size(), started) << "started after only one completion";
      EXPECT_TRUE(log.empty()) << "finished after only one completion";
      fire_second();
    } else {
      rig.poll_times(3);
      EXPECT_EQ(rig.port.sections.size(), started) << "started without TX done";
      rig.port.fire_tx();
    }
    rig.poll_times(3);
    if (log.empty()) {
      started++;
      ASSERT_EQ(rig.port.sections.size(), started) << "no section running after the last completion";
    }
  }
  ASSERT_FALSE(log.empty());
  EXPECT_EQ(rig.port.sections.size(), started) << "started another section after the end";
}

}  // namespace

TEST_P(RmtOneWireSections, I6_NextStartOnlyAfterBothCompletionsTxFirst) {
  Rig rig(GetParam());
  rig.port.manual = true;
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>(30, 0x96), 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 30);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  step_through_sections(rig, log, [&] { rig.port.fire_tx(); }, [&] { rig.port.fire_rx(); });
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(job.read_bytes, std::vector<uint8_t>(30, 0x96));
}

TEST_P(RmtOneWireSections, I6_NextStartOnlyAfterBothCompletionsRxFirst) {
  Rig rig(GetParam());
  rig.port.manual = true;
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>(30, 0x96), 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 30);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  step_through_sections(rig, log, [&] { rig.port.fire_rx(); }, [&] { rig.port.fire_tx(); });
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(job.read_bytes, std::vector<uint8_t>(30, 0x96));
}

TEST_P(RmtOneWireSections, I6_AutomaticRxFirstOrderAlsoCompletes) {
  Rig rig(GetParam());
  rig.port.rx_first = true;
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>(30, 0x69), 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 30);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  EXPECT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  EXPECT_EQ(job.read_bytes, std::vector<uint8_t>(30, 0x69));
}

TEST_P(RmtOneWireSections, I6_WriteOnlyTransactionNeverArmsAfterReset) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A);
  DoneLog log;
  Job job(&log, 1, ROM_A, std::vector<uint8_t>(40, 0x44), 0);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  for (size_t i = 1; i < rig.port.sections.size(); i++)
    EXPECT_FALSE(rig.port.sections[i].armed) << "section " << i;
  EXPECT_EQ(rig.port.arm_count, 1);
}

// ---- I12: timing of what the engine really sends ---------------------------------------------

TEST_P(RmtOneWireSections, I12_TransactionSymbolsInWindows) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{0xAA, 0x00, 0xFF}, 2);
  DoneLog log;
  Job job(&log, 1, ROM_A, {0x00, 0xFF}, 3);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  expect_all_symbols_in_windows(rig.port);
}

TEST_P(RmtOneWireSections, I12_FacadeAndSearchSymbolsInWindows) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A);
  rig.port.bus.devices.emplace_back(ROM_B);
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  const uint8_t search = 0xF0;
  ASSERT_TRUE(rig.engine.write_blocking(&search, 8));
  rig.engine.reset_search();
  EXPECT_NE(rig.engine.search_next(), 0u);
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  const uint8_t skip = 0xCC;
  ASSERT_TRUE(rig.engine.write_blocking(&skip, 8));
  uint8_t data[2];
  ASSERT_TRUE(rig.engine.read_blocking(data, 16));
  ASSERT_GT(rig.port.sections.size(), 60u);
  expect_all_symbols_in_windows(rig.port);
}

// ---- I13: RX idle ----------------------------------------------------------------------------

namespace {

void expect_idle_values(const SimPort &port) {
  ASSERT_FALSE(port.sections.empty());
  int resets = 0;
  int others = 0;
  for (size_t i = 0; i < port.sections.size(); i++) {
    const auto &section = port.sections[i];
    if (!section.armed)
      continue;
    if (section.is_reset()) {
      resets++;
      EXPECT_GT(section.idle_us, section.tx[0].duration0) << "the capture would end inside the reset pulse";
      EXPECT_EQ(section.idle_us, 600u) << "section " << i;
    } else {
      others++;
      EXPECT_EQ(section.idle_us, 130u) << "section " << i;
    }
  }
  EXPECT_GT(resets, 0);
  EXPECT_GT(others, 0);
}

}  // namespace

TEST_P(RmtOneWireSections, I13_TransactionIdleValues) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>(30, 0x5A), 1);
  DoneLog log;
  Job job(&log, 1, ROM_A, {READ_SCRATCHPAD}, 30);
  ASSERT_TRUE(rig.engine.submit(&job.t));
  rig.pump();
  ASSERT_EQ(log, (DoneLog{{1, TransactionStatus::OK}}));
  expect_idle_values(rig.port);
}

TEST_P(RmtOneWireSections, I13_FacadeIdleValues) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A, std::vector<uint8_t>(9, 0x5A));
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  const uint8_t skip = 0xCC;
  ASSERT_TRUE(rig.engine.write_blocking(&skip, 8));
  uint8_t data[9];
  ASSERT_TRUE(rig.engine.read_blocking(data, 72));
  expect_idle_values(rig.port);
}

TEST_P(RmtOneWireSections, I13_SearchIdleValues) {
  Rig rig(GetParam());
  rig.port.bus.devices.emplace_back(ROM_A);
  ASSERT_EQ(rig.engine.reset_blocking(), 1);
  const uint8_t search = 0xF0;
  ASSERT_TRUE(rig.engine.write_blocking(&search, 8));
  rig.engine.reset_search();
  EXPECT_EQ(rig.engine.search_next(), ROM_A);
  expect_idle_values(rig.port);
}

TEST_P(RmtOneWireSections, I13_ResetCaptureSeesPresenceOfEveryTiming) {
  // Over the whole allowed range of tPDH and tPDL the capture must not end early
  Rig rig(GetParam());
  for (uint32_t wait : {15u, 30u, 60u}) {
    for (uint32_t low : {60u, 120u, 240u}) {
      rig.port.bus.devices.clear();
      auto &device = rig.port.bus.devices.emplace_back(ROM_A);
      device.presence_wait_us = wait;
      device.presence_low_us = low;
      const uint64_t before = rig.port.now_us();
      EXPECT_EQ(rig.engine.reset_blocking(), 1) << wait << "/" << low;
      // Section 4.3: 1100-1400 us from the start
      EXPECT_GE(rig.port.now_us() - before, 1000u) << wait << "/" << low;
    }
  }
}

}  // namespace esphome::gpio::rmt_one_wire::testing
