#include <gtest/gtest.h>

#include <functional>
#include <tuple>
#include <vector>

#include "esphome/components/sx126x/sx126x.h"

namespace esphome::sx126x::testing {

class TestPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_NONE; }
  size_t dump_summary(char *buffer, size_t len) const override { return snprintf(buffer, len, "test"); }
  bool digital_read() override {
    this->reads++;
    return this->on_read ? this->on_read() : this->level;
  }
  void digital_write(bool level) override {
    this->level = level;
    if (this->on_write) {
      this->on_write(level);
    }
  }
  bool is_internal() override { return this->internal; }

  bool level{false};
  bool internal{false};
  unsigned reads{0};
  std::function<bool()> on_read;
  std::function<void(bool)> on_write;
};

// Exercise the real software SPI adapter and driver through GPIO pins. Each CS
// assertion starts a command; BUSY can reject any one of those commands.
class RadioHardware {
 public:
  RadioHardware() {
    this->cs.level = true;
    this->cs.on_write = [this](bool high) {
      if (!high) {
        EXPECT_FALSE(this->selected_);
        this->selected_ = true;
        this->transactions.emplace_back();
        this->bit_ = 0;
        this->byte_ = 0;
        this->busy.level = this->transactions.size() == this->fail_at;
      } else if (this->selected_) {
        this->selected_ = false;
        this->ends++;
        const auto &command = this->transactions.back();
        if (command.empty()) {
          return;
        }
        switch (command[0]) {
          case RADIO_SET_PACKETPARAMS:
            this->packet_params = command;
            break;
          case RADIO_CLR_IRQSTATUS:
            this->irq &= ~((uint16_t(command[1]) << 8) | command[2]);
            break;
          case RADIO_SET_TX:
            this->sends++;
            if (this->auto_complete) {
              this->irq |= IRQ_TX_DONE;
            }
            break;
          case RADIO_SET_RX:
            this->receiving = true;
            break;
          case RADIO_SET_SLEEP:
          case RADIO_SET_STANDBY:
            this->receiving = false;
            break;
          default:
            break;
        }
      }
    };
    this->busy.on_read = [this]() {
      EXPECT_TRUE(this->selected_);  // NSS must wake the chip before waiting.
      return this->busy.level;
    };
    this->miso.on_read = [this]() {
      EXPECT_TRUE(this->selected_);
      EXPECT_FALSE(this->busy.level);  // No clocks/data may follow a BUSY timeout.
      auto &command = this->transactions.back();
      const bool bit = (this->reply_byte_(command) >> (7 - this->bit_)) & 1;
      this->byte_ = (this->byte_ << 1) | this->mosi.level;
      if (++this->bit_ == 8) {
        command.push_back(this->byte_);
        this->byte_ = 0;
        this->bit_ = 0;
      }
      return bit;
    };
    this->dio.on_read = [this]() { return this->irq != 0; };
    this->spi.set_clk(&this->clk);
    this->spi.set_mosi(&this->mosi);
    this->spi.set_miso(&this->miso);
    this->spi.setup();
    this->radio.set_spi_parent(&this->spi);
    this->radio.set_cs_pin(&this->cs);
    this->radio.set_busy_pin(&this->busy);
    this->radio.set_dio1_pin(&this->dio);
    this->radio.set_rst_pin(&this->rst);
    this->radio.set_frequency(915000000);
    this->radio.set_tcxo_voltage(TCXO_CTRL_1_8V);
    this->radio.set_rf_switch(true);
    this->radio.set_crc_enable(true);
    this->radio.set_sync_value({0x14, 0x24});
  }
  ~RadioHardware() { this->radio.spi_teardown(); }

