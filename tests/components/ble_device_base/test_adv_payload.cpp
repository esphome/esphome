#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "esphome/components/ble_device_base/adv_payload.h"
#include "esphome/core/helpers.h"

namespace esphome::ble_device_base::testing {

namespace {
constexpr uint8_t FLAGS = 0x06;
// 00467768-6228-2272-4663-277478268000, little endian
constexpr std::array<uint8_t, 16> IMPROV_RAW = {0x00, 0x80, 0x26, 0x78, 0x74, 0x27, 0x63, 0x46,
                                                0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00};

// A second custom 128-bit UUID
constexpr std::array<uint8_t, 16> OTHER_RAW = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                               0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

ESPBTUUID improv_uuid() { return ESPBTUUID::from_raw(IMPROV_RAW.data()); }

// A 16 or 32-bit UUID written out in 128-bit Bluetooth base form
ESPBTUUID base_uuid(uint32_t value) {
  std::array<uint8_t, 16> raw = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00};
  put_le32(raw.data() + 12, value);
  return ESPBTUUID::from_raw(raw.data());
}

std::vector<uint8_t> bytes(const AdvPayload &payload) { return {payload.data(), payload.data() + payload.size()}; }

constexpr uint8_t MANUFACTURER[] = {0x4C, 0x00, 0x23, 0x77};
constexpr uint8_t IMPROV_DATA[] = {0x77, 0x46, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

AdvContent improv_server_content(std::span<const ESPBTUUID> uuids, bool priority) {
  return {.uuids = uuids,
          .priority_uuid = priority ? improv_uuid() : ESPBTUUID{},
          .manufacturer_data = MANUFACTURER,
          .service_data = IMPROV_DATA,
          .name = "esp",
          .name_len = 3,
          .appearance = 0x0340,
          .tx_power_dbm = -3,
          .name_in_adv = false};
}

std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (const auto &part : parts)
    out.insert(out.end(), part.begin(), part.end());
  return out;
}

const std::vector<uint8_t> IMPROV_UUID_ELEMENT = concat({{0x11, 0x07}, {IMPROV_RAW.begin(), IMPROV_RAW.end()}});
const std::vector<uint8_t> IMPROV_DATA_ELEMENT =
    concat({{0x09, 0x16}, {std::begin(IMPROV_DATA), std::end(IMPROV_DATA)}});
const std::vector<uint8_t> FLAGS_ELEMENT = {0x02, 0x01, FLAGS};
const std::vector<uint8_t> APPEARANCE_ELEMENT = {0x03, 0x19, 0x40, 0x03};
const std::vector<uint8_t> NAME_ELEMENT = {0x04, 0x09, 'e', 's', 'p'};
const std::vector<uint8_t> MANUFACTURER_ELEMENT = {0x05, 0xFF, 0x4C, 0x00, 0x23, 0x77};
const std::vector<uint8_t> TX_POWER_ELEMENT = {0x02, 0x0A, 0xFD};
const std::vector<uint8_t> UUID16_ELEMENT = {0x03, 0x03, 0x0A, 0x18};

std::array<ESPBTUUID, 16> sequential_uuid16s() {
  std::array<ESPBTUUID, 16> uuids;
  for (size_t i = 0; i < uuids.size(); i++)
    uuids[i] = ESPBTUUID::from_uint16(static_cast<uint16_t>(0x1800 + i));
  return uuids;
}

std::vector<uint8_t> uuid16_list(uint8_t type, uint16_t first, uint16_t last) {
  std::vector<uint8_t> out = {static_cast<uint8_t>(1 + (last - first + 1) * 2), type};
  for (uint32_t v = first; v <= last; v++) {
    out.push_back(v & 0xFF);
    out.push_back(v >> 8);
  }
  return out;
}

const std::vector<uint8_t> OTHER_UUID_ELEMENT = concat({{0x11, 0x07}, {OTHER_RAW.begin(), OTHER_RAW.end()}});
const std::array<ESPBTUUID, 2> UUID16_AND_IMPROV = {ESPBTUUID::from_uint16(0x180A), improv_uuid()};
const std::array<ESPBTUUID, 2> IMPROV_AND_OTHER = {improv_uuid(), ESPBTUUID::from_raw(OTHER_RAW.data())};
const std::array<ESPBTUUID, 1> IMPROV_ONLY = {improv_uuid()};
const std::array<ESPBTUUID, 16> UUID16S = sequential_uuid16s();
}  // namespace

TEST(AdvPayload, UuidSize) {
  EXPECT_EQ(AdvPayload::uuid_size(ESPBTUUID::from_uint16(0x180F)), 2u);
  EXPECT_EQ(AdvPayload::uuid_size(ESPBTUUID::from_uint32(0x180F)), 2u);
  EXPECT_EQ(AdvPayload::uuid_size(ESPBTUUID::from_uint32(0x12345678)), 4u);
  EXPECT_EQ(AdvPayload::uuid_size(base_uuid(0x180F)), 2u);
  EXPECT_EQ(AdvPayload::uuid_size(base_uuid(0x12345678)), 4u);
  EXPECT_EQ(AdvPayload::uuid_size(improv_uuid()), 16u);
}

