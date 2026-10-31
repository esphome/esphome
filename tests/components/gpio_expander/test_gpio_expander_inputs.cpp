#include <gtest/gtest.h>

#include "esphome/components/gpio_expander/cached_gpio_component.h"

namespace esphome::gpio_expander::testing {

// Reads one bank of the fake chip per digital_read_hw() call.
template<typename T, uint16_t N> class FakeInputsExpander : public GpioExpanderInputs<CachedGpioExpander<T, N>> {
 public:
  static constexpr size_t BANKS = CachedGpioExpander<T, N>::BANKS;

  void keep_cache() { this->set_invalidate_on_read_(false); }

  T chip[BANKS]{};
  unsigned reads[BANKS]{};
  bool fail{false};

 protected:
  bool digital_read_hw(uint8_t pin) override {
    const uint8_t bank = pin / CachedGpioExpander<T, N>::BANK_SIZE;
    this->reads[bank]++;
    if (this->fail)
      return false;
    this->input_[bank] = this->chip[bank];
    return true;
  }
  void digital_write_hw(uint8_t pin, bool value) override {}
};

class FakeInputsComponent : public CachedGpioExpanderComponent<uint8_t, 16> {
 public:
  void setup() override { this->setup_interrupt_pin_(); }

  uint8_t chip[2]{};
  unsigned reads{0};

 protected:
  bool digital_read_hw(uint8_t pin) override {
    const uint8_t bank = pin / 8;
    this->reads++;
    this->input_[bank] = this->chip[bank];
    return true;
  }
  void digital_write_hw(uint8_t pin, bool value) override {}
};

class GpioExpanderInputsTest : public ::testing::Test {
 protected:
  FakeInputsExpander<uint8_t, 16> expander_;
};

TEST_F(GpioExpanderInputsTest, ReturnsEachPinOfEachBank) {
  this->expander_.chip[0] = 0xA5;
  this->expander_.chip[1] = 0x3C;
  for (uint8_t pin = 0; pin < 16; pin++)
    EXPECT_EQ(this->expander_.digital_read(pin), bool(0x3CA5 & (1 << pin))) << "pin " << unsigned(pin);
  EXPECT_EQ(this->expander_.reads[0], 1u);
  EXPECT_EQ(this->expander_.reads[1], 1u);
}

TEST_F(GpioExpanderInputsTest, ReadingOneBankKeepsTheOther) {
  this->expander_.chip[0] = 0xFF;
  this->expander_.chip[1] = 0x0F;
  EXPECT_TRUE(this->expander_.digital_read(0));
  EXPECT_TRUE(this->expander_.digital_read(8));

  // Pin 8 was invalidated by its read, so only bank 1 is read again.
  this->expander_.chip[0] = 0x00;
  this->expander_.chip[1] = 0xF0;
  EXPECT_FALSE(this->expander_.digital_read(8));
  EXPECT_TRUE(this->expander_.digital_read(12));
  EXPECT_TRUE(this->expander_.digital_read(1));
  EXPECT_EQ(this->expander_.reads[0], 1u);
  EXPECT_EQ(this->expander_.reads[1], 2u);
}

TEST_F(GpioExpanderInputsTest, FailedReadReturnsLowAndKeepsOtherPinsCached) {
  this->expander_.chip[0] = 0x03;
  EXPECT_TRUE(this->expander_.digital_read(0));

  this->expander_.fail = true;
  this->expander_.chip[0] = 0x00;
  EXPECT_FALSE(this->expander_.digital_read(0));
  EXPECT_TRUE(this->expander_.digital_read(1));
}

TEST_F(GpioExpanderInputsTest, KeepCacheUntilReset) {
  this->expander_.keep_cache();
  this->expander_.chip[0] = 0x01;
  EXPECT_TRUE(this->expander_.digital_read(0));
  this->expander_.chip[0] = 0x00;
  EXPECT_TRUE(this->expander_.digital_read(0));
  EXPECT_EQ(this->expander_.reads[0], 1u);
}

TEST(GpioExpanderInputs, OneBankReadInOneTransfer) {
  FakeInputsExpander<uint16_t, 16> expander;
  expander.chip[0] = 0x8001;
  EXPECT_TRUE(expander.digital_read(0));
  EXPECT_FALSE(expander.digital_read(1));
  EXPECT_FALSE(expander.digital_read(8));
  EXPECT_TRUE(expander.digital_read(15));
  EXPECT_EQ(expander.reads[0], 1u);
}

TEST(GpioExpanderInputs, WideBanks) {
  FakeInputsExpander<uint16_t, 32> expander;
  expander.chip[0] = 0x8001;
  expander.chip[1] = 0x4002;
  EXPECT_TRUE(expander.digital_read(0));
  EXPECT_TRUE(expander.digital_read(15));
  EXPECT_FALSE(expander.digital_read(14));
  EXPECT_TRUE(expander.digital_read(17));
  EXPECT_TRUE(expander.digital_read(30));
  EXPECT_FALSE(expander.digital_read(31));
  EXPECT_EQ(expander.reads[0], 1u);
  EXPECT_EQ(expander.reads[1], 1u);
}

TEST(GpioExpanderInputsComponent, WithoutAnInterruptPinEachReadInvalidatesItsPin) {
  FakeInputsComponent component;
  component.setup();
  component.chip[0] = 0x01;
  EXPECT_TRUE(component.digital_read(0));
  component.chip[0] = 0x00;
  EXPECT_FALSE(component.digital_read(0));
  EXPECT_EQ(component.reads, 2u);
}

TEST(GpioExpanderInputsComponent, InterruptPinKeepsEveryBankUntilTheLoopRuns) {
  InternalGPIOPin int_pin;
  int_pin.set_inverted(true);  // INT released
  FakeInputsComponent component;
  component.set_interrupt_pin(&int_pin);
  component.setup();
  component.chip[0] = 0x01;
  component.chip[1] = 0x80;
  EXPECT_TRUE(component.digital_read(0));
  EXPECT_TRUE(component.digital_read(15));

  component.chip[0] = 0x00;
  component.chip[1] = 0x00;
  EXPECT_TRUE(component.digital_read(0));
  EXPECT_TRUE(component.digital_read(15));

  component.loop();
  EXPECT_FALSE(component.digital_read(0));
  EXPECT_FALSE(component.digital_read(15));
}

}  // namespace esphome::gpio_expander::testing