  void configure(bool lora, bool rx, bool fixed = false) {
    this->radio.set_modulation(lora ? PACKET_TYPE_LORA : PACKET_TYPE_GFSK);
    this->radio.set_bandwidth(lora ? SX126X_BW_125000 : SX126X_BW_4800);
    this->radio.set_bitrate(4800);
    this->radio.set_rx_start(rx);
    this->radio.set_payload_length(fixed ? 3 : 0);
    this->radio.call();  // The normal component lifecycle runs setup().
  }

  void clear_trace() {
    this->transactions.clear();
    this->ends = 0;
    this->fail_at = 0;
    this->busy.level = false;
    this->dio.reads = 0;
  }

  void expect_aborted(size_t command) {
    ASSERT_EQ(this->transactions.size(), command);
    EXPECT_TRUE(this->transactions.back().empty());
    EXPECT_EQ(this->ends, command);
    EXPECT_TRUE(this->cs.level);
  }

  SX126x radio;
  spi::SPIComponent spi;
  TestPin cs, busy, dio, rst, clk, mosi, miso;
  std::vector<std::vector<uint8_t>> transactions;
  std::vector<uint8_t> packet{0x11, 0x22, 0x33};
  std::vector<uint8_t> packet_params;
  size_t fail_at{0};
  size_t ends{0};
  unsigned sends{0};
  uint16_t irq{0};
  bool receiving{false};
  bool auto_complete{true};

 protected:
  uint8_t reply_byte_(const std::vector<uint8_t> &command) {
    if (command.empty()) {
      return 0;
    }
    const size_t pos = command.size();
    switch (command[0]) {
      case RADIO_READ_REGISTER:
        if (pos >= 4) {
          static constexpr char VERSION[16] = "SX1262";
          return VERSION[pos - 4];
        }
        break;
      case RADIO_GET_IRQSTATUS:
        if (pos == 2) {
          return this->irq >> 8;
        }
        if (pos == 3) {
          return this->irq & 0xFF;
        }
        break;
      case RADIO_GET_PACKETSTATUS:
        if (pos == 2) {
          return 80;
        }
        if (pos == 3) {
          return 40;
        }
        if (pos == 4) {
          return 84;
        }
        break;
      case RADIO_GET_RXBUFFERSTATUS:
        if (pos == 2) {
          return this->packet.size();
        }
        if (pos == 3) {
          return 7;
        }
        break;
      case RADIO_READ_BUFFER:
        if (pos >= 3) {
          EXPECT_EQ(command[1], 7);
          return this->packet[pos - 3];
        }
        break;
      default:
        break;
    }
    return 0;
  }

  bool selected_{false};
  uint8_t bit_{0};
  uint8_t byte_{0};
};

class PacketListener : public SX126xListener {
 public:
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override {
    this->packets.push_back(packet);
    this->rssi = rssi;
    this->snr = snr;
  }
  std::vector<std::vector<uint8_t>> packets;
  float rssi{};
  float snr{};
};

