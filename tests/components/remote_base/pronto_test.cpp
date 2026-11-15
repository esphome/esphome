#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/logger/logger.h"
#include "esphome/components/remote_base/pronto_protocol.h"

// Needs AddressSanitizer's malloc hooks
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define PRONTO_TEST_ASAN
extern "C" int __sanitizer_install_malloc_and_free_hooks(void (*malloc_hook)(const volatile void *, size_t),
                                                         void (*free_hook)(const volatile void *));
#endif

namespace esphome::remote_base::testing {

// Output of the string-building decode this replaced, for the same timings
static constexpr const char *SHORT_TEXT = "0000 006D 0005 0000 0159 00AE 0015 0016 0015 0042 0015 0603 0000 0001 ";
static constexpr size_t LOG_CHUNK_SIZE = 230;

static const std::vector<int32_t> SHORT_TIMINGS{9000, -4500, 560, -560, 560, -1690, 560, -40000, 10, -5};

// 124 numbers: three log chunks
static std::vector<int32_t> long_timings() {
  std::vector<int32_t> timings;
  timings.reserve(120);
  for (int i = 0; i < 120; i++)
    timings.push_back(i % 2 == 0 ? 300 + i * 7 : -(200 + i * 11));
  return timings;
}

TEST(ProntoProtocol, DecodeText) {
  RemoteReceiveData src(SHORT_TIMINGS, 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = ProntoProtocol().decode(src);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->data, SHORT_TEXT);
  EXPECT_EQ(decoded->delta, -1);
}

TEST(ProntoProtocol, WriteNumbersInPiecesMatchesDecode) {
  const auto timings = long_timings();
  RemoteReceiveData src(timings, 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = ProntoProtocol().decode(src);
  ASSERT_TRUE(decoded.has_value());
  const std::string &whole = decoded->data;
  const size_t total = ProntoProtocol::number_count(timings);
  // One number at a time, and the dumper's 46 numbers per log line
  for (size_t piece : {size_t{1}, LOG_CHUNK_SIZE / ProntoProtocol::TEXT_NUMBER_LENGTH}) {
    std::string joined;
    char buf[LOG_CHUNK_SIZE];
    for (size_t first = 0; first < total; first += piece) {
      const size_t count = std::min(piece, total - first);
      ProntoProtocol::write_numbers(timings, first, count, buf);
      joined.append(buf, count * ProntoProtocol::TEXT_NUMBER_LENGTH);
    }
    EXPECT_EQ(joined, whole) << "piece " << piece;
  }
}

TEST(ProntoDumper, LogsTheTextInChunks) {
  const auto timings = long_timings();
  RemoteReceiveData src(timings, 25, TOLERANCE_MODE_PERCENTAGE);
  const auto decoded = ProntoProtocol().decode(src);
  ASSERT_TRUE(decoded.has_value());
  const std::string &whole = decoded->data;
  // The test logger only prints while it has a baud rate
  const uint32_t baud_rate = logger::global_logger->get_baud_rate();
  logger::global_logger->set_baud_rate(115200);
  ::testing::internal::CaptureStdout();
  EXPECT_TRUE(ProntoDumper().dump(src));
  const std::string log = ::testing::internal::GetCapturedStdout();
  logger::global_logger->set_baud_rate(baud_rate);

  ASSERT_NE(log.find("Received Pronto: data="), std::string::npos) << log;
  // Lines after the header read "[time]<colour>[I][remote.pronto:NNN]: <chunk><colour reset>"
  std::string joined;
  size_t lines = 0;
  for (size_t pos = log.find('\n'); (pos = log.find("]: ", pos)) != std::string::npos; lines++) {
    pos += 3;
    const size_t end = log.find_first_of("\033\n", pos);
    EXPECT_LE(end - pos, LOG_CHUNK_SIZE);
    joined.append(log, pos, end - pos);
  }
  EXPECT_GE(lines, 2u);
  EXPECT_EQ(joined, whole) << log;
}

TEST(ProntoDumper, DumpDoesNotAllocate) {
#ifdef PRONTO_TEST_ASAN
  static std::atomic<size_t> allocations{0};
  static const int hooks_installed = __sanitizer_install_malloc_and_free_hooks(
      [](const volatile void *, size_t) { allocations.fetch_add(1, std::memory_order_relaxed); },
      [](const volatile void *) {});
  ASSERT_NE(hooks_installed, 0);
  const auto timings = long_timings();
  RemoteReceiveData src(timings, 25, TOLERANCE_MODE_PERCENTAGE);
  ProntoDumper dumper;

  const size_t before = allocations.load(std::memory_order_relaxed);
  const bool dumped = dumper.dump(src);
  const size_t after = allocations.load(std::memory_order_relaxed);

  EXPECT_TRUE(dumped);
  EXPECT_EQ(after - before, 0u);
#else
  GTEST_SKIP() << "Allocation counting requires AddressSanitizer";
#endif
}

}  // namespace esphome::remote_base::testing
