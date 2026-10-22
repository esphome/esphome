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

// Header, length 4, command 0xA1 (set baud rate): the rate byte and a zero follow.
constexpr uint8_t SET_BAUD_RATE_PREFIX[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA1, 0x00};
// Header, length 28, command 0xC2 (set zone): the zone type byte and a zero follow.
constexpr uint8_t SET_ZONE_PREFIX[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x1C, 0x00, 0xC2, 0x00};

// The two payload bytes after the first frame with this prefix, or none when it was not written.
template<size_t N>
optional<std::pair<uint8_t, uint8_t>> payload_after(const std::vector<uint8_t> &w, const uint8_t (&prefix)[N]) {
  auto it = std::search(w.begin(), w.end(), std::begin(prefix), std::end(prefix));
  if (it == w.end() || static_cast<size_t>(w.end() - it) < N + 2)
    return {};
  return std::make_pair(*(it + N), *(it + N + 1));
}
}  // namespace

class LD2450SelectNamesTest : public ::testing::Test {
 protected:
  void SetUp() override { this->ld2450_.set_uart_parent(&this->uart_); }

  CapturingUARTComponent uart_;
  TestableLD2450 ld2450_;
};

TEST_F(LD2450SelectNamesTest, BaudRateNamesSendTheirDeviceValue) {
  static constexpr const char *NAMES[] = {"9600", "19200", "38400", "57600", "115200", "230400", "256000", "460800"};
  for (uint8_t i = 0; i < 8; i++) {
    this->uart_.written.clear();
    this->ld2450_.set_baud_rate(NAMES[i]);
    const auto payload = payload_after(this->uart_.written, SET_BAUD_RATE_PREFIX);
    ASSERT_TRUE(payload.has_value()) << NAMES[i];
    EXPECT_EQ(payload->first, i + 1) << NAMES[i];
    EXPECT_EQ(payload->second, 0) << NAMES[i];
  }
}

TEST_F(LD2450SelectNamesTest, BaudRateEnumMatchesTheSelectOrder) {
  this->ld2450_.set_baud_rate(BAUD_RATE_256000);
  const auto payload = payload_after(this->uart_.written, SET_BAUD_RATE_PREFIX);
  ASSERT_TRUE(payload.has_value());
  EXPECT_EQ(payload->first, 7);
}

TEST_F(LD2450SelectNamesTest, UnknownBaudRateNameWritesNothing) {
  this->ld2450_.set_baud_rate("1200");
  EXPECT_TRUE(this->uart_.written.empty());
}

TEST_F(LD2450SelectNamesTest, ZoneTypeNamesSetTheTypeAndSendIt) {
  static constexpr const char *NAMES[] = {"Disabled", "Detection", "Filter"};
  for (uint8_t i = 0; i < 3; i++) {
    this->uart_.written.clear();
    this->ld2450_.set_zone_type(NAMES[i]);
    EXPECT_EQ(this->ld2450_.zone_type_, i) << NAMES[i];
    const auto payload = payload_after(this->uart_.written, SET_ZONE_PREFIX);
    ASSERT_TRUE(payload.has_value()) << NAMES[i];
    EXPECT_EQ(payload->first, i) << NAMES[i];
    EXPECT_EQ(payload->second, 0) << NAMES[i];
  }
}

TEST_F(LD2450SelectNamesTest, ZoneTypeEnumMatchesTheSelectOrder) {
  this->ld2450_.set_zone_type(ZONE_FILTER);
  EXPECT_EQ(this->ld2450_.zone_type_, 2);
  this->ld2450_.set_zone_type(ZONE_DETECTION);
  EXPECT_EQ(this->ld2450_.zone_type_, 1);
}

TEST_F(LD2450SelectNamesTest, UnknownZoneTypeNameChangesNothing) {
  this->ld2450_.set_zone_type(ZONE_DETECTION);
  this->uart_.written.clear();
  this->ld2450_.set_zone_type("Bogus");
  EXPECT_EQ(this->ld2450_.zone_type_, 1);
  EXPECT_TRUE(this->uart_.written.empty());
}

}  // namespace esphome::ld2450::testing
