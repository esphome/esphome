#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

#include "HeatpumpIR.h"
#include "esphome/components/heatpumpir/heatpumpir.h"
#include "esphome/components/heatpumpir/receiver_mitsubishi_heavy.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::heatpumpir {

bool decode_mitsubishi_heavy_frame(remote_base::RemoteReceiveData &, uint8_t[MITSUBISHI_HEAVY_FRAME_LENGTH],
                                   HeatpumpIRClimate &) {
  return false;
}

void decode_mitsubishi_heavy_zmp(const uint8_t[MITSUBISHI_HEAVY_FRAME_LENGTH], HeatpumpIRClimate &) {}

}  // namespace esphome::heatpumpir

namespace esphome::heatpumpir::testing {

struct Burst {
  remote_base::RawTimings data;
  uint32_t carrier_frequency{0};
};

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<Burst> bursts;

 protected:
  void send_internal(uint32_t, uint32_t) override {
    this->bursts.push_back(Burst{this->temp_.get_data(), this->temp_.get_carrier_frequency()});
  }
};

void configure_climate(HeatpumpIRClimate &climate, CapturingTransmitter &transmitter) {
  climate.set_transmitter(&transmitter);
  climate.set_horizontal_default(HORIZONTAL_DIRECTION_AUTO);
  climate.set_vertical_default(VERTICAL_DIRECTION_AUTO);
  climate.set_min_temperature(16);
  climate.set_max_temperature(30);
}

void transmit_cool(HeatpumpIRClimate &climate) {
  auto call = climate.make_call();
  call.set_mode(climate::CLIMATE_MODE_COOL);
  call.set_target_temperature(24);
  call.perform();
}

TEST(HeatpumpIRMultiMessage, SingleMessageProtocolSendsOneBurst) {
  CapturingTransmitter transmitter;
  HeatpumpIRClimate climate{ProtocolTag<PROTOCOL_GREE>{}};
  configure_climate(climate, transmitter);
  transmit_cool(climate);

  ASSERT_EQ(transmitter.bursts.size(), 1);
  EXPECT_EQ(transmitter.bursts[0].carrier_frequency, 38000);
  const remote_base::RawTimings expected{555, -666};
  EXPECT_EQ(transmitter.bursts[0].data, expected);
}

TEST(HeatpumpIRMultiMessage, GreeYapSendsThreeSeparateBursts) {
  CapturingTransmitter transmitter;
  HeatpumpIRClimate climate{ProtocolTag<PROTOCOL_GREEYAP>{}};
  configure_climate(climate, transmitter);
  transmit_cool(climate);

  ASSERT_EQ(transmitter.bursts.size(), 3) << "dev resends 1, then 1+2, then 1+2+3 instead of three frames";
  const remote_base::RawTimings block1{1000, -400};
  const remote_base::RawTimings block2{2000, -800};
  const remote_base::RawTimings block3{3000, -1200};
  EXPECT_EQ(transmitter.bursts[0].data, block1);
  EXPECT_EQ(transmitter.bursts[1].data, block2);
  EXPECT_EQ(transmitter.bursts[2].data, block3);
  for (const auto &burst : transmitter.bursts) {
    EXPECT_EQ(burst.carrier_frequency, 38000);
  }
}

TEST(HeatpumpIRMultiMessage, PanasonicCkpKeepsInterMessageGap) {
  CapturingTransmitter transmitter;
  panasonic_ckp_delay_ms = 0;
  HeatpumpIRClimate climate{ProtocolTag<PROTOCOL_PANASONIC_CKP>{}};
  configure_climate(climate, transmitter);
  transmit_cool(climate);

  ASSERT_EQ(transmitter.bursts.size(), 2) << "collecting every space(0) into one transmit collapses the CKP gap";
  EXPECT_EQ(panasonic_ckp_delay_ms, 1000);
  const remote_base::RawTimings first{111, -222};
  const remote_base::RawTimings second{333, -444};
  EXPECT_EQ(transmitter.bursts[0].data, first);
  EXPECT_EQ(transmitter.bursts[1].data, second);
  EXPECT_EQ(transmitter.bursts[0].carrier_frequency, 38000);
  EXPECT_EQ(transmitter.bursts[1].carrier_frequency, 38000);
}

}  // namespace esphome::heatpumpir::testing
