#include <gtest/gtest.h>

#include <array>

#include "esphome/components/pcm1863/pcm1863.h"
#include "esphome/components/select/select.h"

namespace esphome::pcm1863::testing {

static constexpr uint8_t ADDRESS = 0x4A;

// The body codegen emits for the input select with publish="on_success".
static PCM1863 *adc = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void apply_input(select::Select *entity, size_t index) {
  if (adc->set_input(static_cast<PCM1863Input>(index)))
    entity->publish_state(index);
}

// Page 0 registers written as register/value pairs; failing writes are not acknowledged.
class RegisterBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t * /*out*/,
                             size_t /*out_size*/) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size == 2)
      this->registers[data[0]] = data[1];
    return i2c::ERROR_OK;
  }

  std::array<uint8_t, 256> registers{};
  bool fail{false};
};

class PCM1863InputSelectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->adc_.set_i2c_bus(&this->bus_);
    this->adc_.set_i2c_address(ADDRESS);
    adc = &this->adc_;
    this->select_.traits.set_options({"NONE", "VIN1", "VIN2", "VIN3", "VIN4", "DIFF_1", "DIFF_2"});
    this->select_.add_on_state_callback([this](size_t) { ++this->publishes_; });
  }

  void choose_(size_t index) { this->select_.make_call().set_index(index).perform(); }

  RegisterBus bus_;
  PCM1863 adc_;
  select::ApplySelect<apply_input> select_;
  unsigned publishes_{0};
};

TEST_F(PCM1863InputSelectTest, EveryOptionWritesItsMuxBitsToBothChannels) {
  this->adc_.setup();
  ASSERT_FALSE(this->adc_.is_failed());
  static constexpr std::array<uint8_t, 7> MUX = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
  for (size_t index = 0; index < MUX.size(); index++) {
    this->choose_(index);
    // Bit 6 is reserved and always written as 1
    EXPECT_EQ(this->bus_.registers[PCM1863_REG_ADC1_INPUT_SEL_L], PCM1863_INPUT_SEL_RSV | MUX[index]);
    EXPECT_EQ(this->bus_.registers[PCM1863_REG_ADC1_INPUT_SEL_R], PCM1863_INPUT_SEL_RSV | MUX[index]);
    EXPECT_EQ(this->select_.active_index(), index);
  }
  EXPECT_EQ(this->publishes_, MUX.size());
}

TEST_F(PCM1863InputSelectTest, FailedWriteLeavesTheSelectUnpublished) {
  this->adc_.setup();
  this->bus_.fail = true;
  this->choose_(2);
  EXPECT_EQ(this->publishes_, 0u);
  EXPECT_FALSE(this->select_.has_state());
}

TEST_F(PCM1863InputSelectTest, FailedSetupRejectsTheSelect) {
  this->bus_.fail = true;
  this->adc_.setup();
  ASSERT_TRUE(this->adc_.is_failed());
  this->bus_.fail = false;
  this->choose_(3);
  EXPECT_EQ(this->publishes_, 0u);
  EXPECT_FALSE(this->select_.has_state());
  EXPECT_EQ(this->bus_.registers[PCM1863_REG_ADC1_INPUT_SEL_L], 0);
}

}  // namespace esphome::pcm1863::testing
