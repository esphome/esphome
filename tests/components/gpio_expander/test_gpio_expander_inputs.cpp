#include <gtest/gtest.h>

#include "esphome/components/gpio_expander/cached_gpio.h"

namespace esphome::gpio_expander::testing {

// Reads one bank of the fake chip per digital_read_hw() call.
template<typename T, uint16_t N> class FakeInputsExpander : public GpioExpanderInputs<CachedGpioExpander<T, N>> {
  using Base = GpioExpanderInputs<CachedGpioExpander<T, N>>;

 public:
  using Base::BANKS;

  void keep_cache() { this->set_invalidate_on_read_(false); }
  void reset() { this->reset_pin_cache_(); }

  T chip[BANKS]{};
  unsigned reads[BANKS]{};
  bool fail{false};

 protected:
  bool digital_read_hw(uint8_t pin) override {
    const uint8_t bank = pin / this->BANK_SIZE;
    this->reads[bank]++;
    if (this->fail)
      return false;
    this->input_[bank] = this->chip[bank];
    return true;
  }
  void digital_write_hw(uint8_t pin, bool value) override {}
};

// Every pin returns its bit of the packed chip state, with one read per bank.
template<typename T, uint16_t N> void expect_every_pin(uint32_t packed) {
  FakeInputsExpander<T, N> expander;
  constexpr uint8_t bank_size = sizeof(T) * 8;
  for (size_t bank = 0; bank < expander.BANKS; bank++)
    expander.chip[bank] = packed >> (bank * bank_size);
  for (uint8_t pin = 0; pin < N; pin++)
    EXPECT_EQ(expander.digital_read(pin), bool((packed >> pin) & 1)) << "pin " << unsigned(pin);
  for (size_t bank = 0; bank < expander.BANKS; bank++)
    EXPECT_EQ(expander.reads[bank], 1u) << "bank " << bank;
}

TEST(GpioExpanderInputs, ReturnsEveryPin) {
  expect_every_pin<uint8_t, 16>(0x3CA5);
  expect_every_pin<uint16_t, 16>(0x8001);  // one bank read in one transfer
  expect_every_pin<uint16_t, 32>(0x40028001);
}

TEST(GpioExpanderInputs, ReadingOneBankKeepsTheOther) {
  FakeInputsExpander<uint8_t, 16> expander;
  expander.chip[0] = 0xFF;
  expander.chip[1] = 0x0F;
  EXPECT_TRUE(expander.digital_read(0));
  EXPECT_TRUE(expander.digital_read(8));

  // Pin 8 was invalidated by its read, so only bank 1 is read again.
  expander.chip[0] = 0x00;
  expander.chip[1] = 0xF0;
  EXPECT_FALSE(expander.digital_read(8));
  EXPECT_TRUE(expander.digital_read(12));
  EXPECT_TRUE(expander.digital_read(1));
  EXPECT_EQ(expander.reads[0], 1u);
  EXPECT_EQ(expander.reads[1], 2u);
}

TEST(GpioExpanderInputs, FailedReadReturnsLowAndKeepsOtherPinsCached) {
  FakeInputsExpander<uint8_t, 16> expander;
  expander.chip[0] = 0x03;
  EXPECT_TRUE(expander.digital_read(0));

  expander.fail = true;
  expander.chip[0] = 0x00;
  EXPECT_FALSE(expander.digital_read(0));
  EXPECT_TRUE(expander.digital_read(1));
}

TEST(GpioExpanderInputs, KeepCacheUntilReset) {
  FakeInputsExpander<uint8_t, 16> expander;
  expander.keep_cache();
  expander.chip[0] = 0x01;
  EXPECT_TRUE(expander.digital_read(0));
  expander.chip[0] = 0x00;
  EXPECT_TRUE(expander.digital_read(0));
  EXPECT_EQ(expander.reads[0], 1u);

  expander.reset();
  EXPECT_FALSE(expander.digital_read(0));
  EXPECT_EQ(expander.reads[0], 2u);
}

}  // namespace esphome::gpio_expander::testing