class SX126xBusySetup : public ::testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(SX126xBusySetup, StopsAtEveryConfigurationCommand) {
  const auto [lora, rx] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx);
  ASSERT_FALSE(reference.radio.is_failed());
  ASSERT_FALSE(reference.radio.status_has_warning());
  const size_t count = reference.transactions.size();
  ASSERT_GT(count, 10u);
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hardware;
    hardware.fail_at = fault;
    hardware.configure(lora, rx);
    hardware.expect_aborted(fault);
    EXPECT_TRUE(hardware.radio.is_failed());
    hardware.radio.dump_config();  // Also safe if the version read never happened.
    hardware.clear_trace();
    hardware.radio.configure();
    hardware.radio.set_mode_rx();
    hardware.radio.set_mode_tx();
    hardware.radio.set_mode_sleep();
    hardware.radio.set_mode_standby(STDBY_XOSC);
    hardware.radio.run_image_cal();
    hardware.radio.loop();
    EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::SX126X_ERROR_NOT_READY);
    EXPECT_TRUE(hardware.transactions.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(LoRaAndFSKWithRxOrSleep, SX126xBusySetup,
                         ::testing::Combine(::testing::Bool(), ::testing::Bool()));

class SX126xBusyTransmit : public ::testing::TestWithParam<std::tuple<bool, bool, bool>> {};

TEST_P(SX126xBusyTransmit, StopsAtEverySendAndCleanupCommandAndCanRetry) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx, fixed);
  reference.clear_trace();
  ASSERT_EQ(reference.radio.transmit_packet(reference.packet), SX126xError::NONE);
  const size_t count = reference.transactions.size();
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hardware;
    hardware.configure(lora, rx, fixed);
    hardware.clear_trace();
    hardware.fail_at = fault;
    EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet.data(), hardware.packet.size()),
              SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
    EXPECT_TRUE(hardware.transactions[fault - 1].empty());
    EXPECT_TRUE(hardware.cs.level);
    EXPECT_EQ(hardware.ends, hardware.transactions.size());
    EXPECT_FALSE(hardware.radio.is_failed());
    if (hardware.sends == 0) {
      EXPECT_EQ(hardware.dio.reads, 0u);  // Never enter the 4-second TX wait after a failed start.
    }
    hardware.clear_trace();
    hardware.radio.configure();
    EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::NONE);
    EXPECT_FALSE(hardware.radio.status_has_warning());
    EXPECT_EQ(hardware.receiving, rx);
  }
}

TEST_P(SX126xBusyTransmit, RestoresEveryPreparationFailureWithoutAnotherTransmission) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx, fixed);
  reference.clear_trace();
  ASSERT_EQ(reference.radio.transmit_packet(reference.packet), SX126xError::NONE);
  size_t tx_command = 0;
  for (size_t i = 0; i < reference.transactions.size(); i++) {
    if (reference.transactions[i][0] == RADIO_SET_TX) {
      tx_command = i + 1;
      break;
    }
  }
  ASSERT_NE(tx_command, 0u);
  for (size_t fault = 1; fault <= tx_command; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    PacketListener listener;
    hw.configure(lora, rx, fixed);
    hw.radio.register_listener(&listener);
    const auto configured_params = hw.packet_params;
    hw.clear_trace();
    hw.fail_at = fault;
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
    EXPECT_TRUE(hw.transactions[fault - 1].empty());
    EXPECT_TRUE(hw.cs.level);
    EXPECT_EQ(hw.ends, hw.transactions.size());
    EXPECT_EQ(hw.receiving, rx);
    EXPECT_EQ(hw.packet_params, configured_params);
    EXPECT_EQ(hw.irq, 0u);
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_FALSE(hw.radio.is_failed());
    const auto sends = hw.sends;
    hw.clear_trace();
    if (rx) {
      if (!fixed) {
        hw.packet.resize(9, 0x55);  // RX must accept a different length than the attempted TX.
      }
      hw.irq = IRQ_RX_DONE;
      hw.radio.loop();
      ASSERT_EQ(listener.packets.size(), 1u);
      EXPECT_EQ(listener.packets[0], hw.packet);
    }
    EXPECT_EQ(hw.sends, sends);
  }
}

