#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

#include "esphome/components/modbus/modbus.h"

// The allocation counters rely on AddressSanitizer's malloc hooks. The cpp_unit_test harness always
// builds with ASan, so this is exercised in CI; the fallback only applies to out-of-harness builds.
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define HEAP_PROBE_HAS_ASAN
#endif

#ifdef HEAP_PROBE_HAS_ASAN

// Allocation counters fed by ASan's malloc hooks; sampled tightly around the calls under test.
static std::atomic<size_t> g_alloc_count{0};
static std::atomic<size_t> g_alloc_bytes{0};

static void malloc_hook(const volatile void *, size_t size) {
  g_alloc_count++;
  g_alloc_bytes += size;
}
static void free_hook(const volatile void *) {}

extern "C" int __sanitizer_install_malloc_and_free_hooks(void (*malloc_hook)(const volatile void *, size_t),
                                                         void (*free_hook)(const volatile void *));

[[maybe_unused]] static const int g_hooks_installed = __sanitizer_install_malloc_and_free_hooks(malloc_hook, free_hook);

namespace esphome::modbus::testing {

namespace {

// A UART the test can inject received bytes into; sent bytes are discarded.
class InjectableUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override {
    if (this->rx_.empty())
      return false;
    *data = this->rx_.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx_.size())
      return false;
    memcpy(data, this->rx_.data(), len);
    this->rx_.erase(this->rx_.begin(), this->rx_.begin() + len);
    return true;
  }
  size_t available() override { return this->rx_.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

  void inject_frame(uint8_t address, std::span<const uint8_t> pdu) {
    // Wire frame: address + PDU + CRC16(low, high)
    size_t start = this->rx_.size();
    this->rx_.push_back(address);
    this->rx_.insert(this->rx_.end(), pdu.begin(), pdu.end());
    uint16_t crc = crc16(this->rx_.data() + start, this->rx_.size() - start);
    this->rx_.push_back(crc & 0xFF);
    this->rx_.push_back(crc >> 8);
  }

 private:
  std::vector<uint8_t> rx_;
};

class NullDevice : public ModbusClientDevice {
 public:
  using ModbusClientDevice::ModbusClientDevice;
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override {
    this->responses++;
  }
  int responses{0};
};

struct Sample {
  size_t count;
  size_t bytes;
};

template<typename F> Sample sample(F &&f) {
  size_t c0 = g_alloc_count.load(), b0 = g_alloc_bytes.load();
  f();
  return {g_alloc_count.load() - c0, g_alloc_bytes.load() - b0};
}

}  // namespace

// Typical frames (reads and single-register/coil writes are exactly address + 5-byte PDU + CRC = 8
// bytes) fit the SmallInlineBuffer and are built with zero heap allocations; only larger frames spill
// to a single allocation.
TEST(HeapProbe, TypicalFrameConstructionIsAllocationFree) {
  const uint8_t read_pdu[] = {0x03, 0x01, 0x00, 0x00, 0x02};  // 5 bytes -> 8-byte frame, inline
  Sample typical = sample([&] {
    ModbusFrame frame(0x02, read_pdu, sizeof(read_pdu));
    (void) frame;
  });
  printf("HEAPPROBE frame_typical count=%zu bytes=%zu\n", typical.count, typical.bytes);
  EXPECT_EQ(typical.count, 0u);

  uint8_t large_pdu[250] = {0x10};  // multi-register write -> 253-byte frame, spills once
  Sample large = sample([&] {
    ModbusFrame frame(0x02, large_pdu, sizeof(large_pdu));
    (void) frame;
  });
  printf("HEAPPROBE frame_large count=%zu bytes=%zu\n", large.count, large.bytes);
  EXPECT_EQ(large.count, 1u);
}

