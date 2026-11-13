#include <gtest/gtest.h>
#include <vector>
#include "esphome/components/ld2420/ld2420.h"

namespace esphome::ld2420::testing {

class LD2420PublishTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2420_.set_distance_sensor(&this->distance_);
    this->ld2420_.set_presence_binary_sensor(&this->presence_);
    this->distance_.add_on_state_callback([this](float value) { this->distances_.push_back(value); });
    this->presence_.add_on_state_callback([this](bool value) { this->presences_.push_back(value); });
#ifdef USE_BINARY_SENSOR_RAW_CALLBACK
    // Counts every publish_state() call, which the binary sensor would otherwise de-duplicate itself
    this->presence_.add_on_raw_state_callback([this](bool) { this->presence_publishes_++; });
#endif
  }
  // The fixture is a friend of the hub, so it sets the parsed values and publishes them
  void publish_to_(LD2420Component &hub, uint16_t distance, bool presence) {
    hub.set_distance_(distance);
    hub.set_presence_(presence);
    hub.publish_states_();
  }
  void publish_(uint16_t distance, bool presence) { this->publish_to_(this->ld2420_, distance, presence); }

  LD2420Component ld2420_;
  sensor::Sensor distance_;
  binary_sensor::BinarySensor presence_;
  std::vector<float> distances_;
  std::vector<bool> presences_;
  size_t presence_publishes_{0};
};

TEST_F(LD2420PublishTest, PublishesDistanceAndPresence) {
  this->publish_(120, true);
  EXPECT_EQ(this->distances_, std::vector<float>{120.0f});
  EXPECT_EQ(this->presences_, std::vector<bool>{true});
}

TEST_F(LD2420PublishTest, SkipsUnchangedValues) {
  this->publish_(120, true);
  this->publish_(120, true);
  EXPECT_EQ(this->distances_.size(), 1u);
  EXPECT_EQ(this->presences_.size(), 1u);
#ifdef USE_BINARY_SENSOR_RAW_CALLBACK
  EXPECT_EQ(this->presence_publishes_, 1u);
#endif
  this->publish_(150, true);
  this->publish_(150, false);
  EXPECT_EQ(this->distances_, (std::vector<float>{120.0f, 150.0f}));
  EXPECT_EQ(this->presences_, (std::vector<bool>{true, false}));
}

TEST_F(LD2420PublishTest, DistanceIsPublishedBeforePresence) {
  float distance_seen = NAN;
  this->presence_.add_on_state_callback([this, &distance_seen](bool) { distance_seen = this->distance_.get_state(); });
  this->publish_(200, true);
  EXPECT_EQ(distance_seen, 200.0f);
}

TEST_F(LD2420PublishTest, NoEntitiesConfigured) {
  LD2420Component hub;
  this->publish_to_(hub, 10, true);  // must not crash without entities
}

}  // namespace esphome::ld2420::testing