TEST_P(SX126xBusyTransmit, FailedRestorationRequiresConfigureBeforeReceiveRecovery) {
  const auto [lora, rx, fixed] = GetParam();
  RadioHardware reference;
  reference.configure(lora, rx, fixed);
  reference.clear_trace();
  ASSERT_EQ(reference.radio.transmit_packet(reference.packet), SX126xError::NONE);
  size_t restoration = 0;
  bool sent = false;
  for (size_t i = 0; i < reference.transactions.size(); i++) {
    if (reference.transactions[i][0] == RADIO_SET_TX) {
      sent = true;
    }
    if (sent && reference.transactions[i][0] == RADIO_SET_STANDBY) {
      restoration = i + 1;
      break;
    }
  }
  ASSERT_NE(restoration, 0u);
  for (size_t fault = restoration; fault <= reference.transactions.size(); fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    PacketListener listener;
    hw.configure(lora, rx, fixed);
    hw.radio.register_listener(&listener);
    const auto configured_params = hw.packet_params;
    hw.clear_trace();
    hw.fail_at = fault;
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
    EXPECT_TRUE(hw.cs.level);
    EXPECT_TRUE(hw.radio.status_has_warning());
    EXPECT_FALSE(hw.radio.is_failed());
    const auto sends = hw.sends;
    hw.clear_trace();  // Clearing BUSY alone must not re-admit partial configuration.
    hw.irq = IRQ_RX_DONE;
    hw.radio.loop();
    hw.radio.set_mode_rx();
    hw.radio.set_mode_tx();
    hw.radio.set_mode_standby(STDBY_XOSC);
    hw.radio.set_mode_sleep();
    hw.radio.run_image_cal();
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    EXPECT_TRUE(hw.transactions.empty());
    EXPECT_TRUE(listener.packets.empty());
    EXPECT_TRUE(hw.radio.status_has_warning());
    hw.irq = 0;  // Hardware reset clears IRQs; the GPIO fake does not model reset.
    hw.radio.configure();
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_EQ(hw.packet_params, configured_params);
    EXPECT_EQ(hw.receiving, rx);
    if (rx) {
      hw.irq = IRQ_RX_DONE;
      hw.radio.loop();
      ASSERT_EQ(listener.packets.size(), 1u);
      EXPECT_EQ(listener.packets[0], hw.packet);
    }
    EXPECT_EQ(hw.sends, sends);  // Recovery must not depend on sending another packet.
  }
}

TEST(SX126xBusy, PersistentBusyDuringPreparationAndRestorationRequiresConfigure) {
  RadioHardware hw;
  PacketListener listener;
  hw.configure(true, true);
  hw.radio.register_listener(&listener);
  const auto configured_params = hw.packet_params;
  hw.clear_trace();
  auto normal_busy = hw.busy.on_read;
  hw.busy.on_read = [&]() { return hw.transactions.size() >= 3 || normal_busy(); };
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
  EXPECT_FALSE(hw.receiving);
  EXPECT_TRUE(hw.radio.status_has_warning());
  EXPECT_EQ(hw.sends, 0u);
  hw.busy.on_read = normal_busy;
  hw.clear_trace();
  hw.radio.set_mode_rx();
  hw.radio.run_image_cal();
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(hw.radio.status_has_warning());
  hw.radio.configure();
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.packet_params, configured_params);
  hw.irq = IRQ_RX_DONE;
  hw.radio.loop();
  ASSERT_EQ(listener.packets.size(), 1u);
  EXPECT_EQ(listener.packets[0], hw.packet);
  EXPECT_EQ(hw.sends, 0u);
}

INSTANTIATE_TEST_SUITE_P(LoRaAndFSKWithRxOrSleepAndFixedOrVariablePayload, SX126xBusyTransmit,
                         ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Bool()));

class SX126xBusyReceive : public ::testing::TestWithParam<bool> {};

TEST_P(SX126xBusyReceive, DropsFailedReadsAndRecoversOnNextPacket) {
  const bool lora = GetParam();
  for (size_t fault = 1; fault <= 5; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hardware;
    PacketListener listener;
    hardware.configure(lora, true);
    hardware.radio.register_listener(&listener);
    hardware.clear_trace();
    hardware.irq = IRQ_RX_DONE;
    hardware.fail_at = fault;
    hardware.radio.loop();
    hardware.expect_aborted(fault);
    EXPECT_TRUE(listener.packets.empty());
    EXPECT_TRUE(hardware.radio.status_has_warning());
    EXPECT_FALSE(hardware.radio.is_failed());
    hardware.clear_trace();
    hardware.irq = IRQ_RX_DONE;
    hardware.radio.loop();
    ASSERT_EQ(listener.packets.size(), 1u);
    EXPECT_EQ(listener.packets[0], hardware.packet);
    EXPECT_FLOAT_EQ(listener.rssi, lora ? -40 : -42);
    EXPECT_FLOAT_EQ(listener.snr, lora ? 10 : 0);
    EXPECT_FALSE(hardware.radio.status_has_warning());
  }
}

