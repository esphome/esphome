#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "esphome/components/esp32_ble/adv_payload.h"

namespace esphome::esp32_ble::testing {

namespace {
using UUID128 = AdvPayload::UUID128;

constexpr uint8_t FLAGS = 0x06;
// 00467768-6228-2272-4663-277478268000, little endian
constexpr UUID128 IMPROV_UUID = {0x00, 0x80, 0x26, 0x78, 0x74, 0x27, 0x63, 0x46,
                                 0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00};

UUID128 base_uuid(uint32_t value) {
  UUID128 uuid = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00};
  put_le32(uuid.data() + 12, value);
  return uuid;
}

std::vector<uint8_t> bytes(const AdvPayload &payload) { return {payload.data(), payload.data() + payload.size()}; }
}  // namespace

TEST(AdvPayload, UuidSize) {
  EXPECT_EQ(AdvPayload::uuid_size(base_uuid(0x180F)), 2u);
  EXPECT_EQ(AdvPayload::uuid_size(base_uuid(0x12345678)), 4u);
  EXPECT_EQ(AdvPayload::uuid_size(IMPROV_UUID), 16u);
}

TEST(AdvPayload, ImprovPriorityPacketFillsAll31Bytes) {
  const uint8_t service_data[8] = {0x77, 0x46, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_uuids(std::span<const UUID128>(&IMPROV_UUID, 1));
  adv.add_service_data(service_data);

  std::vector<uint8_t> expected = {0x02, 0x01, FLAGS, 0x11, 0x07};
  expected.insert(expected.end(), IMPROV_UUID.begin(), IMPROV_UUID.end());
  expected.insert(expected.end(), {0x09, 0x16});
  expected.insert(expected.end(), std::begin(service_data), std::end(service_data));
  EXPECT_EQ(bytes(adv), expected);
  EXPECT_EQ(adv.size(), AdvPayload::MAX_LEN);
}

TEST(AdvPayload, ServiceDataThatDoesNotFitIsDropped) {
  const uint8_t service_data[9] = {};
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_uuids(std::span<const UUID128>(&IMPROV_UUID, 1));
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
  const UUID128 uuids[] = {IMPROV_UUID, base_uuid(0x180F), base_uuid(0x12345678), base_uuid(0x180A)};
  AdvPayload adv;
  adv.add_uuids(uuids);
  EXPECT_EQ(bytes(adv), (std::vector<uint8_t>{0x05, 0x03, 0x0F, 0x18, 0x0A, 0x18, 0x05, 0x05, 0x78, 0x56,
                                              0x34, 0x12, 0x11, 0x07, 0x00, 0x80, 0x26, 0x78, 0x74, 0x27,
                                              0x63, 0x46, 0x72, 0x22, 0x28, 0x62, 0x68, 0x77, 0x46, 0x00}));
}

TEST(AdvPayload, Only128BitUuidWhenItFits) {
  const UUID128 uuids[] = {IMPROV_UUID, IMPROV_UUID};
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_name("0123456789", 10);  // leaves 16 bytes
  adv.add_uuids(uuids);
  EXPECT_EQ(adv.size(), 15u);
}

TEST(AdvPayload, Partial16BitList) {
  std::array<UUID128, 16> uuids;
  for (uint16_t i = 0; i < uuids.size(); i++)
    uuids[i] = base_uuid(0x1800 + i);
  AdvPayload adv;
  adv.add_flags(FLAGS);
  adv.add_uuids(uuids);
  ASSERT_EQ(adv.size(), 3u + 2u + 13u * 2u);
  EXPECT_EQ(adv.data()[3], 27);
  EXPECT_EQ(adv.data()[4], 0x02);
  EXPECT_EQ(adv.data()[5], 0x00);
  EXPECT_EQ(adv.data()[6], 0x18);
}

namespace {
constexpr uint8_t MANUFACTURER[] = {0x4C, 0x00, 0x23, 0x77};
constexpr uint8_t IMPROV_DATA[] = {0x77, 0x46, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

AdvContent improv_server_content(std::span<const UUID128> uuids, bool priority) {
  return {.uuids = uuids,
          .manufacturer_data = MANUFACTURER,
          .service_data = IMPROV_DATA,
          .name = "esp",
          .name_len = 3,
          .appearance = 0x0340,
          .tx_power_dbm = -3,
          .name_in_adv = false,
          .priority = priority};
}

std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (const auto &part : parts)
    out.insert(out.end(), part.begin(), part.end());
  return out;
}

const std::vector<uint8_t> IMPROV_UUID_ELEMENT = concat({{0x11, 0x07}, {IMPROV_UUID.begin(), IMPROV_UUID.end()}});
const std::vector<uint8_t> IMPROV_DATA_ELEMENT =
    concat({{0x09, 0x16}, {std::begin(IMPROV_DATA), std::end(IMPROV_DATA)}});
const std::vector<uint8_t> FLAGS_ELEMENT = {0x02, 0x01, FLAGS};
const std::vector<uint8_t> APPEARANCE_ELEMENT = {0x03, 0x19, 0x40, 0x03};
const std::vector<uint8_t> NAME_ELEMENT = {0x04, 0x09, 'e', 's', 'p'};
const std::vector<uint8_t> MANUFACTURER_ELEMENT = {0x05, 0xFF, 0x4C, 0x00, 0x23, 0x77};
const std::vector<uint8_t> TX_POWER_ELEMENT = {0x02, 0x0A, 0xFD};
const std::vector<uint8_t> UUID16_ELEMENT = {0x03, 0x03, 0x0A, 0x18};
}  // namespace

TEST(BuildAdvPackets, PriorityUuidOwnsTheAdvertisingPacket) {
  const UUID128 uuids[] = {IMPROV_UUID, base_uuid(0x180A)};
  AdvPayload adv;
  AdvPayload rsp;
  build_adv_packets(improv_server_content(uuids, true), adv, rsp);
  EXPECT_EQ(bytes(adv), concat({FLAGS_ELEMENT, IMPROV_UUID_ELEMENT, IMPROV_DATA_ELEMENT}));
  EXPECT_EQ(bytes(rsp),
            concat({APPEARANCE_ELEMENT, NAME_ELEMENT, MANUFACTURER_ELEMENT, TX_POWER_ELEMENT, UUID16_ELEMENT}));
}

TEST(BuildAdvPackets, WithoutPriorityScanResponseRepeatsUuidsAndServiceData) {
  const UUID128 uuids[] = {base_uuid(0x180A), IMPROV_UUID};
  AdvPayload adv;
  AdvPayload rsp;
  build_adv_packets(improv_server_content(uuids, false), adv, rsp);
  // No room for the 128-bit UUID in the advertising packet, nor for the service data in the scan response
  EXPECT_EQ(bytes(adv),
            concat({FLAGS_ELEMENT, APPEARANCE_ELEMENT, MANUFACTURER_ELEMENT, UUID16_ELEMENT, IMPROV_DATA_ELEMENT}));
  EXPECT_EQ(bytes(rsp), concat({NAME_ELEMENT, TX_POWER_ELEMENT, UUID16_ELEMENT, IMPROV_UUID_ELEMENT}));
}

TEST(BuildAdvPackets, NameInAdvertisingPacket) {
  AdvContent content = improv_server_content({}, false);
  content.manufacturer_data = {};
  content.service_data = {};
  content.appearance = 0;
  content.name_in_adv = true;
  AdvPayload adv;
  AdvPayload rsp;
  build_adv_packets(content, adv, rsp);
  EXPECT_EQ(bytes(adv), concat({FLAGS_ELEMENT, NAME_ELEMENT}));
  EXPECT_EQ(bytes(rsp), concat({NAME_ELEMENT, TX_POWER_ELEMENT}));
}

}  // namespace esphome::esp32_ble::testing
