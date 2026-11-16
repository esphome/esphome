#include "esphome/components/zigbee/zigbee_endpoint_callbacks.h"

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <functional>

namespace esphome::zigbee::testing {

TEST(EndpointCallbacksTest, SparseRoutingAndCapacity) {
  esphome::zigbee::EndpointCallbacks<2> callbacks;
  int first = 0;
  int second = 0;
  EXPECT_FALSE(callbacks.dispatch(0, 1));
  EXPECT_FALSE(callbacks.dispatch(17, 1));
  EXPECT_FALSE(callbacks.add(0, [](uint8_t) {}));
  EXPECT_FALSE(callbacks.add(240, [](uint8_t) {}));
  EXPECT_FALSE(callbacks.add(255, [](uint8_t) {}));
  ASSERT_TRUE(callbacks.add(17, [&](uint8_t value) { first += value; }));
  EXPECT_FALSE(callbacks.add(17, [&](uint8_t) { first = -100; }));
  ASSERT_TRUE(callbacks.add(239, [&](uint8_t value) { second += value; }));
  EXPECT_FALSE(callbacks.add(1, [](uint8_t) {}));
  ASSERT_TRUE(callbacks.dispatch(239, 3));
  ASSERT_TRUE(callbacks.dispatch(17, 7));
  EXPECT_EQ(first, 7);
  EXPECT_EQ(second, 3);
  EXPECT_FALSE(callbacks.dispatch(0, 10));
  EXPECT_FALSE(callbacks.dispatch(1, 10));
  EXPECT_FALSE(callbacks.dispatch(240, 10));
  EXPECT_EQ(first, 7);
  EXPECT_EQ(second, 3);

  esphome::zigbee::EndpointCallbacks<8> full;
  std::array<int, 8> values{};
  for (uint8_t endpoint = 1; endpoint <= 8; endpoint++) {
    ASSERT_TRUE(full.add(endpoint, [&, endpoint](uint8_t value) { values[endpoint - 1] = value; }));
  }
  EXPECT_FALSE(full.add(9, [](uint8_t) {}));
  for (uint8_t endpoint = 1; endpoint <= 8; endpoint++) {
    ASSERT_TRUE(full.dispatch(endpoint, endpoint));
    EXPECT_EQ(values[endpoint - 1], endpoint);
  }
  esphome::zigbee::EndpointCallbacks<0> empty;
  EXPECT_FALSE(empty.add(1, [](uint8_t) {}));
  EXPECT_FALSE(empty.dispatch(1, 0));
}

TEST(EndpointCallbacksTest, SingleSlot) {
  EndpointCallbacks<1> callbacks;
  int value = 0;
  ASSERT_TRUE(callbacks.add(239, [&](uint8_t buffer) { value = buffer; }));
  EXPECT_FALSE(callbacks.add(1, [](uint8_t) {}));
  EXPECT_TRUE(callbacks.dispatch(239, 7));
  EXPECT_EQ(value, 7);
}

TEST(EndpointCallbacksTest, EmptyCallbackKeepsItsSlot) {
  EndpointCallbacks<1> callbacks;
  ASSERT_TRUE(callbacks.add(17, std::function<void(uint8_t)>{}));
  EXPECT_FALSE(callbacks.dispatch(17, 7));
  EXPECT_FALSE(callbacks.add(17, [](uint8_t) {}));
  EXPECT_FALSE(callbacks.add(239, [](uint8_t) {}));
  EXPECT_FALSE(callbacks.dispatch(17, 7));
}

}  // namespace esphome::zigbee::testing
