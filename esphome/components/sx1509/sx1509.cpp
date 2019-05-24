#include "sx1509.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509";

void SX1509Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SX1509ComponentComponent...");

  ESP_LOGV(TAG, "  Resetting devices...");
  this->write_byte(REG_RESET, 0x12);
  this->write_byte(REG_RESET, 0x34);

  // Communication test.
  uint16_t data;
  this->read_byte_16(REG_INTERRUPT_MASK_A, &data);
  if (data == 0xFF00) {
    clock(INTERNAL_CLOCK_2MHZ);
  } else {
    this->mark_failed();
    return;
  }

  for (auto *channel : this->float_output_channels_)
    channel->setup_channel();

  delayMicroseconds(500);

  this->loop();
}

void SX1509Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1509:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up SX1509 failed!");
  }
}

void SX1509Component::loop() {}

SX1509FloatOutputChannel *
SX1509Component::create_float_output_channel(uint8_t channel) {
  ESP_LOGD(TAG, "Set pin mode for channel %d", channel);
  auto *c = new SX1509FloatOutputChannel(this, channel);
  float_output_channels_.push_back(c);
  return c;
}

void SX1509Component::digitalWrite(uint8_t channel, uint8_t highLow) {
  uint16_t tempRegDir = 0;
  this->read_byte_16(REG_DIR_B, &tempRegDir);

  if ((0xFFFF ^ tempRegDir) &
      (1 << channel)) // If the pin is an output, write high/low
  {
    uint16_t tempRegData = 0;
    this->read_byte_16(REG_DATA_B, &tempRegData);
    if (highLow)
      tempRegData |= (1 << channel);
    else
      tempRegData &= ~(1 << channel);
    this->write_byte_16(REG_DATA_B, tempRegData);
  } else // Otherwise the pin is an input, pull-up/down
  {
    uint16_t tempPullUp;
    this->read_byte_16(REG_PULL_UP_B, &tempPullUp);
    uint16_t tempPullDown;
    this->read_byte_16(REG_PULL_DOWN_B, &tempPullDown);

    if (highLow) // if HIGH, do pull-up, disable pull-down
    {
      tempPullUp |= (1 << channel);
      tempPullDown &= ~(1 << channel);
      this->write_byte_16(REG_PULL_UP_B, tempPullUp);
      this->write_byte_16(REG_PULL_DOWN_B, tempPullDown);
    } else // If LOW do pull-down, disable pull-up
    {
      tempPullDown |= (1 << channel);
      tempPullUp &= ~(1 << channel);
      this->write_byte_16(REG_PULL_UP_B, tempPullUp);
      this->write_byte_16(REG_PULL_DOWN_B, tempPullDown);
    }
  }
}

void SX1509Component::pinMode(uint8_t channel, uint8_t inOut) {
  // The SX1509 RegDir registers: REG_DIR_B, REG_DIR_A
  //	0: IO is configured as an output
  //	1: IO is configured as an input
  uint8_t modeBit;
  if ((inOut == OUTPUT) || (inOut == ANALOG_OUTPUT))
    modeBit = 0;
  else
    modeBit = 1;

  uint16_t tempRegDir = 0;
  this->read_byte_16(REG_DIR_B, &tempRegDir);

  if (modeBit)
    tempRegDir |= (1 << channel);
  else
    tempRegDir &= ~(1 << channel);

  this->write_byte_16(REG_DIR_B, tempRegDir);

  // If INPUT_PULLUP was called, set up the pullup too:
  if (inOut == INPUT_PULLUP)
    digitalWrite(channel, HIGH);

  if (inOut == ANALOG_OUTPUT) {
    ledDriverInit(channel);
  }
}

