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

// Queueing typical commands is fully allocation-free: the frame fits the inline buffer and the tx
// deque's first block is already allocated when the hub is constructed. (A queue deeper than one
// deque block - roughly a dozen commands - would allocate further blocks.)
TEST(HeapProbe, QueueingTypicalCommandsIsAllocationFree) {
  ModbusClientHub hub;
  ModbusClientDevice device(&hub, 0x02);

  StaticVector<uint8_t, MAX_PDU_SIZE> req;
  const uint8_t read_pdu[] = {0x03, 0x01, 0x00, 0x00, 0x02};
  req.assign(read_pdu, read_pdu + sizeof(read_pdu));

  constexpr int n = 12;
  size_t total = 0;
  for (int i = 0; i != n; i++) {
    total += sample([&] { device.send_pdu(req); }).count;
  }
  printf("HEAPPROBE queue_%d_typical_commands total_allocs=%zu\n", n, total);
  EXPECT_EQ(total, 0u);
}

}  // namespace esphome::modbus::testing

#else  // !HEAP_PROBE_HAS_ASAN

namespace esphome::modbus::testing {
TEST(HeapProbe, TypicalFrameConstructionIsAllocationFree) {
  GTEST_SKIP() << "allocation counting requires an AddressSanitizer build";
}
}  // namespace esphome::modbus::testing

#endif  // HEAP_PROBE_HAS_ASAN