TEST(AdvPayload, ServiceDataThatDoesNotFitIsDropped) {
  const uint8_t service_data[9] = {};
  AdvPayload adv;
  adv.add_flags(FLAGS);
  const ESPBTUUID uuid = improv_uuid();
  adv.add_uuids({&uuid, 1});
  adv.add_service_data(service_data);
  EXPECT_EQ(adv.size(), 3u + 18u);
}

TEST(AdvPayload, LongNameIsShortened) {
  const char *name = "a-very-long-device-name-over-31";
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_name(name, strlen(name));
  ASSERT_EQ(adv.size(), AdvPayload::MAX_LEN);
  EXPECT_EQ(adv.data()[3], 27);
  EXPECT_EQ(adv.data()[4], 0x08);
  EXPECT_EQ(memcmp(adv.data() + 5, name, 26), 0);
}

TEST(AdvPayload, NameAndTxPower) {
  AdvPayload rsp;
  rsp.add_name("esp", 3);
  rsp.add_tx_power(-3);
  EXPECT_EQ(bytes(rsp), (std::vector<uint8_t>{0x04, 0x09, 'e', 's', 'p', 0x02, 0x0A, 0xFD}));
}

TEST(AdvPayload, ManufacturerDataIsTruncated) {
  std::vector<uint8_t> data(40, 0xAB);
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_appearance(0x0340);
  adv.add_manufacturer_data(data);
  ASSERT_EQ(adv.size(), AdvPayload::MAX_LEN);
  EXPECT_EQ(adv.data()[3], 0x03);
  EXPECT_EQ(adv.data()[4], 0x19);
  EXPECT_EQ(adv.data()[5], 0x40);
  EXPECT_EQ(adv.data()[6], 0x03);
  EXPECT_EQ(adv.data()[7], 23);  // 22 bytes of data
  EXPECT_EQ(adv.data()[8], 0xFF);
}

TEST(AdvPayload, AppearanceNeedsFourBytes) {
  AdvPayload adv;
  adv.add_name("abcdefghijklmnopqrstuvwxyz", 26);  // leaves 3 bytes
  adv.add_appearance(0x0340);
  EXPECT_EQ(adv.size(), 28u);
  adv.add_appearance(0);
  EXPECT_EQ(adv.size(), 28u);
}

TEST(AdvPayload, UuidListsByTypeThenFirst128) {
  const ESPBTUUID uuids[] = {improv_uuid(), ESPBTUUID::from_uint16(0x180F), base_uuid(0x12345678), base_uuid(0x180A)};
  AdvPayload adv;
  adv.add_uuids(uuids);
  EXPECT_EQ(bytes(adv), (std::vector<uint8_t>{0x05, 0x03, 0x0F, 0x18, 0x0A, 0x18, 0x05, 0x05, 0x78, 0x56,
                                              0x34, 0x12, 0x11, 0x07, 0x00, 0x80, 0x26, 0x78, 0x74, 0x27,
                                              0x63, 0x46, 0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00}));
}

TEST(AdvPayload, Only128BitUuidWhenItFits) {
  const ESPBTUUID uuids[] = {improv_uuid(), improv_uuid()};
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_name("0123456789", 10);  // leaves 16 bytes
  adv.add_uuids(uuids);
  EXPECT_EQ(adv.size(), 15u);
}

TEST(AdvPayload, Partial16BitList) {
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_uuids(UUID16S);
  ASSERT_EQ(adv.size(), 3u + 2u + 13u * 2u);
  EXPECT_EQ(adv.data()[3], 27);
  EXPECT_EQ(adv.data()[4], 0x02);
  EXPECT_EQ(adv.data()[5], 0x00);
  EXPECT_EQ(adv.data()[6], 0x18);
}

TEST(AdvPayload, NoEmpty16BitList) {
  const ESPBTUUID uuids[] = {ESPBTUUID::from_uint16(0x180A)};
  AdvPayload adv;
  adv.add_name("abcdefghijklmnopqrstuvwxyz", 26);  // leaves 3 bytes
  const auto before = bytes(adv);
  adv.add_uuids(uuids);
  EXPECT_EQ(bytes(adv), before);
}

TEST(AdvPayload, NoEmpty32BitList) {
  const ESPBTUUID uuids[] = {ESPBTUUID::from_uint32(0x12345678)};
  AdvPayload adv;
  adv.add_name("abcdefghijklmnopqrstuvwx", 24);  // leaves 5 bytes
  const auto before = bytes(adv);
  adv.add_uuids(uuids);
  EXPECT_EQ(bytes(adv), before);
}

