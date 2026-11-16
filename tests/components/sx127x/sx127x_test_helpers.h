#pragma once
#include <gtest/gtest.h>
#include <array>
#include <functional>
#include <vector>
#include "esphome/components/sx127x/sx127x.h"
#include "esphome/core/application.h"

namespace esphome::sx127x::testing {

class TestPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_NONE; }
  size_t dump_summary(char *buffer, size_t len) const override { return snprintf(buffer, len, "test"); }
  bool digital_read() override { return this->on_read ? this->on_read() : this->level; }
  void digital_write(bool level) override {
    this->level = level;
    if (this->on_write) {
      this->on_write(level);
    }
  }

  bool level{false};
  std::function<bool()> on_read;
  std::function<void(bool)> on_write;
};

// An SX127x register file behind the real software SPI bus. The first byte of each
// transaction is the address, with bit 7 set for a write; FIFO reads and writes stream.
class RadioHardware {
 public:
  RadioHardware() {
    this->reset_();
    this->cs.level = true;
    this->cs.on_write = [this](bool high) {
      if (!high) {
        this->selected_ = true;
        this->pos_ = 0;
        this->bit_ = 0;
        this->byte_ = 0;
      } else {
        this->selected_ = false;
      }
    };
    this->miso.on_read = [this]() {
      EXPECT_TRUE(this->selected_);
      if (this->bit_ == 0) {
        this->reply_ = this->reply_byte_();
      }
      const bool bit = (this->reply_ >> (7 - this->bit_)) & 1;
      this->byte_ = (this->byte_ << 1) | this->mosi.level;
      if (++this->bit_ == 8) {
        this->byte_done_(this->byte_);
        this->byte_ = 0;
        this->bit_ = 0;
        this->pos_++;
      }
      return bit;
    };
    this->dio0.on_read = [this]() {
      this->finish_pending_send_();
      return this->dio0_level();
    };
    this->rst.on_write = [this](bool high) {
      if (!high) {
        this->reset_();
      }
    };
    this->spi.set_clk(&this->clk);
    this->spi.set_mosi(&this->mosi);
    this->spi.set_miso(&this->miso);
    // call() runs setup() and marks the bus set up, so it accepts devices
    this->spi.call();
    this->radio.set_spi_parent(&this->spi);
    this->radio.set_cs_pin(&this->cs);
    this->radio.set_dio0_pin(&this->dio0);
    this->radio.set_rst_pin(&this->rst);
    this->radio.set_frequency(433920000);
    this->radio.set_pa_pin(PA_PIN_BOOST);
    this->radio.set_pa_power(17);
    this->radio.set_pa_ramp(PA_RAMP_40);
    this->radio.set_spreading_factor(7);
    this->radio.set_coding_rate(CODING_RATE_4_5);
    this->radio.set_preamble_size(8);
    this->radio.set_crc_enable(true);
    this->radio.set_sync_value({0x12});
    this->radio.set_rx_floor(-94);  // FSK setup converts it to an int, so it must be set
  }
  ~RadioHardware() {
    // The configuration retry interval (id 0) must not outlive the radio
    App.scheduler.cancel_interval(&this->radio, uint32_t{0});
    this->radio.spi_teardown();
  }

  void configure(bool lora, bool rx, bool fixed = false) {
    this->radio.set_modulation(lora ? MOD_LORA : MOD_FSK);
    this->radio.set_bandwidth(lora ? SX127X_BW_125_0 : SX127X_BW_50_0);
    this->radio.set_bitrate(4800);
    this->radio.set_deviation(5000);
    this->radio.set_packet_mode(true);
    this->radio.set_rx_start(rx);
    this->radio.set_payload_length(fixed ? 3 : 0);
    this->radio.call();  // The normal component lifecycle runs setup().
  }

  bool lora() const { return (this->regs[REG_OP_MODE] & MOD_LORA) != 0; }
  uint8_t mode() const { return this->regs[REG_OP_MODE] & MODE_MASK; }
  bool receiving() const { return this->mode() == MODE_RX; }

  // A packet arrives, as the chip reports it in its current modulation
  void receive(const std::vector<uint8_t> &data, bool crc_error = false) {
    this->rx_packet = data;
    this->rx_pos_ = 0;
    if (this->lora()) {
      this->irq |= RX_DONE | (crc_error ? PAYLOAD_CRC_ERROR : 0);
    } else if (!crc_error) {
      this->payload_ready = true;
      // A variable length FSK packet starts with its length
      this->fsk_length_pending_ = (this->regs[REG_PACKET_CONFIG_1] & VARIABLE_LENGTH) != 0;
    }
  }

  bool dio0_level() const {
    if (this->lora()) {
      const uint8_t mapping = this->regs[REG_DIO_MAPPING1] & DIO0_MAPPING_11;
      return (this->irq & (mapping == DIO0_MAPPING_01 ? TX_DONE : RX_DONE)) != 0;
    }
    return this->mode() == MODE_TX ? this->packet_sent : this->payload_ready;
  }

  SX127x radio;
  spi::SPIComponent spi;
  TestPin cs, dio0, rst, clk, mosi, miso;
  std::array<uint8_t, 128> regs{};
  std::vector<uint8_t> rx_packet{0x11, 0x22, 0x33};
  std::vector<uint8_t> packet{0x11, 0x22, 0x33};
  std::vector<uint8_t> fifo;  // bytes written since the last send
  std::vector<std::vector<uint8_t>> sent;
  uint8_t irq{0};
  uint8_t version{0x12};
  bool payload_ready{false};
  bool packet_sent{false};
  // The chip completes a send as soon as it enters TX
  bool auto_complete{true};
  // Writes to the mode register are ignored, so set_mode_() times out
  bool stuck{false};

