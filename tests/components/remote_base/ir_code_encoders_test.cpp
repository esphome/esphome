#include <gtest/gtest.h>
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
uint16_t aeha_address() { return AEHA_ADDRESS; }
uint32_t aeha_carrier() { return 38000; }

// The Haier and Mirage decoders expect one item after the final mark, as a receiver capture has
RawTimings with_trailing_space(RawTimings timings) {
  timings.push_back(-10000);
  return timings;
}

template<typename A> RawTimings encode(A &action) {
  RemoteTransmitData dst;
  action.encode(&dst);
  return dst.get_data();
}

// Exposes the protected encode() of an action for testing
template<typename A> class Open : public A {
 public:
  using A::encode;
};

}  // namespace

TEST(IrCodeEncodersTest, HaierStaticAndLambdaMatchGolden) {
  Open<HaierAction<>> fixed, lambda;
  fixed.set_code_static(HAIER_CODE, sizeof(HAIER_CODE));
  lambda.set_code_template(haier_code);
  EXPECT_EQ(encode(fixed), HAIER_GOLDEN);
  EXPECT_EQ(encode(lambda), HAIER_GOLDEN);
}

TEST(IrCodeEncodersTest, MirageStaticAndLambdaMatchGolden) {
  Open<MirageAction<>> fixed, lambda;
  fixed.set_code_static(MIRAGE_CODE, sizeof(MIRAGE_CODE));
  lambda.set_code_template(mirage_code);
  EXPECT_EQ(encode(fixed), MIRAGE_GOLDEN);
  EXPECT_EQ(encode(lambda), MIRAGE_GOLDEN);
}

TEST(IrCodeEncodersTest, AehaStaticAndLambdaMatchGolden) {
  Open<AEHAAction<>> fixed, lambda;
  for (auto *action : {&fixed, &lambda}) {
    action->set_address(aeha_address);
    action->set_carrier_frequency(aeha_carrier);
  }
  fixed.set_data_static(AEHA_CODE, sizeof(AEHA_CODE));
  lambda.set_data_template(aeha_code);
  EXPECT_EQ(encode(fixed), AEHA_GOLDEN);
  EXPECT_EQ(encode(lambda), AEHA_GOLDEN);
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
  auto haier =
      HaierProtocol().decode(RemoteReceiveData(with_trailing_space(HAIER_GOLDEN), 25, TOLERANCE_MODE_PERCENTAGE));
  ASSERT_TRUE(haier.has_value());
  if (haier.has_value()) {
    EXPECT_EQ(haier->data, haier_code());
  }
  auto mirage =
      MirageProtocol().decode(RemoteReceiveData(with_trailing_space(MIRAGE_GOLDEN), 25, TOLERANCE_MODE_PERCENTAGE));
  ASSERT_TRUE(mirage.has_value());
  if (mirage.has_value()) {
    EXPECT_EQ(mirage->data, mirage_code());
  }
  auto aeha = AEHAProtocol().decode(RemoteReceiveData(AEHA_GOLDEN, 25, TOLERANCE_MODE_PERCENTAGE));
  ASSERT_TRUE(aeha.has_value());
  if (aeha.has_value()) {
    EXPECT_EQ(aeha->address, AEHA_ADDRESS);
    EXPECT_EQ(aeha->data, aeha_code());
  }
}

}  // namespace esphome::remote_base::testing
