#include "esphome/components/zigbee/zigbee_endpoint_callbacks.h"

#include <gtest/gtest.h>
#include <cstdint>

namespace esphome::zigbee {

TEST(EndpointCallbacksTest, SparseRoutingAndCapacity) {
  esphome::zigbee::EndpointCallbacks<2> callbacks;
  int first = 0;
  int second = 0;
  ASSERT_TRUE(!callbacks.dispatch(0, 1));
  ASSERT_TRUE(!callbacks.dispatch(17, 1));
  ASSERT_TRUE(!callbacks.add(0, [](uint8_t) {}));
  ASSERT_TRUE(!callbacks.add(240, [](uint8_t) {}));
  ASSERT_TRUE(!callbacks.add(255, [](uint8_t) {}));
  ASSERT_TRUE(callbacks.add(17, [&](uint8_t value) { first += value; }));
  ASSERT_TRUE(!callbacks.add(17, [&](uint8_t) { first = -100; }));
  ASSERT_TRUE(callbacks.add(239, [&](uint8_t value) { second += value; }));
  ASSERT_TRUE(!callbacks.add(1, [](uint8_t) {}));
  ASSERT_TRUE(callbacks.dispatch(239, 3));
  ASSERT_TRUE(callbacks.dispatch(17, 7));
  ASSERT_TRUE(first == 7 && second == 3);
  ASSERT_TRUE(!callbacks.dispatch(0, 10));
  ASSERT_TRUE(!callbacks.dispatch(1, 10));
  ASSERT_TRUE(!callbacks.dispatch(240, 10));
  ASSERT_TRUE(first == 7 && second == 3);

  esphome::zigbee::EndpointCallbacks<8> full;
  int calls = 0;
  for (uint8_t endpoint = 1; endpoint <= 8; endpoint++) {
    ASSERT_TRUE(full.add(endpoint, [&](uint8_t) { calls++; }));
  }
  ASSERT_TRUE(!full.add(9, [](uint8_t) {}));
  for (uint8_t endpoint = 1; endpoint <= 8; endpoint++) {
    ASSERT_TRUE(full.dispatch(endpoint, 0));
  }
  ASSERT_TRUE(calls == 8);
  esphome::zigbee::EndpointCallbacks<0> empty;
  ASSERT_TRUE(!empty.add(1, [](uint8_t) {}));
  ASSERT_TRUE(!empty.dispatch(1, 0));
}

TEST(EndpointCallbacksTest, SingleSlot) {
  EndpointCallbacks<1> callbacks;
  int value = 0;
  ASSERT_TRUE(callbacks.add(239, [&](uint8_t buffer) { value = buffer; }));
  EXPECT_FALSE(callbacks.add(1, [](uint8_t) {}));
  EXPECT_TRUE(callbacks.dispatch(239, 7));
  EXPECT_EQ(value, 7);
}

}  // namespace esphome::zigbee