INSTANTIATE_TEST_SUITE_P(LoRaAndFSK, SX126xBusyReceive, ::testing::Bool());

TEST(SX126xBusy, KeepsPollingAnUnclearedInterruptAfterBusyTimeout) {
  RadioHardware hardware;
  PacketListener listener;
  hardware.configure(true, true);
  hardware.radio.register_listener(&listener);
  hardware.dio.internal = true;  // No new ISR edge while the pending IRQ stays high.
  hardware.clear_trace();
  hardware.fail_at = 1;
  hardware.irq = IRQ_RX_DONE;
  hardware.radio.call();
  EXPECT_TRUE(listener.packets.empty());
  hardware.clear_trace();
  hardware.radio.call();
  EXPECT_EQ(listener.packets.size(), 1u);
}

TEST(SX126xBusy, AssertsChipSelectBeforeWaitingForWakeup) {
  RadioHardware hardware;
  hardware.configure(true, true);
  hardware.clear_trace();
  hardware.busy.level = true;  // The CS callback releases BUSY only after selecting the chip.
  EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::NONE);
  EXPECT_TRUE(hardware.cs.level);
  EXPECT_EQ(hardware.ends, hardware.transactions.size());
}

TEST(SX126xBusy, RejectsSendBeforeSetupWithoutTouchingPins) {
  RadioHardware hardware;
  EXPECT_EQ(hardware.radio.transmit_packet(hardware.packet), SX126xError::SX126X_ERROR_NOT_READY);
  EXPECT_TRUE(hardware.transactions.empty());
  EXPECT_EQ(hardware.busy.reads, 0u);
  EXPECT_EQ(hardware.dio.reads, 0u);
}

TEST(SX126xBusy, FailedReconfigurationRequiresSuccessfulConfigureBeforeOperations) {
  RadioHardware reference;
  reference.configure(true, true);
  const size_t count = reference.transactions.size();
  for (size_t fault = 1; fault <= count; fault++) {
    SCOPED_TRACE(fault);
    RadioHardware hw;
    PacketListener listener;
    hw.configure(true, true);
    hw.radio.register_listener(&listener);
    hw.clear_trace();
    hw.fail_at = fault;
    hw.radio.configure();
    EXPECT_FALSE(hw.radio.is_failed());
    hw.clear_trace();
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    hw.irq = IRQ_RX_DONE;
    hw.radio.loop();
    hw.radio.set_mode_rx();
    hw.radio.set_mode_tx();
    hw.radio.set_mode_standby(STDBY_XOSC);
    hw.radio.set_mode_sleep();
    hw.radio.run_image_cal();
    EXPECT_TRUE(hw.transactions.empty());
    EXPECT_TRUE(listener.packets.empty());
    hw.radio.configure();
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  }
}

TEST(SX126xBusy, RetryClearsStaleTxDoneBeforeWaitingForDelayedCompletion) {
  RadioHardware reference;
  reference.configure(true, true);
  reference.clear_trace();
  reference.radio.transmit_packet(reference.packet);
  size_t cleanup = 0;
  bool sent = false;
  for (size_t i = 0; i < reference.transactions.size(); i++) {
    if (reference.transactions[i][0] == RADIO_SET_TX) {
      sent = true;
    }
    if (sent && reference.transactions[i][0] == RADIO_CLR_IRQSTATUS) {
      cleanup = i + 1;
      break;
    }
  }
  ASSERT_NE(cleanup, 0u);
  RadioHardware hw;
  hw.configure(true, true);
  hw.clear_trace();
  hw.fail_at = cleanup;
  ASSERT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY_TIMEOUT);
  ASSERT_NE(hw.irq & IRQ_TX_DONE, 0);
  hw.clear_trace();
  hw.radio.configure();
  hw.irq = IRQ_TX_DONE;  // A stale flag must not finish the next transmission.
  hw.auto_complete = false;
  unsigned polls = 0;
  hw.dio.on_read = [&]() {
    if (++polls == 5) {
      hw.irq |= IRQ_TX_DONE;
    }
    return hw.irq != 0;
  };
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  EXPECT_EQ(polls, 5u);
  EXPECT_TRUE(hw.receiving);
}

}  // namespace esphome::sx126x::testing

