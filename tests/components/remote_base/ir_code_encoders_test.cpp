#include <gtest/gtest.h>
#include <iterator>
#include <vector>
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/components/remote_base/haier_protocol.h"
#include "esphome/components/remote_base/mirage_protocol.h"

namespace esphome::remote_base::testing {

namespace {

const uint8_t HAIER_CODE[] = {0xA6, 0x12, 0x00, 0x00, 0x40, 0x40, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x05};
const uint8_t MIRAGE_CODE[] = {0x56, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t AEHA_CODE[] = {0x80, 0x00, 0x00, 0x00, 0x00};
constexpr uint16_t AEHA_ADDRESS = 0x2002;

// Captured from the struct encoders before they gained the pointer and length overloads
const RawTimings HAIER_GOLDEN = {
    3100, -3100, 3100, -4400, 540, -1650, 540, -580,  540, -1650, 540, -580,  540, -580,  540, -1650, 540, -1650,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -1650, 540, -580,  540, -580,  540, -1650, 540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -1650,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -1650, 540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -1650, 540, -580,  540, -580,  540, -580,  540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,  540, -580,
    540,  -580,  540,  -580,  540, -580,  540, -580,  540, -1650, 540, -580,  540, -1650, 540, -1650, 540, -580,
    540,  -1650, 540,  -1650, 540, -1650, 540, -1650, 540, -580,  540, -1650, 540};
const RawTimings MIRAGE_GOLDEN = {
    8360, -4248, 554, -545, 554, -1592, 554, -1592, 554, -545,  554, -1592, 554, -545,  554, -1592, 554, -545,
    554,  -1592, 554, -545, 554, -1592, 554, -545,  554, -1592, 554, -1592, 554, -1592, 554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,  554, -545,
    554,  -545,  554, -545, 554, -545,  554, -545,  554, -545,  554, -1592, 554, -1592, 554, -1592, 554, -545,
    554,  -1592, 554, -545, 554, -545,  554, -545,  554};
const RawTimings AEHA_GOLDEN = {
    3400, -1700, 425, -425, 425, -425, 425, -1275, 425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425, -425,
    425,  -425,  425, -425, 425, -425, 425, -425,  425, -425, 425, -1275, 425, -425, 425, -1275, 425, -425, 425, -425,
    425,  -425,  425, -425, 425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425, -425,
    425,  -425,  425, -425, 425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425, -425,
    425,  -425,  425, -425, 425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425, -425,
    425,  -425,  425, -425, 425, -425, 425, -425,  425, -425, 425, -425,  425, -425, 425};

std::vector<uint8_t> haier_code() { return {std::begin(HAIER_CODE), std::end(HAIER_CODE)}; }
std::vector<uint8_t> mirage_code() { return {std::begin(MIRAGE_CODE), std::end(MIRAGE_CODE)}; }
std::vector<uint8_t> aeha_code() { return {std::begin(AEHA_CODE), std::end(AEHA_CODE)}; }

// The Haier and Mirage decoders expect an item after the last mark (pre-existing)
RawTimings with_trailing_space(RawTimings timings) {
  timings.push_back(-10000);
  return timings;
}

// A constant table and a lambda's vector take the two encoder overloads generated actions call
template<typename P, typename... Lead>
void expect_pointer_and_vector_match(const uint8_t *code, size_t len, const std::vector<uint8_t> &bytes,
                                     const RawTimings &golden, const Lead &...lead) {
  RemoteTransmitData fixed, from_vector;
  P().encode(&fixed, lead..., code, len);
  P().encode(&from_vector, lead..., bytes);
  EXPECT_EQ(fixed.get_data(), golden);
  EXPECT_EQ(from_vector.get_data(), golden);
}

template<typename P> void expect_decodes_to(const RawTimings &timings, const std::vector<uint8_t> &code) {
  auto decoded = P().decode(RemoteReceiveData(timings, 25, TOLERANCE_MODE_PERCENTAGE));
  ASSERT_TRUE(decoded.has_value());
  // clang-tidy's unchecked-optional-access models neither gtest's ASSERT_TRUE nor value() as a check
  EXPECT_EQ(decoded.value_or(typename P::ProtocolData{}).data, code);
}

}  // namespace

TEST(IrCodeEncodersTest, HaierPointerAndVectorMatchGolden) {
  expect_pointer_and_vector_match<HaierProtocol>(HAIER_CODE, sizeof(HAIER_CODE), haier_code(), HAIER_GOLDEN);
}

TEST(IrCodeEncodersTest, MiragePointerAndVectorMatchGolden) {
  expect_pointer_and_vector_match<MirageProtocol>(MIRAGE_CODE, sizeof(MIRAGE_CODE), mirage_code(), MIRAGE_GOLDEN);
}

TEST(IrCodeEncodersTest, AehaPointerAndVectorMatchGolden) {
  expect_pointer_and_vector_match<AEHAProtocol>(AEHA_CODE, sizeof(AEHA_CODE), aeha_code(), AEHA_GOLDEN, AEHA_ADDRESS);
}

TEST(IrCodeEncodersTest, StructEncodersMatchGolden) {
  RemoteTransmitData haier, mirage, aeha;
  HaierProtocol().encode(&haier, HaierData{haier_code()});
  MirageProtocol().encode(&mirage, MirageData{mirage_code()});
  AEHAData aeha_data;
  aeha_data.address = AEHA_ADDRESS;
  aeha_data.data = aeha_code();
  AEHAProtocol().encode(&aeha, aeha_data);
  EXPECT_EQ(haier.get_data(), HAIER_GOLDEN);
  EXPECT_EQ(mirage.get_data(), MIRAGE_GOLDEN);
  EXPECT_EQ(aeha.get_data(), AEHA_GOLDEN);
}

TEST(IrCodeEncodersTest, RoundTripDecodes) {
  expect_decodes_to<HaierProtocol>(with_trailing_space(HAIER_GOLDEN), haier_code());
  expect_decodes_to<MirageProtocol>(with_trailing_space(MIRAGE_GOLDEN), mirage_code());
  expect_decodes_to<AEHAProtocol>(AEHA_GOLDEN, aeha_code());
  auto aeha = AEHAProtocol().decode(RemoteReceiveData(AEHA_GOLDEN, 25, TOLERANCE_MODE_PERCENTAGE));
  EXPECT_EQ(aeha.value_or(AEHAData{}).address, AEHA_ADDRESS);
}

}  // namespace esphome::remote_base::testing
