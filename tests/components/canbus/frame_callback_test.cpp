#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "esphome/components/canbus/canbus.h"

// Needs AddressSanitizer's malloc hooks
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define CANBUS_TEST_ASAN
extern "C" int __sanitizer_install_malloc_and_free_hooks(void (*malloc_hook)(const volatile void *, size_t),
                                                         void (*free_hook)(const volatile void *));
#endif

namespace esphome::canbus::testing {

// Receives queued frames
class QueuedCanbus : public Canbus {
 public:
  void queue(uint32_t can_id, bool extended, std::initializer_list<uint8_t> data) {
    CanFrame frame{};
    frame.can_id = can_id;
    frame.use_extended_id = extended;
    frame.can_data_length_code = static_cast<uint8_t>(data.size());
    std::copy(data.begin(), data.end(), frame.data);
    this->rx_.push_back(frame);
  }

 protected:
  bool setup_internal() override { return true; }
  Error send_message(CanFrame *frame) override { return ERROR_OK; }
  Error read_message(CanFrame *frame) override {
    if (this->rx_.empty())
      return ERROR_NOMSG;
    *frame = this->rx_.front();
    this->rx_.pop_front();
    return ERROR_OK;
  }

  std::deque<CanFrame> rx_;
};

TEST(CanbusFrameCallback, PassesEveryFrameAndItsPayload) {
  using Received = std::vector<std::pair<uint32_t, std::vector<uint8_t>>>;
  QueuedCanbus bus;
  Received received;
  bus.add_frame_callback([&](const CanFrame &frame) { received.emplace_back(frame.can_id, frame.payload()); });
  bus.setup();
  bus.queue(0x100, false, {0x01, 0x02});
  bus.queue(0x1ABCDE42, true, {});
  bus.queue(0x7FF, false, {1, 2, 3, 4, 5, 6, 7, 8});

  bus.loop();

  EXPECT_EQ(received, (Received{{0x100, {0x01, 0x02}}, {0x1ABCDE42, {}}, {0x7FF, {1, 2, 3, 4, 5, 6, 7, 8}}}));
}

TEST(CanbusFrameCallback, ReceivingFramesDoesNotAllocate) {
#ifdef CANBUS_TEST_ASAN
  static std::atomic<size_t> allocations{0};
  static const int hooks_installed = __sanitizer_install_malloc_and_free_hooks(
      [](const volatile void *, size_t) { allocations.fetch_add(1, std::memory_order_relaxed); },
      [](const volatile void *) {});
  ASSERT_NE(hooks_installed, 0);
  QueuedCanbus bus;
  size_t bytes_seen = 0;
  bus.add_frame_callback([&](const CanFrame &frame) { bytes_seen += frame.payload().size(); });
  bus.setup();
  for (uint8_t i = 0; i < 10; i++)
    bus.queue(0x100 + i, false, {i, i, i, i, i, i, i, i});

  const size_t before = allocations.load(std::memory_order_relaxed);
  bus.loop();
  const size_t after = allocations.load(std::memory_order_relaxed);

  EXPECT_EQ(bytes_seen, 80u);
  EXPECT_EQ(after - before, 0u);
#else
  GTEST_SKIP() << "Allocation counting requires AddressSanitizer";
#endif
}

TEST(CanbusFrameCallback, DeprecatedVectorCallbackStillReceivesFrames) {
  QueuedCanbus bus;
  std::vector<uint8_t> received;
  uint32_t received_id = 0;
  int calls = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  // mutable, as before
  bus.add_callback([&, seen = 0](uint32_t can_id, bool, bool, const std::vector<uint8_t> &data) mutable {
    received_id = can_id;
    received = data;
    calls = ++seen;
  });
#pragma GCC diagnostic pop
  bus.setup();
  bus.queue(0x122, false, {0x01});
  bus.queue(0x123, false, {0xAA, 0xBB});

  bus.loop();

  EXPECT_EQ(calls, 2);
  EXPECT_EQ(received_id, 0x123u);
  EXPECT_EQ(received, (std::vector<uint8_t>{0xAA, 0xBB}));
}

TEST(CanbusFrameCallback, PayloadKeepsVectorForms) {
  CanFrame frame{};
  frame.can_data_length_code = 3;
  frame.data[0] = 'a';
  frame.data[1] = 'b';
  frame.data[2] = 'c';
  const CanPayload x = frame.payload();

  EXPECT_EQ(x.size(), 3u);
  EXPECT_FALSE(x.empty());
  EXPECT_EQ(x[1], 'b');
  EXPECT_EQ(x.data()[2], 'c');
  EXPECT_EQ(std::string(x.begin(), x.end()), "abc");
  const std::vector<uint8_t> copy = x;
  EXPECT_EQ(copy, (std::vector<uint8_t>{'a', 'b', 'c'}));
  auto takes_vector = [](const std::vector<uint8_t> &v) { return v.size(); };
  EXPECT_EQ(takes_vector(x), 3u);

  CanPayload braced = {0x00, 0x20, 0x42};
  EXPECT_EQ(braced.size(), 3u);
  const CanPayload too_long = std::vector<uint8_t>(CAN_MAX_DATA_LENGTH + 2, 7);
  EXPECT_EQ(too_long.size(), CAN_MAX_DATA_LENGTH);
  frame.can_data_length_code = 15;  // legal on the wire
  EXPECT_EQ(frame.payload().size(), CAN_MAX_DATA_LENGTH);
}

}  // namespace esphome::canbus::testing