namespace esphome::sx126x::testing {
template<typename T>
concept RawSpiAccessible = requires(T &device) { device.transfer_byte(0); };
template<typename T>
concept ManualSelectionAccessible = requires(T &device) {
  device.enable();
  device.disable();
};
static_assert(!RawSpiAccessible<SX126x>);
static_assert(!RawSpiAccessible<Sx126xSpiDevice>);
static_assert(!ManualSelectionAccessible<SX126x>);
static_assert(!ManualSelectionAccessible<Sx126xSpiDevice>);
}  // namespace esphome::sx126x::testing

namespace esphome::sx126x::testing {
TEST(SX126xBusy, FailedModeChangeCannotBeClearedByAnotherModeCommand) {
  for (size_t fault = 1; fault <= 3; fault++) {
    RadioHardware hw;
    hw.configure(true, true);
    hw.clear_trace();
    hw.fail_at = fault;
    hw.radio.set_mode_rx();
    EXPECT_TRUE(hw.radio.status_has_warning());
    hw.clear_trace();
    hw.radio.set_mode_standby(STDBY_XOSC);
    hw.radio.run_image_cal();
    EXPECT_TRUE(hw.transactions.empty());
    EXPECT_TRUE(hw.radio.status_has_warning());
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
    hw.radio.configure();
    EXPECT_FALSE(hw.radio.status_has_warning());
    EXPECT_TRUE(hw.receiving);
  }
}

TEST(SX126xBusy, ConfigurationCannotReenterSendPreparationOrRestoration) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.clear_trace();
  auto write = hw.cs.on_write;
  unsigned guarded = 0;
  hw.cs.on_write = [&](bool high) {
    write(high);
    if (!high || hw.transactions.back().empty() || hw.transactions.back()[0] != RADIO_SET_STANDBY) {
      return;
    }
    guarded++;
    const auto commands = hw.transactions.size();
    hw.radio.configure();
    hw.radio.set_mode_rx();
    hw.radio.loop();
    EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_BUSY);
    EXPECT_EQ(hw.transactions.size(), commands);
  };
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  EXPECT_EQ(guarded, 2u);
  EXPECT_TRUE(hw.receiving);
}
}  // namespace esphome::sx126x::testing

namespace esphome::sx126x::testing {
TEST(SX126xBusy, InvalidPayloadDoesNotEnterAnOperation) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.clear_trace();
  EXPECT_EQ(hw.radio.transmit_packet(nullptr, 3), SX126xError::INVALID_PARAMS);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet.data(), 0), SX126xError::INVALID_PARAMS);
  EXPECT_TRUE(hw.transactions.empty());
  EXPECT_TRUE(hw.receiving);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
}

TEST(SX126xBusy, ColdSleepRequiresConfigurationAndWarmSleepRetainsIt) {
  RadioHardware hw;
  hw.configure(true, true);
  hw.radio.set_mode_sleep(false);
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
  hw.radio.set_mode_sleep(true);
  hw.clear_trace();
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::SX126X_ERROR_NOT_READY);
  hw.radio.set_mode_rx();
  EXPECT_TRUE(hw.transactions.empty());
  hw.radio.configure();
  EXPECT_EQ(hw.radio.transmit_packet(hw.packet), SX126xError::NONE);
}
}  // namespace esphome::sx126x::testing
