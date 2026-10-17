#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/vevor_dtx/vevor_dtx.h"

namespace esphome::vevor_dtx::testing {

static constexpr uint32_t BIT_TIME_US = 88;
static constexpr int32_t FRAME_GAP_US = -20000;

class VevorDtxUnderTest : public VevorDtxComponent {
 public:
  using VevorDtxComponent::decode_timings_;
  using VevorDtxComponent::Payload;
};

// Payload of one transmission: station 0x1234 with a low battery, 21.5 °C, 55 %, 9.96 km/h wind,
// 20 km/h gust, 180°, 23.3 mm of rain, UV index 5, 1234 lx, transmission counter `tx`
static VevorDtxUnderTest::Payload sample_payload(uint8_t tx = 7) {
  VevorDtxUnderTest::Payload p{0xAA, 0x00, 0x12, 0x34, 0x80, 0x02, 0xCB, 55, 0x01, 0x54, 25,
                               0x01, 0xB5, 0x01, 0x65, 6,    0x05, 0xD3, tx, 0,    0};
  uint8_t checksum = 0;
  for (size_t i = 0; i < 19; i++) {
    checksum += p[i];
  }
  p[19] = checksum;
  return p;
}

// Run lengths of the NRZ bit stream for the sync bytes plus payload, framed by the inter-frame gap on both
// sides; `lead_bits` extra one bits in front shift the byte alignment the receiver sees
static remote_base::RawTimings timings(const VevorDtxUnderTest::Payload &payload, uint8_t lead_bits = 0) {
  std::vector<bool> bits(lead_bits, true);
  for (uint8_t byte : {0xAA, 0xAA, 0xCA, 0xCA, 0x54}) {
    for (int i = 7; i >= 0; i--) {
      bits.push_back(((byte >> i) & 1) != 0);
    }
  }
  for (uint8_t byte : payload) {
    for (int i = 7; i >= 0; i--) {
      bits.push_back(((byte >> i) & 1) != 0);
    }
  }
  remote_base::RawTimings out{FRAME_GAP_US};
  bool level = bits[0];
  uint32_t run = 0;
  for (bool bit : bits) {
    if (bit == level) {
      run++;
      continue;
    }
    out.push_back(level ? int32_t(run * BIT_TIME_US) : -int32_t(run * BIT_TIME_US));
    level = bit;
    run = 1;
  }
  out.push_back(level ? int32_t(run * BIT_TIME_US) : -int32_t(run * BIT_TIME_US));
  out.push_back(FRAME_GAP_US);
  return out;
}

static remote_base::RemoteReceiveData receive_data(const remote_base::RawTimings &raw) {
  return remote_base::RemoteReceiveData(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
}

TEST(VevorDtxTest, DecodesFrame) {
  VevorDtxUnderTest hub;
  VevorDtxUnderTest::Payload payload{};
  ASSERT_TRUE(hub.decode_timings_(timings(sample_payload()), payload));
  EXPECT_EQ(payload, sample_payload());
}

TEST(VevorDtxTest, DecodesFrameAtEveryBitOffset) {
  VevorDtxUnderTest hub;
  for (uint8_t lead = 1; lead < 8; lead++) {
    VevorDtxUnderTest::Payload payload{};
    ASSERT_TRUE(hub.decode_timings_(timings(sample_payload(), lead), payload)) << "lead bits " << int(lead);
    EXPECT_EQ(payload, sample_payload());
  }
}

TEST(VevorDtxTest, RejectsBadChecksum) {
  VevorDtxUnderTest hub;
  auto corrupted = sample_payload();
  corrupted[7] ^= 0x01;
  VevorDtxUnderTest::Payload payload{};
  EXPECT_FALSE(hub.decode_timings_(timings(corrupted), payload));
}

TEST(VevorDtxTest, PublishesDecodedValues) {
  VevorDtxUnderTest hub;
  sensor::Sensor temperature, humidity, wind_speed, wind_gust, wind_direction, rain, uv_index, illuminance, tx_counter;
  text_sensor::TextSensor sensor_id;
  binary_sensor::BinarySensor low_battery;
  hub.set_temperature_sensor(&temperature);
  hub.set_humidity_sensor(&humidity);
  hub.set_wind_speed_sensor(&wind_speed);
  hub.set_wind_gust_sensor(&wind_gust);
  hub.set_wind_direction_sensor(&wind_direction);
  hub.set_rain_sensor(&rain);
  hub.set_uv_index_sensor(&uv_index);
  hub.set_illuminance_sensor(&illuminance);
  hub.set_tx_counter_sensor(&tx_counter);
  hub.set_sensor_id_text_sensor(&sensor_id);
  hub.set_low_battery_binary_sensor(&low_battery);

  EXPECT_TRUE(hub.on_receive(receive_data(timings(sample_payload()))));

  EXPECT_NEAR(temperature.state, 21.5f, 0.01f);
  EXPECT_NEAR(humidity.state, 55.0f, 0.01f);
  EXPECT_NEAR(wind_speed.state, 9.96f / 3.6f, 0.01f);
  EXPECT_NEAR(wind_gust.state, 20.0f / 3.6f, 0.01f);
  EXPECT_NEAR(wind_direction.state, 180.0f, 0.01f);
  EXPECT_NEAR(rain.state, 23.3f, 0.01f);
  EXPECT_NEAR(uv_index.state, 5.0f, 0.01f);
  EXPECT_NEAR(illuminance.state, 1234.0f, 0.01f);
  EXPECT_NEAR(tx_counter.state, 7.0f, 0.01f);
  EXPECT_EQ(sensor_id.state, "0x1234");
  EXPECT_TRUE(low_battery.state);
}

TEST(VevorDtxTest, IgnoresOtherStations) {
  VevorDtxUnderTest hub;
  hub.set_station_id(0x9999);
  EXPECT_FALSE(hub.on_receive(receive_data(timings(sample_payload()))));
  hub.set_station_id(0x1234);
  EXPECT_TRUE(hub.on_receive(receive_data(timings(sample_payload()))));
}

TEST(VevorDtxTest, TxCounterKeepsCountingPastTheByteWrap) {
  VevorDtxUnderTest hub;
  sensor::Sensor tx_counter;
  hub.set_tx_counter_sensor(&tx_counter);
  EXPECT_TRUE(hub.on_receive(receive_data(timings(sample_payload(250)))));
  EXPECT_NEAR(tx_counter.state, 250.0f, 0.01f);
  EXPECT_TRUE(hub.on_receive(receive_data(timings(sample_payload(3)))));
  EXPECT_NEAR(tx_counter.state, 259.0f, 0.01f);
}

}  // namespace esphome::vevor_dtx::testing