void SX1509Component::ledDriverInit(uint8_t channel, uint8_t freq, bool log) {
  uint16_t tempWord;
  uint8_t tempByte;

  // Disable input buffer
  // Writing a 1 to the pin bit will disable that pins input buffer
  this->read_byte_16(REG_INPUT_DISABLE_B, &tempWord);
  tempWord |= (1 << channel);
  this->write_byte_16(REG_INPUT_DISABLE_B, tempWord);

  // Disable pull-up
  // Writing a 0 to the pin bit will disable that pull-up resistor
  this->read_byte_16(REG_PULL_UP_B, &tempWord);
  tempWord &= ~(1 << channel);
  this->write_byte_16(REG_PULL_UP_B, tempWord);

  // Set direction to output (REG_DIR_B)
  this->read_byte_16(REG_DIR_B, &tempWord);
  tempWord &= ~(1 << channel); // 0=output
  this->write_byte_16(REG_DIR_B, tempWord);

  // Enable oscillator (REG_CLOCK)
  this->read_byte(REG_CLOCK, &tempByte);
  tempByte |= (1 << 6);  // Internal 2MHz oscillator part 1 (set bit 6)
  tempByte &= ~(1 << 5); // Internal 2MHz oscillator part 2 (clear bit 5)
  this->write_byte(REG_CLOCK, tempByte);

  // Configure LED driver clock and mode (REG_MISC)
  this->read_byte(REG_MISC, &tempByte);
  if (log) {
    tempByte |= (1 << 7); // set logarithmic mode bank B
    tempByte |= (1 << 3); // set logarithmic mode bank A
  } else {
    tempByte &= ~(1 << 7); // set linear mode bank B
    tempByte &= ~(1 << 3); // set linear mode bank A
  }

  // Use configClock to setup the clock divder
  if (_clkX == 0) // Make clckX non-zero
  {
    _clkX = 2000000.0 / (1 << (1 - 1)); // Update private clock variable

    byte freq = (1 & 0x07) << 4; // freq should only be 3 bits from 6:4
    tempByte |= freq;
  }
  this->write_byte(REG_MISC, tempByte);

  // Enable LED driver operation (REG_LED_DRIVER_ENABLE)
  this->read_byte_16(REG_LED_DRIVER_ENABLE_B, &tempWord);
  tempWord |= (1 << channel);
  this->write_byte_16(REG_LED_DRIVER_ENABLE_B, tempWord);

  // Set REG_DATA bit low ~ LED driver started
  this->read_byte_16(REG_DATA_B, &tempWord);
  tempWord &= ~(1 << channel);
  this->write_byte_16(REG_DATA_B, tempWord);
}

void SX1509Component::clock(byte oscSource, byte oscPinFunction,
                            byte oscFreqOut, byte oscDivider) {
  oscSource = (oscSource & 0b11) << 5;        // 2-bit value, bits 6:5
  oscPinFunction = (oscPinFunction & 1) << 4; // 1-bit value bit 4
  oscFreqOut = (oscFreqOut & 0b1111);         // 4-bit value, bits 3:0
  byte regClock = oscSource | oscPinFunction | oscFreqOut;
  this->write_byte(REG_CLOCK, regClock);

  oscDivider = constrain(oscDivider, 1, 7);
  _clkX = 2000000.0 / (1 << (oscDivider - 1)); // Update private clock variable
  oscDivider = (oscDivider & 0b111) << 4;      // 3-bit value, bits 6:4

  uint8_t regMisc;
  this->read_byte(REG_MISC, &regMisc);
  regMisc &= ~(0b111 << 4);
  regMisc |= oscDivider;
  this->write_byte(REG_MISC, regMisc);
}

void SX1509Component::set_channel_value_(uint8_t channel, uint8_t iOn) {
  ESP_LOGD(TAG, "set_channel_value_ for channel %d to %d", channel, iOn);
  this->write_byte(REG_I_ON[channel], iOn);
}

void SX1509FloatOutputChannel::write_state(float state) {
  ESP_LOGD(TAG, "write_state %f", state);
  const uint16_t max_duty = 255;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint16_t>(duty_rounded);
  this->parent_->set_channel_value_(this->channel_, duty);
}

void SX1509FloatOutputChannel::setup_channel() {
  this->parent_->pinMode(this->channel_, ANALOG_OUTPUT);
}

} // namespace sx1509
} // namespace esphome
