#include <algorithm>
#include "common.h"

namespace esphome::ld2450::testing {

namespace {
class CapturingUARTComponent : public MockUARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    this->written.insert(this->written.end(), data, data + len);
  }
  std::vector<uint8_t> written;
};

class TestNumber : public number::Number {
 protected:
  void control(float value) override {}
};

// Header, length 28, command 0xC2 (set zone): the coordinate payload follows two zone type bytes.
constexpr uint8_t SET_ZONE_PREFIX[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x1C, 0x00, 0xC2, 0x00};
}  // namespace

class LD2450ZoneNumberTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2450_.set_uart_parent(&this->uart_);
    for (auto &zone : this->ld2450_.zone_config_) {
      zone = {1, 2, 3, 4};
    }
    // Only the third zone has numbers, so the lookup must walk past the empty slots.
    this->ld2450_.set_zone_numbers(2, &this->x1_, &this->y1_, &this->x2_, &this->y2_);
    this->x1_.publish_state(-100);
    this->y1_.publish_state(200);
    this->x2_.publish_state(1500);
    this->y2_.publish_state(3000);
  }

  CapturingUARTComponent uart_;
  TestableLD2450 ld2450_;
  TestNumber x1_, y1_, x2_, y2_;
};

TEST_F(LD2450ZoneNumberTest, EachCoordinateNumberUpdatesItsZoneAndSendsCommand) {
  for (TestNumber *n : {&this->x1_, &this->y1_, &this->x2_, &this->y2_}) {
    this->uart_.written.clear();
    this->ld2450_.zone_config_[2] = {0, 0, 0, 0};
    this->ld2450_.set_zone_coordinate(n);

    EXPECT_EQ(this->ld2450_.zone_config_[2].x1, -100);
    EXPECT_EQ(this->ld2450_.zone_config_[2].y1, 200);
    EXPECT_EQ(this->ld2450_.zone_config_[2].x2, 1500);
    EXPECT_EQ(this->ld2450_.zone_config_[2].y2, 3000);
    for (size_t i = 0; i < 2; i++) {
      EXPECT_EQ(this->ld2450_.zone_config_[i].x1, 1) << "zone " << i;
      EXPECT_EQ(this->ld2450_.zone_config_[i].y2, 4) << "zone " << i;
    }

    const auto &w = this->uart_.written;
    auto it = std::search(w.begin(), w.end(), std::begin(SET_ZONE_PREFIX), std::end(SET_ZONE_PREFIX));
    ASSERT_NE(it, w.end());
    ASSERT_GE(static_cast<size_t>(w.end() - it), sizeof(SET_ZONE_PREFIX) + 26 + 4);
    const uint8_t *zone2 = &*it + sizeof(SET_ZONE_PREFIX) + 2 + 2 * 8;
    const std::vector<uint8_t> expected = {0x9C, 0xFF, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B};
    EXPECT_EQ(std::vector<uint8_t>(zone2, zone2 + 8), expected);
  }
}

TEST_F(LD2450ZoneNumberTest, UnregisteredNumberChangesNothing) {
  TestNumber other;
  other.publish_state(42);
  this->ld2450_.set_zone_coordinate(&other);

  for (size_t i = 0; i < MAX_ZONES; i++) {
    EXPECT_EQ(this->ld2450_.zone_config_[i].x1, 1) << "zone " << i;
    EXPECT_EQ(this->ld2450_.zone_config_[i].y1, 2) << "zone " << i;
    EXPECT_EQ(this->ld2450_.zone_config_[i].x2, 3) << "zone " << i;
    EXPECT_EQ(this->ld2450_.zone_config_[i].y2, 4) << "zone " << i;
  }
  EXPECT_TRUE(this->uart_.written.empty());
}

}  // namespace esphome::ld2450::testing