 protected:
  void reset_() {
    this->regs.fill(0);
    this->regs[REG_OP_MODE] = MODE_STDBY;
    this->irq = 0;
    this->payload_ready = false;
    this->packet_sent = false;
    this->fifo.clear();
  }

  uint8_t reply_byte_() {
    if (this->pos_ == 0) {
      return 0;
    }
    const uint8_t addr = this->addr_ & 0x7F;
    if (this->addr_ & 0x80) {
      return 0;
    }
    if (addr == REG_FIFO) {
      if (this->fsk_length_pending_) {
        this->fsk_length_pending_ = false;
        return this->rx_packet.size();
      }
      const uint8_t value = this->rx_pos_ < this->rx_packet.size() ? this->rx_packet[this->rx_pos_] : 0;
      this->rx_pos_++;
      if (!this->lora() && this->rx_pos_ >= this->rx_packet.size()) {
        this->payload_ready = false;
      }
      return value;
    }
    if (addr == REG_VERSION) {
      return this->version;
    }
    if (addr == REG_IRQ_FLAGS2 || (addr == REG_IRQ_FLAGS && this->lora())) {
      this->finish_pending_send_();
    }
    if (addr == REG_IRQ_FLAGS2) {
      return (this->payload_ready ? PAYLOAD_READY : 0) | (this->packet_sent ? PACKET_SENT : 0);
    }
    if (this->lora()) {
      switch (addr) {
        case REG_IRQ_FLAGS:
          return this->irq;
        case REG_NB_RX_BYTES:
          return this->rx_packet.size();
        case REG_FIFO_RX_CURR_ADDR:
          return 0;
        case REG_PKT_RSSI_VALUE:
          return 100;
        case REG_PKT_SNR_VALUE:
          return 20;
        default:
          break;
      }
    }
    return this->regs[addr];
  }

  void byte_done_(uint8_t value) {
    if (this->pos_ == 0) {
      this->addr_ = value;
      return;
    }
    if (!(this->addr_ & 0x80)) {
      return;
    }
    const uint8_t addr = this->addr_ & 0x7F;
    if (addr == REG_FIFO) {
      this->fifo.push_back(value);
      return;
    }
    if (addr == REG_OP_MODE) {
      this->write_mode_(value);
      return;
    }
    if (addr == REG_IRQ_FLAGS && this->lora()) {
      this->irq &= ~value;
      return;
    }
    if (addr == REG_FIFO_ADDR_PTR) {
      this->rx_pos_ = 0;
    }
    this->regs[addr] = value;
  }

  void write_mode_(uint8_t value) {
    if (this->stuck) {
      return;
    }
    const uint8_t previous = this->mode();
    const uint8_t mode = value & MODE_MASK;
    // As on an SX1276, a send that is still going out can be stopped by standby or sleep only
    if (previous == MODE_TX && !this->send_done_() && mode != MODE_STDBY && mode != MODE_SLEEP) {
      return;
    }
    this->regs[REG_OP_MODE] = value;
    if (previous == MODE_TX && mode != MODE_TX) {
      this->packet_sent = false;
      this->send_pending_ = false;  // Stopped before it finished
    }
    if (mode == MODE_SLEEP) {
      this->payload_ready = false;
    }
    if (mode == MODE_TX && previous != MODE_TX) {
      this->sent.push_back(this->fifo);
      this->fifo.clear();
      // Completes on the next poll, not before the mode reads back as TX
      this->send_pending_ = this->auto_complete;
    }
  }

  bool send_done_() const { return this->lora() ? (this->irq & TX_DONE) != 0 : this->packet_sent; }

  void finish_pending_send_() {
    if (this->send_pending_ && this->mode() == MODE_TX) {
      this->complete_send();
    }
    this->send_pending_ = false;
  }

 public:
  // In LoRa the chip returns to standby by itself; in FSK it stays in TX
  void complete_send() {
    if (this->lora()) {
      this->irq |= TX_DONE;
      this->regs[REG_OP_MODE] = (this->regs[REG_OP_MODE] & ~MODE_MASK) | MODE_STDBY;
    } else {
      this->packet_sent = true;
    }
  }

 protected:
  size_t pos_{0};
  size_t rx_pos_{0};
  uint8_t addr_{0};
  uint8_t bit_{0};
  uint8_t byte_{0};
  uint8_t reply_{0};
  bool selected_{false};
  bool fsk_length_pending_{false};
  bool send_pending_{false};
};

class PacketListener : public SX127xListener {
 public:
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override {
    this->packets.push_back(packet);
    this->rssi = rssi;
    this->snr = snr;
    if (this->on_receive) {
      this->on_receive();
    }
  }
  std::vector<std::vector<uint8_t>> packets;
  float rssi{};
  float snr{};
  std::function<void()> on_receive;
};

class TransmitListener : public SX127xTransmitListener {
 public:
  void on_transmit_complete(SX127xError result) override {
    this->results.push_back(result);
    if (this->on_complete) {
      this->on_complete(result);
    }
  }
  std::vector<SX127xError> results;
  std::function<void(SX127xError)> on_complete;
};

// Runs the scheduler for ms milliseconds of real time, as the main loop would
inline void run_scheduler(uint32_t ms) {
  const uint32_t start = millis();
  while (millis() - start < ms) {
    App.scheduler.call(millis());
    delay(5);
  }
}

}  // namespace esphome::sx127x::testing
