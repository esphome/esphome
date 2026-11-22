#include "esphome/components/zigbee/zigbee_endpoint_listeners.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

namespace esphome::zigbee::testing {

struct FakeListener {
  uint8_t endpoint;
  std::vector<uint8_t> events{};
  uint8_t get_endpoint() const { return this->endpoint; }
  void on_zcl_device_event(uint8_t buffer) { this->events.push_back(buffer); }
};

TEST(EndpointListenersTest, SparseRoutingAndCapacity) {
  EndpointListeners<FakeListener, 2> listeners;
  FakeListener zero{0}, too_high{240}, first{17}, duplicate{17}, last{239}, extra{1};
  EXPECT_FALSE(listeners.dispatch(17, 1));
  EXPECT_FALSE(listeners.add(&zero));
  EXPECT_FALSE(listeners.add(&too_high));
  ASSERT_TRUE(listeners.add(&first));
  EXPECT_FALSE(listeners.add(&duplicate));
  ASSERT_TRUE(listeners.add(&last));
  EXPECT_FALSE(listeners.add(&extra));

  EXPECT_TRUE(listeners.dispatch(239, 3));
  EXPECT_TRUE(listeners.dispatch(17, 7));
  EXPECT_FALSE(listeners.dispatch(0, 10));
  EXPECT_FALSE(listeners.dispatch(1, 10));
  EXPECT_EQ(first.events, std::vector<uint8_t>{7});
  EXPECT_EQ(last.events, std::vector<uint8_t>{3});
  EXPECT_TRUE(duplicate.events.empty());
}

TEST(EndpointListenersTest, FullTableAndEmptyTable) {
  EndpointListeners<FakeListener, 8> full;
  std::vector<FakeListener> entities;
  entities.reserve(9);
  for (uint8_t endpoint = 1; endpoint <= 9; endpoint++)
    entities.push_back(FakeListener{endpoint});
  for (uint8_t i = 0; i < 8; i++)
    ASSERT_TRUE(full.add(&entities[i]));
  EXPECT_FALSE(full.add(&entities[8]));
  for (uint8_t i = 0; i < 8; i++) {
    ASSERT_TRUE(full.dispatch(entities[i].endpoint, i));
    EXPECT_EQ(entities[i].events, std::vector<uint8_t>{i});
  }

  EndpointListeners<FakeListener, 0> empty;
  FakeListener one{1};
  EXPECT_FALSE(empty.add(&one));
  EXPECT_FALSE(empty.dispatch(1, 0));
}

}  // namespace esphome::zigbee::testing