// Queueing typical commands is allocation-free within the deque's first block: the frame fits the
// inline buffer, every entry is a plain append (ordering lives in selection, not storage), and the
// first block is already allocated when the hub is constructed. A 512-byte deque block holds
// 512 / sizeof(ModbusDeviceCommand) entries (16 on the 64-bit host); a deeper queue allocates more.
TEST(HeapProbe, QueueingTypicalCommandsIsAllocationFree) {
  ModbusClientHub hub;
  ModbusClientDevice device(&hub, 0x02);

  StaticVector<uint8_t, MAX_PDU_SIZE> req;
  const uint8_t read_pdu[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  req.assign(read_pdu, read_pdu + sizeof(read_pdu));

  constexpr int n = 12;
  static_assert(n * sizeof(ModbusDeviceCommand) < 512, "keep n within one deque block so the probe stays meaningful");
  size_t total = 0;
  for (int i = 0; i != n; i++) {
    req[2] = static_cast<uint8_t>(i);  // distinct start addresses: identical frames would dedup, not enqueue
    total += sample([&] { device.queue_pdu(req); }).count;
  }
  printf("HEAPPROBE queue_%d_typical_commands total_allocs=%zu\n", n, total);
  EXPECT_EQ(total, 0u);
}

// A WRITE arriving behind queued reads is a plain append too - the old priority front-insert (and
// its possible front-block allocation) is gone; the write wins transmit SELECTION instead.
TEST(HeapProbe, WriteBehindQueuedReadsAppendsAllocationFree) {
  ModbusClientHub hub;
  ModbusClientDevice device(&hub, 0x02);

  StaticVector<uint8_t, MAX_PDU_SIZE> req;
  const uint8_t read_pdu[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  req.assign(read_pdu, read_pdu + sizeof(read_pdu));
  for (int i = 0; i != 3; i++) {
    req[2] = static_cast<uint8_t>(i);  // distinct start addresses: identical frames would dedup, not enqueue
    device.queue_pdu(req);
  }

  const uint8_t write_pdu[] = {0x06, 0x00, 0x10, 0xBE, 0xEF};
  Sample append = sample([&] { device.queue_pdu(write_pdu); });
  printf("HEAPPROBE write_append count=%zu bytes=%zu\n", append.count, append.bytes);
  EXPECT_EQ(append.count, 0u);
}

// End to end: bytes injected at the UART travel through receive, frame parsing, response matching and
// device dispatch. The first response may grow the hub's rx buffer once; after that warm-up, handling a
// response performs zero heap allocations all the way to the device callback.
TEST(HeapProbe, ResponseHandlingIsAllocationFreeAfterWarmup) {
  InjectableUART uart;
  uart.set_baud_rate(115200);  // tx timing math divides by the baud rate
  ModbusClientHub hub;
  hub.set_uart_parent(&uart);
  hub.setup();  // computes frame timing from the baud rate
  NullDevice device(&hub, 0x02);

  StaticVector<uint8_t, MAX_PDU_SIZE> req;
  const uint8_t read_pdu[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  req.assign(read_pdu, read_pdu + sizeof(read_pdu));

  // Largest possible read response first, so the rx buffer warm-up covers every later size.
  uint8_t large_resp[252] = {0x03, 250};
  const uint8_t small_resp[] = {0x03, 0x04, 0x00, 0x2A, 0x01, 0x00};

  auto round_trip = [&](std::span<const uint8_t> response_pdu) {
    device.queue_pdu(req);
    hub.loop();  // transmit; the tx queue is empty during the measured receive below
    uart.inject_frame(0x02, response_pdu);
    return sample([&] { hub.loop(); });  // receive + parse + match + dispatch
  };

  Sample warmup = round_trip(std::span<const uint8_t>(large_resp, sizeof(large_resp)));
  Sample steady_large = round_trip(std::span<const uint8_t>(large_resp, sizeof(large_resp)));
  Sample steady_small = round_trip(small_resp);

  printf("HEAPPROBE warmup count=%zu bytes=%zu\n", warmup.count, warmup.bytes);
  printf("HEAPPROBE steady_large count=%zu bytes=%zu\n", steady_large.count, steady_large.bytes);
  printf("HEAPPROBE steady_small count=%zu bytes=%zu\n", steady_small.count, steady_small.bytes);

  EXPECT_EQ(device.responses, 3);
  EXPECT_LE(warmup.count, 1u);  // at most the one-time rx buffer growth
  EXPECT_EQ(steady_large.count, 0u);
  EXPECT_EQ(steady_small.count, 0u);
}

}  // namespace esphome::modbus::testing

#else  // !HEAP_PROBE_HAS_ASAN

// Stub every ASan-gated test name, so the suite's test list is identical in every build configuration.
namespace esphome::modbus::testing {
TEST(HeapProbe, TypicalFrameConstructionIsAllocationFree) {
  GTEST_SKIP() << "allocation counting requires an AddressSanitizer build";
}
TEST(HeapProbe, QueueingTypicalCommandsIsAllocationFree) {
  GTEST_SKIP() << "allocation counting requires an AddressSanitizer build";
}
TEST(HeapProbe, WriteBehindQueuedReadsAppendsAllocationFree) {
  GTEST_SKIP() << "allocation counting requires an AddressSanitizer build";
}
TEST(HeapProbe, ResponseHandlingIsAllocationFreeAfterWarmup) {
  GTEST_SKIP() << "allocation counting requires an AddressSanitizer build";
}
}  // namespace esphome::modbus::testing

#endif  // HEAP_PROBE_HAS_ASAN
