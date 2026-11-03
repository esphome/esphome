#pragma once
#include <gtest/gtest.h>
#include <functional>
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
  bool supported_version{true};

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
          return this->supported_version ? VERSION[pos - 4] : 0;
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

}  // namespace esphome::sx126x::testing
