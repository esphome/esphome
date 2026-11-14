#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "esphome/components/genibus/genibus_frame.h"

// The byte sequences are frames recorded from a real ALPHA3 or built to the same layout
namespace esphome::genibus::testing {

namespace {

using Bytes = std::vector<uint8_t>;

// A class other than 10, for APDUs the class 10 helpers must not take (class 2: measured data)
constexpr uint8_t OTHER_CLASS = 2;

Bytes vec(std::span<const uint8_t> data) { return {data.begin(), data.end()}; }

Bytes build(uint8_t destination, uint8_t source, const Bytes &apdus) {
  std::array<uint8_t, MAX_FRAME_SIZE> out{};
  const size_t size = build_request(out, destination, source, apdus);
  return {out.begin(), out.begin() + size};
}

// Feeds frame in pieces of chunk bytes; returns how many complete frames the assembler reported
int feed(FrameAssembler &assembler, const Bytes &frame, size_t chunk) {
  int complete = 0;
  for (size_t at = 0; at < frame.size(); at += chunk) {
    const size_t end = std::min(frame.size(), at + chunk);
    for (size_t i = at; i < end; i++)
      complete += assembler.feed(frame[i]) ? 1 : 0;
  }
  return complete;
}

// An ALPHA3 answers a GET of an object it does not have (86/999) with ACK 0 and the single data byte 4
const Bytes REPLY_OBJECT_MISSING = {0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x01, 0x04, 0xEE, 0x26};

const Bytes REPLY_012F = {0x24, 0x12, 0xF8, 0xE7, 0x0A, 0x0E, 0x00, 0x01, 0x2F, 0x01, 0x00,
                          0x00, 0x07, 0x00, 0x00, 0x00, 0x46, 0xE5, 0xC6, 0x00, 0xC5, 0x36};

}  // namespace

TEST(GenibusFrame, CrcMatchesKnownFrames) {
  const Bytes flow_head = {0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21};
  const Bytes power = {0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x45};
  const Bytes start = {0x05, 0xE7, 0xF8, 0x03, 0x81, 0x06};
  EXPECT_EQ(frame_crc(flow_head.data(), flow_head.size()), 0x521F);
  EXPECT_EQ(frame_crc(power.data(), power.size()), 0x8ACD);
  EXPECT_EQ(frame_crc(start.data(), start.size()), 0xE587);
}

TEST(GenibusFrame, BuildsGetRequests) {
  uint8_t apdu[5];
  EXPECT_EQ(put_get_object(apdu, 93, 289), apdu + 5);
  EXPECT_EQ(build(0xE7, 0xF8, {apdu, apdu + 5}),
            (Bytes{0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x52, 0x1F}));
  put_get_object(apdu, 87, 69);
  EXPECT_EQ(build(0xE7, 0xF8, {apdu, apdu + 5}),
            (Bytes{0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x45, 0x8A, 0xCD}));
}

TEST(GenibusFrame, BuildsSeveralApdus) {
  EXPECT_EQ(build(0xE7, 0xF8, {0x0A, 0x03, 0x5D, 0x01, 0x21, 0x0A, 0x03, 0x57, 0x00, 0x45}),
            (Bytes{0x27, 0x0C, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x0A, 0x03, 0x57, 0x00, 0x45, 0xAA, 0xB9}));
}

TEST(GenibusFrame, RefusesRequestThatDoesNotFit) {
  std::array<uint8_t, 8> small{};
  const Bytes apdus = {0x0A, 0x03, 0x5D, 0x01, 0x21};
  EXPECT_EQ(build_request(small, 0xE7, 0xF8, apdus), 0u);
  std::array<uint8_t, MAX_FRAME_SIZE + 1> big{};
  EXPECT_EQ(build_request(big, 0xE7, 0xF8, {}), 0u);
  const Bytes too_long(254, 0);
  EXPECT_EQ(build_request(big, 0xE7, 0xF8, too_long), 0u);
}

TEST(GenibusFrame, AssemblesFromAnyChunks) {
  for (size_t chunk : {1u, 3u, 20u, 64u}) {
    FrameAssembler assembler;
    EXPECT_EQ(feed(assembler, REPLY_012F, chunk), 1) << "chunk " << chunk;
    EXPECT_EQ(vec(assembler.frame()), REPLY_012F);
    EXPECT_TRUE(assembler.crc_ok());
    EXPECT_EQ(vec(assembler.apdus()), Bytes(REPLY_012F.begin() + 4, REPLY_012F.end() - 2));
  }
}

TEST(GenibusFrame, DropsGarbageBeforeStart) {
  FrameAssembler assembler;
  Bytes noisy = {0x00, 0xFF, 0x13, 0x24, 0x01};  // 0x24 followed by an impossible length starts nothing
  noisy.insert(noisy.end(), REPLY_012F.begin(), REPLY_012F.end());
  EXPECT_EQ(feed(assembler, noisy, 5), 1);
  EXPECT_EQ(vec(assembler.frame()), REPLY_012F);
}

TEST(GenibusFrame, StartsOverWithoutReset) {
  FrameAssembler assembler;
  Bytes two = REPLY_012F;
  two.insert(two.end(), REPLY_012F.begin(), REPLY_012F.end());
  EXPECT_EQ(feed(assembler, two, 7), 2);
  EXPECT_EQ(vec(assembler.frame()), REPLY_012F);
  Bytes longest = {0x24, 0xFF};
  longest.resize(MAX_FRAME_SIZE, 0);
  Bytes many = longest;
  many.insert(many.end(), longest.begin(), longest.end());
  EXPECT_EQ(feed(assembler, many, 64), 2);
}

TEST(GenibusFrame, DetectsBadCrc) {
  Bytes bad = REPLY_012F;
  bad[10] ^= 0x01;
  FrameAssembler assembler;
  EXPECT_EQ(feed(assembler, bad, 20), 1);
  EXPECT_FALSE(assembler.crc_ok());
  assembler.reset();
  EXPECT_FALSE(assembler.crc_ok());
  EXPECT_TRUE(assembler.apdus().empty());
}

TEST(GenibusFrame, LongestFrameFits) {
  Bytes frame = {0x24, 0xFF};
  frame.resize(MAX_FRAME_SIZE, 0);
  FrameAssembler assembler;
  EXPECT_EQ(feed(assembler, frame, 20), 1);
  EXPECT_EQ(assembler.frame().size(), MAX_FRAME_SIZE);
}

TEST(GenibusFrame, ReadsSeveralApdus) {
  const Bytes apdus = {0x0A, 0x01, 0x00, OTHER_CLASS, 0x40};
  ApduReader reader(apdus);
  Apdu apdu{};
  ASSERT_TRUE(reader.next(apdu));
  EXPECT_EQ(apdu.data_class, CLASS_DATA_OBJECT);
  EXPECT_EQ(apdu.ack, 0);
  EXPECT_EQ(vec(apdu.data), Bytes{0x00});
  ASSERT_TRUE(reader.next(apdu));
  EXPECT_EQ(apdu.data_class, OTHER_CLASS);
  EXPECT_EQ(apdu.ack, 1);
  EXPECT_TRUE(apdu.data.empty());
  EXPECT_FALSE(reader.next(apdu));
  EXPECT_TRUE(reader.at_end());
}

TEST(GenibusFrame, RejectsApduLongerThanFrame) {
  // The CRC is fine, but the APDU announces 3 data bytes and has 2
  const Bytes apdu_too_long = {0x27, 0x06, 0xF8, 0xE7, 0x0A, 0x03, 0x81, 0x07, 0x06, 0x87};
  FrameAssembler assembler;
  ASSERT_EQ(feed(assembler, apdu_too_long, 20), 1);
  EXPECT_TRUE(assembler.crc_ok());
  ApduReader reader(assembler.apdus());
  Apdu apdu{};
  EXPECT_FALSE(reader.next(apdu));
  EXPECT_FALSE(reader.at_end());
  const Bytes header_only = {0x0A};
  ApduReader short_reader(header_only);
  EXPECT_FALSE(short_reader.next(apdu));
}

TEST(GenibusFrame, ParsesDataObject) {
  ApduReader reader({REPLY_012F.data() + 4, REPLY_012F.size() - 6});
  Apdu apdu{};
  ASSERT_TRUE(reader.next(apdu));
  DataObject object{};
  ASSERT_TRUE(parse_data_object(apdu, object));
  EXPECT_EQ(object.status, 0);
  EXPECT_EQ(object.type, 0x012F);
  EXPECT_EQ(object.version, 1);
  EXPECT_EQ(vec(object.body), (Bytes{0x00, 0x00, 0x00, 0x46, 0xE5, 0xC6, 0x00}));
}

TEST(GenibusFrame, ExposesTypeAndVersion) {
  // Header of the power object of a newer pump: type 0x0100 version 2, 41 bytes
  Bytes data = {0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x29};
  data.resize(DATA_OBJECT_HEADER_SIZE + 41, 0);
  DataObject object{};
  ASSERT_TRUE(parse_data_object({CLASS_DATA_OBJECT, 0, data}, object));
  EXPECT_EQ(object.type, 0x0100);
  EXPECT_EQ(object.version, 2);
  EXPECT_EQ(object.body.size(), 41u);
}

TEST(GenibusFrame, RejectsBadDataObject) {
  Bytes data = {0x00, 0x01, 0x2F, 0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x46, 0xE5, 0xC6, 0x00};
  DataObject object{};
  EXPECT_FALSE(parse_data_object({CLASS_DATA_OBJECT, 2, data}, object));  // ACK: ID unknown
  EXPECT_FALSE(parse_data_object({OTHER_CLASS, 0, data}, object));
  EXPECT_FALSE(parse_data_object({CLASS_DATA_OBJECT, 0, {data.data(), 6}}, object));
  EXPECT_FALSE(parse_data_object({CLASS_DATA_OBJECT, 0, {data.data(), data.size() - 1}}, object));
  data.push_back(0);
  EXPECT_FALSE(parse_data_object({CLASS_DATA_OBJECT, 0, data}, object));
}

TEST(GenibusFrame, DetectsMissingObject) {
  FrameAssembler assembler;
  ASSERT_EQ(feed(assembler, REPLY_OBJECT_MISSING, 20), 1);
  ASSERT_TRUE(assembler.crc_ok());
  ApduReader reader(assembler.apdus());
  Apdu apdu{};
  ASSERT_TRUE(reader.next(apdu));
  EXPECT_TRUE(is_object_missing(apdu));
  DataObject object{};
  EXPECT_FALSE(parse_data_object(apdu, object));
  // ACK 2 (ID unknown), as the specification has it
  EXPECT_TRUE(is_object_missing({CLASS_DATA_OBJECT, ACK_UNKNOWN_ID, {}}));
}

TEST(GenibusFrame, OtherRepliesAreNoMissingObject) {
  const Bytes status_ok = {0x00};
  const Bytes status_missing = {DATA_OBJECT_STATUS_MISSING};
  const Bytes two_bytes = {DATA_OBJECT_STATUS_MISSING, 0x00};
  EXPECT_FALSE(is_object_missing({CLASS_DATA_OBJECT, 0, status_ok}));
  EXPECT_FALSE(is_object_missing({CLASS_DATA_OBJECT, 0, two_bytes}));
  EXPECT_FALSE(is_object_missing({CLASS_DATA_OBJECT, 0, {}}));
  EXPECT_FALSE(is_object_missing({CLASS_DATA_OBJECT, 3, status_missing}));
  EXPECT_FALSE(is_object_missing({OTHER_CLASS, 0, status_missing}));
  EXPECT_FALSE(is_object_missing({OTHER_CLASS, ACK_UNKNOWN_ID, {}}));
  ApduReader reader({REPLY_012F.data() + 4, REPLY_012F.size() - 6});
  Apdu apdu{};
  ASSERT_TRUE(reader.next(apdu));
  EXPECT_FALSE(is_object_missing(apdu));
}

}  // namespace esphome::genibus::testing
