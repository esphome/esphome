#include <gtest/gtest.h>

#include <deque>
#include <vector>

#include "esphome/components/weikai/weikai.h"

namespace esphome::weikai::testing {

/// State of a fake WeiKai channel: the bytes of the receive FIFO and the bytes written to the transmit FIFO.
struct FakeChip {
  std::deque<uint8_t> rx_fifo;
  std::vector<uint8_t> tx_written;
  int fsr_reads{0};
};

class FakeRegister : public WeikaiRegister {
 public:
  FakeRegister(WeikaiComponent *comp, FakeChip *chip) : WeikaiRegister(comp, 0, 0), chip_(chip) {}

  uint8_t read_reg() const override {
    switch (this->register_) {
      case WKREG_FSR:
        this->chip_->fsr_reads++;
        return this->chip_->rx_fifo.empty() ? 0 : FSR_RFDAT;
      case WKREG_RFCNT:
        return static_cast<uint8_t>(this->chip_->rx_fifo.size());  // 256 bytes read as 0
      case WKREG_LCR:
        return this->lcr_;
      default:
        return 0;
    }
  }
  void write_reg(uint8_t value) override {
    if (this->register_ == WKREG_LCR)
      this->lcr_ = value;
  }
  void read_fifo(uint8_t *data, size_t length) const override {
    for (size_t i = 0; i < length; i++) {
      data[i] = this->chip_->rx_fifo.front();
      this->chip_->rx_fifo.pop_front();
    }
  }
  void write_fifo(uint8_t *data, size_t length) override {
    this->chip_->tx_written.insert(this->chip_->tx_written.end(), data, data + length);
  }

 protected:
  FakeChip *chip_;
  uint8_t lcr_{0};  // read back by the channel test done in setup_channel()
};

class FakeComponent : public WeikaiComponent {
 public:
  FakeComponent() : reg_(this, &this->chip) { this->set_crystal(14745600); }
  WeikaiRegister &reg(uint8_t reg, uint8_t channel) override {
    this->reg_.register_ = reg;
    this->reg_.channel_ = channel;
    return this->reg_;
  }
  FakeChip chip;

 protected:
  FakeRegister reg_;
};

class WeikaiChannelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->channel_.set_parent(&this->comp_);
    this->channel_.set_channel(0);
    this->channel_.set_channel_name("test");
    this->channel_.set_baud_rate(115200);
    this->channel_.setup_channel();
  }
  FakeComponent comp_;
  WeikaiChannel channel_;
};

TEST_F(WeikaiChannelTest, ReceivesFullFifoInOrder) {
  for (int i = 0; i < 256; i++)
    this->comp_.chip.rx_fifo.push_back(i);
  EXPECT_EQ(this->channel_.available(), 256u);  // RFCNT reads 0 for 256 bytes
  std::vector<uint8_t> data(256);
  EXPECT_TRUE(this->channel_.read_array(data.data(), data.size()));
  for (int i = 0; i < 256; i++)
    EXPECT_EQ(data[i], i);
  EXPECT_EQ(this->channel_.available(), 0u);
}

TEST_F(WeikaiChannelTest, IdleChannelCostsOneStatusRead) {
  this->comp_.chip.fsr_reads = 0;
  EXPECT_EQ(this->channel_.available(), 0u);
  EXPECT_EQ(this->comp_.chip.fsr_reads, 1);
}

TEST_F(WeikaiChannelTest, FullBufferLeavesTheRestInTheFifo) {
  for (int i = 0; i < 256; i++)
    this->comp_.chip.rx_fifo.push_back(i);
  EXPECT_EQ(this->channel_.available(), 256u);
  for (int i = 0; i < 10; i++)  // arrives while the ring buffer is full
    this->comp_.chip.rx_fifo.push_back(200 + i);
  std::vector<uint8_t> data(100);
  EXPECT_TRUE(this->channel_.read_array(data.data(), data.size()));
  EXPECT_EQ(this->channel_.available(), 156u);
  EXPECT_EQ(this->comp_.chip.rx_fifo.size(), 10u);  // not read yet: the buffer was not empty
  std::vector<uint8_t> rest(156);
  EXPECT_TRUE(this->channel_.read_array(rest.data(), rest.size()));
  EXPECT_EQ(rest.front(), 100);
  EXPECT_EQ(this->channel_.available(), 10u);  // the buffer is empty: the FIFO is read again
  uint8_t first{};
  EXPECT_TRUE(this->channel_.peek_byte(&first));
  EXPECT_EQ(first, 200);
}

TEST_F(WeikaiChannelTest, ReadingMoreThanAvailableFails) {
  this->comp_.chip.rx_fifo.push_back(1);
  EXPECT_EQ(this->channel_.available(), 1u);
  uint8_t data[4] = {};
  EXPECT_FALSE(this->channel_.read_array(data, sizeof(data)));
  EXPECT_EQ(data[0], 1);
}

TEST_F(WeikaiChannelTest, LongWriteIsSentInChunks) {
  std::vector<uint8_t> out(300);
  for (size_t i = 0; i < out.size(); i++)
    out[i] = i;
  this->channel_.write_array(out.data(), out.size());
  EXPECT_EQ(this->comp_.chip.tx_written, out);
}

TEST_F(WeikaiChannelTest, BaudRateTooHighIsReduced) {
  this->channel_.set_baud_rate(10000000);
  this->channel_.setup_channel();
  EXPECT_EQ(this->channel_.get_baud_rate(), 14745600u / 16);
}

TEST_F(WeikaiChannelTest, BaudRateTooLowReportsTheRateReallyUsed) {
  this->channel_.set_baud_rate(1);
  this->channel_.setup_channel();
  EXPECT_EQ(this->channel_.get_baud_rate(), 14u);  // 14745600 / (16 * 65536.9)
}

}  // namespace esphome::weikai::testing