TEST(AdvPayload, ShortUuidAfterLongListsStillFits) {
  // 15 custom 128-bit and 16 32-bit UUIDs ahead of a 16-bit one, which goes first on air
  std::array<ESPBTUUID, 15 + 16 + 1> uuids;
  for (size_t i = 0; i < 15; i++)
    uuids[i] = improv_uuid();
  for (size_t i = 0; i < 16; i++)
    uuids[15 + i] = ESPBTUUID::from_uint32(static_cast<uint32_t>(0x12340000 + i));
  uuids.back() = ESPBTUUID::from_uint16(0x180A);
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_uuids(uuids);
  EXPECT_EQ(adv.data()[3], 0x03);
  EXPECT_EQ(adv.data()[4], 0x03);
  EXPECT_EQ(adv.data()[5], 0x0A);
  EXPECT_EQ(adv.data()[6], 0x18);
  EXPECT_EQ(adv.data()[8], 0x04);  // partial 32-bit list
}

TEST(AdvPayload, SkipLeavesOutOneUuid) {
  const ESPBTUUID uuids[] = {improv_uuid(), ESPBTUUID::from_uint16(0x180A)};
  AdvPayload rsp;
  rsp.add_uuids(uuids, improv_uuid());
  EXPECT_EQ(bytes(rsp), (std::vector<uint8_t>{0x03, 0x03, 0x0A, 0x18}));
}

TEST(BuildAdvPackets, PriorityUuidOwnsTheAdvertisingPacket) {
  const ESPBTUUID uuids[] = {improv_uuid(), ESPBTUUID::from_uint16(0x180A)};
  AdvPayload adv;
  AdvPayload rsp;
  build_adv_packets(improv_server_content(uuids, true), adv, rsp);
  EXPECT_EQ(bytes(adv), concat({FLAGS_ELEMENT, IMPROV_UUID_ELEMENT, IMPROV_DATA_ELEMENT}));
  EXPECT_EQ(adv.size(), AdvPayload::MAX_LEN);
  EXPECT_EQ(bytes(rsp),
            concat({APPEARANCE_ELEMENT, NAME_ELEMENT, MANUFACTURER_ELEMENT, TX_POWER_ELEMENT, UUID16_ELEMENT}));
}

struct SplitCase {
  const char *name;
  std::span<const ESPBTUUID> uuids;
  bool full_content;  // appearance, manufacturer data and service data; else only the name and UUIDs
  bool service_data;
  bool name_in_adv;
  std::vector<uint8_t> adv;
  std::vector<uint8_t> rsp;
};

class BuildAdvPacketsWithoutPriority : public ::testing::TestWithParam<SplitCase> {};

// The scan response carries only what did not fit in the advertising packet
TEST_P(BuildAdvPacketsWithoutPriority, ScanResponseCarriesWhatDidNotFit) {
  const SplitCase &c = GetParam();
  AdvContent content = improv_server_content(c.uuids, false);
  if (!c.full_content) {
    content.manufacturer_data = {};
    content.appearance = 0;
  }
  if (!c.service_data)
    content.service_data = {};
  content.name_in_adv = c.name_in_adv;
  AdvPayload adv;
  AdvPayload rsp;
  build_adv_packets(content, adv, rsp);
  EXPECT_EQ(bytes(adv), c.adv);
  EXPECT_EQ(bytes(rsp), c.rsp);
}

INSTANTIATE_TEST_SUITE_P(Split, BuildAdvPacketsWithoutPriority,
                         ::testing::Values(SplitCase{"Uuid128", UUID16_AND_IMPROV, true, true, false,
                                                     concat({FLAGS_ELEMENT, APPEARANCE_ELEMENT, MANUFACTURER_ELEMENT,
                                                             UUID16_ELEMENT, IMPROV_DATA_ELEMENT}),
                                                     concat({NAME_ELEMENT, TX_POWER_ELEMENT, IMPROV_UUID_ELEMENT})},
                                           SplitCase{"SecondUuid128", IMPROV_AND_OTHER, false, false, false,
                                                     concat({FLAGS_ELEMENT, IMPROV_UUID_ELEMENT}),
                                                     concat({NAME_ELEMENT, TX_POWER_ELEMENT, OTHER_UUID_ELEMENT})},
                                           SplitCase{"Partial16BitList", UUID16S, false, false, false,
                                                     concat({FLAGS_ELEMENT, uuid16_list(0x02, 0x1800, 0x180C)}),
                                                     concat({NAME_ELEMENT, TX_POWER_ELEMENT,
                                                             uuid16_list(0x03, 0x180D, 0x180F)})},
                                           SplitCase{"ServiceData", IMPROV_ONLY, false, true, true,
                                                     concat({FLAGS_ELEMENT, NAME_ELEMENT, IMPROV_UUID_ELEMENT}),
                                                     concat({NAME_ELEMENT, TX_POWER_ELEMENT, IMPROV_DATA_ELEMENT})},
                                           SplitCase{"NameInAdvertisingPacket",
                                                     {},
                                                     false,
                                                     false,
                                                     true,
                                                     concat({FLAGS_ELEMENT, NAME_ELEMENT}),
                                                     concat({NAME_ELEMENT, TX_POWER_ELEMENT})}),
                         [](const ::testing::TestParamInfo<SplitCase> &info) { return std::string(info.param.name); });

}  // namespace esphome::ble_device_base::testing
