#include "sx126x_test_helpers.h"
#include "esphome/components/sx126x/packet_transport/sx126x_transport.h"
#include "esphome/components/host/preferences.h"
#include "esphome/core/application.h"

namespace esphome::sx126x::testing {

class TransportCompletion : public SX126xTransmitListener {
 public:
  void on_transmit_complete(SX126xError result) override { this->completions++; }
  unsigned completions{0};
};

class SX126xTransportRetry : public ::testing::TestWithParam<int> {};

TEST_P(SX126xTransportRetry, SensorAndPingSurviveBusyUntilRadioAcceptsThem) {
  RadioHardware hw;
  hw.configure(true, true);
  App.pre_setup("radio_test", 10, "", 0);
  host::HostPreferences prefs;
  auto *previous_preferences = global_preferences;
  global_preferences = &prefs;
  SX126xTransport transport;
  sensor::Sensor sensor;
  sensor.state = 42.0f;
  transport.set_parent(&hw.radio);
  transport.set_sensor_count(1);
  transport.add_sensor("temperature", &sensor);
  transport.set_is_provider(true);
  transport.set_ping_pong_enable(true);
  transport.set_ping_pong_recycle_time(60000);
  transport.setup();
  global_preferences = previous_preferences;

  TransportCompletion completion;
  auto write = hw.cs.on_write;
  bool inject = GetParam() == 2;
  hw.cs.on_write = [&](bool high) {
    write(high);
    if (high && inject && !hw.transactions.back().empty() && hw.transactions.back()[0] == RADIO_SET_STANDBY) {
      inject = false;
      hw.irq = IRQ_RX_DONE;
    }
  };
  if (GetParam() == 0) {
    hw.auto_complete = false;
    ASSERT_EQ(hw.radio.start_transmit_async(&completion, hw.packet), SX126xError::NONE);
  } else if (GetParam() == 1) {
    hw.irq = IRQ_RX_DONE;
  }
  const auto initial_sends = hw.sends;
  transport.update();
  transport.loop();
  EXPECT_EQ(hw.sends, initial_sends);
  if (GetParam() == 0) {
    hw.irq = IRQ_TX_DONE;
  }
  hw.radio.loop();
  hw.auto_complete = true;
  hw.clear_trace();
  transport.loop();
  EXPECT_EQ(hw.sends, initial_sends + 2);  // Retained ping and sensor update.
  std::vector<std::vector<uint8_t>> packets;
  for (const auto &cmd : hw.transactions) {
    if (!cmd.empty() && cmd[0] == RADIO_WRITE_BUFFER) {
      packets.emplace_back(cmd.begin() + 2, cmd.end());
    }
  }
  ASSERT_EQ(packets.size(), 2u);
  EXPECT_EQ(packets[0][0], 0x48);  // PacketTransport MAGIC_PING, little endian.
  EXPECT_EQ(packets[0][1], 0x50);
  EXPECT_EQ(packets[1][0], 0x53);  // PacketTransport MAGIC_NUMBER.
  EXPECT_EQ(packets[1][1], 0x45);
  const std::vector<uint8_t> encoded_value{0x00, 0x00, 0x28, 0x42};
  EXPECT_NE(std::search(packets[1].begin(), packets[1].end(), encoded_value.begin(), encoded_value.end()),
            packets[1].end());
  transport.loop();
  EXPECT_EQ(hw.sends, initial_sends + 2);
}

INSTANTIATE_TEST_SUITE_P(AsyncTxPendingRxAndStandbyRace, SX126xTransportRetry, ::testing::Values(0, 1, 2));

}  // namespace esphome::sx126x::testing
