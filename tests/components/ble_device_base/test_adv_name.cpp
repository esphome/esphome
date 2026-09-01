#include "esphome/components/ble_device_base/ble_device.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

namespace esphome::ble_device_base {
namespace {

// AD types under test
constexpr uint8_t AD_SHORT_NAME = 0x08;
constexpr uint8_t AD_COMPLETE_NAME = 0x09;

void append_name(std::vector<uint8_t> &adv, uint8_t ad_type, const char *name) {
  size_t len = strlen(name);
  adv.push_back(static_cast<uint8_t>(len + 1));
  adv.push_back(ad_type);
  adv.insert(adv.end(), name, name + len);
}

ESPBTDevice device_from(const std::vector<uint8_t> &adv) {
  const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  ESPBTDevice device;
  device.from_scan_result(mac, -59, 0, adv.data(), static_cast<uint16_t>(adv.size()));
  return device;
}

}  // namespace

TEST(BleAdvName, ParsesACompleteName) {
  std::vector<uint8_t> adv;
  append_name(adv, AD_COMPLETE_NAME, "TP96");
  ESPBTDevice device = device_from(adv);
  EXPECT_EQ(device.get_name(), "TP96");
  // The backing buffer is NUL-terminated so c_str() is usable directly.
  EXPECT_STREQ(device.get_name().c_str(), "TP96");
}

TEST(BleAdvName, LongestNameWinsShortenedThenComplete) {
  // A merged adv + scan-response frame can carry both forms; the shortened
  // one must never replace the complete one.
  std::vector<uint8_t> adv;
  append_name(adv, AD_SHORT_NAME, "Radon");
  append_name(adv, AD_COMPLETE_NAME, "RadonEye");
  EXPECT_EQ(device_from(adv).get_name(), "RadonEye");
}

TEST(BleAdvName, LongestNameWinsCompleteThenShortened) {
  std::vector<uint8_t> adv;
  append_name(adv, AD_COMPLETE_NAME, "RadonEye");
  append_name(adv, AD_SHORT_NAME, "Radon");
  EXPECT_EQ(device_from(adv).get_name(), "RadonEye");
}

TEST(BleAdvName, MaxLengthNameFitsAndTerminates) {
  // 29 bytes is the largest name a legacy AD element can carry and exactly
  // fills the fixed buffer.
  std::string max_name(29, 'a');
  std::vector<uint8_t> adv;
  append_name(adv, AD_COMPLETE_NAME, max_name.c_str());
  ESPBTDevice device = device_from(adv);
  EXPECT_EQ(device.get_name().size(), 29u);
  EXPECT_EQ(device.get_name(), max_name);
  EXPECT_STREQ(device.get_name().c_str(), max_name.c_str());
}

TEST(BleAdvName, ReparseResetsThePreviousName) {
  const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  std::vector<uint8_t> first;
  append_name(first, AD_COMPLETE_NAME, "RadonEye");
  std::vector<uint8_t> second;
  append_name(second, AD_COMPLETE_NAME, "TP96");

  ESPBTDevice device;
  device.from_scan_result(mac, -59, 0, first.data(), static_cast<uint16_t>(first.size()));
  ASSERT_EQ(device.get_name(), "RadonEye");
  // A shorter name from a fresh report must fully replace the longer one:
  // the longest-name rule applies within one report, not across reports.
  device.from_scan_result(mac, -59, 0, second.data(), static_cast<uint16_t>(second.size()));
  EXPECT_EQ(device.get_name(), "TP96");
  EXPECT_STREQ(device.get_name().c_str(), "TP96");
}

TEST(BleAdvName, NoNamePresentIsEmpty) {
  std::vector<uint8_t> adv = {0x02, 0x0A, 0x00};  // TX power only
  EXPECT_TRUE(device_from(adv).get_name().empty());
}

}  // namespace esphome::ble_device_base
