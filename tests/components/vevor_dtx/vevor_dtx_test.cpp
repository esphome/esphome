#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/vevor_dtx/vevor_dtx.h"
#include "esphome/core/helpers.h"

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
  p[19] = sum_bytes(p.data(), 19);
  return p;
}

// Run lengths of the NRZ bit stream for the sync bytes plus payload, framed by the inter-frame gap on both
// sides; `lead_bits` extra one bits in front shift the byte alignment the receiver sees
static remote_base::RawTimings timings(const VevorDtxUnderTest::Payload &payload, uint8_t lead_bits = 0) {
  std::vector<bool> bits(lead_bits, true);
  const auto push_byte = [&bits](uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
      bits.push_back(((byte >> i) & 1) != 0);
    }
  };
  for (uint8_t byte : {0xAA, 0xAA, 0xCA, 0xCA, 0x54}) {
    push_byte(byte);
  }
  for (uint8_t byte : payload) {
    push_byte(byte);
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

// A payload may hold a run of zeros longer than any inter-bit gap; it must not be taken for a frame gap
TEST(VevorDtxTest, DecodesAFrameWithALongZeroRun) {
  auto payload = sample_payload(0);
  payload[7] = 49;
  payload[16] = 0x02;
  payload[17] = 0x00;
  payload[20] = 1;
  payload[19] = sum_bytes(payload.data(), 19);
  ASSERT_EQ(payload[19], 0);  // 32 zero bits in a row across bytes 17 to 20
  VevorDtxUnderTest hub;
  VevorDtxUnderTest::Payload decoded{};
  ASSERT_TRUE(hub.decode_timings_(timings(payload), decoded));
  EXPECT_EQ(decoded[7], 49);
  EXPECT_EQ(decoded[19], 0);
}

// The receiver ends its capture at idle, so the frame's trailing low bits merge into one long gap
TEST(VevorDtxTest, DecodesAFrameWhoseTailMergesIntoTheIdleGap) {
  // A counter value whose checksum ends in zero bits, so the frame's last bits are low
  auto payload = sample_payload();
  for (uint8_t tx = 0;; tx++) {
    payload[18] = tx;
    payload[19] = sum_bytes(payload.data(), 19);
    if ((payload[19] & 0x0F) == 0)
      break;
  }
  auto raw = timings(payload);
  // Drop the trailing zero run and the gap, and end the capture in one idle entry instead
  raw.pop_back();
  if (raw.back() < 0)
    raw.pop_back();
  raw.push_back(FRAME_GAP_US);
  VevorDtxUnderTest hub;
  VevorDtxUnderTest::Payload decoded{};
  ASSERT_TRUE(hub.decode_timings_(raw, decoded));
  EXPECT_EQ(decoded[19], payload[19]);
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

// A byte that wrapped past 0xFF and a light word with the tenfold bit set
TEST(VevorDtxTest, UndoesTheOffsetPerByte) {
  VevorDtxUnderTest hub;
  sensor::Sensor rain, illuminance;
  hub.set_rain_sensor(&rain);
  hub.set_illuminance_sensor(&illuminance);
  auto payload = sample_payload();
  payload[13] = 0x01;  // rain 255: the low byte is 255 + 1 wrapped to 0
  payload[14] = 0x00;
  payload[16] = 0x80;  // light 0x7F00 = 32512 lx without the tenfold bit
  payload[17] = 0x01;
  payload[19] = sum_bytes(payload.data(), 19);
  EXPECT_TRUE(hub.on_receive(receive_data(timings(payload))));
  EXPECT_NEAR(rain.state, 255 * 0.233f, 0.01f);
  EXPECT_NEAR(illuminance.state, 32512.0f, 0.01f);
}

TEST(VevorDtxTest, DecodesAFrameAfterNoise) {
  VevorDtxUnderTest hub;
  VevorDtxUnderTest::Payload payload{};
  // A noise burst, the gap, then the frame: the gap resets the search and the frame still decodes
  remote_base::RawTimings raw{FRAME_GAP_US, 300, -500, 1000, -176};
  const auto frame = timings(sample_payload());
  raw.insert(raw.end(), frame.begin(), frame.end());
  ASSERT_TRUE(hub.decode_timings_(raw, payload));
  EXPECT_EQ(payload, sample_payload());
}

TEST(VevorDtxTest, IgnoresOtherStations) {
  VevorDtxUnderTest hub;
  hub.set_station_id(0x9999);
  EXPECT_FALSE(hub.on_receive(receive_data(timings(sample_payload()))));
  hub.set_station_id(0x1234);
  EXPECT_TRUE(hub.on_receive(receive_data(timings(sample_payload()))));
}

TEST(VevorDtxTest, TxCounterIsTheStationsByte) {
  VevorDtxUnderTest hub;
  sensor::Sensor tx_counter;
  hub.set_tx_counter_sensor(&tx_counter);
  EXPECT_TRUE(hub.on_receive(receive_data(timings(sample_payload(250)))));
  EXPECT_NEAR(tx_counter.state, 250.0f, 0.01f);
  EXPECT_TRUE(hub.on_receive(receive_data(timings(sample_payload(3)))));
  EXPECT_NEAR(tx_counter.state, 3.0f, 0.01f);
}

}  // namespace esphome::vevor_dtx::testing
