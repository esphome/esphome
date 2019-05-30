#include "sx1509.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509";

void SX1509Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SX1509Component...");

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

  // for (auto *channel : this->float_output_channels_)
  //   channel->setup();

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

// SX1509FloatOutputChannel *SX1509Component::create_float_output_channel(uint8_t pin) {
//   ESP_LOGD(TAG, "Create float channel for pin %d", pin);
//   auto *channel = new SX1509FloatOutputChannel(this, pin);
//   float_output_channels_.push_back(channel);
//   return channel;
// }

uint8_t SX1509Component::digital_read(uint8_t pin) {
  uint16_t tempRegDir;
  this->read_byte_16(REG_DIR_B, &tempRegDir);

  if (tempRegDir & (1 << pin)) {
    uint16_t tempRegData;
    this->read_byte_16(REG_DATA_B, &tempRegData);
    if (tempRegData & (1 << pin))
      return 1;
  }
  return 0;
}

void SX1509Component::digital_write(uint8_t pin, uint8_t bit_value) {
  uint16_t temp_reg_dir = 0;
  this->read_byte_16(REG_DIR_B, &temp_reg_dir);

  if ((0xFFFF ^ temp_reg_dir) & (1 << pin))  // If the pin is an output, write high/low
  {
    uint16_t temp_reg_data = 0;
    this->read_byte_16(REG_DATA_B, &temp_reg_data);
    if (bit_value)
      temp_reg_data |= (1 << pin);
    else
      temp_reg_data &= ~(1 << pin);
    this->write_byte_16(REG_DATA_B, temp_reg_data);
  } else  // Otherwise the pin is an input, pull-up/down
  {
    uint16_t temp_pullup;
    this->read_byte_16(REG_PULL_UP_B, &temp_pullup);
    uint16_t temp_pull_down;
    this->read_byte_16(REG_PULL_DOWN_B, &temp_pull_down);

    if (bit_value)  // if HIGH, do pull-up, disable pull-down
    {
      temp_pullup |= (1 << pin);
      temp_pull_down &= ~(1 << pin);
      this->write_byte_16(REG_PULL_UP_B, temp_pullup);
      this->write_byte_16(REG_PULL_DOWN_B, temp_pull_down);
    } else  // If LOW do pull-down, disable pull-up
    {
      temp_pull_down |= (1 << pin);
      temp_pullup &= ~(1 << pin);
      this->write_byte_16(REG_PULL_UP_B, temp_pullup);
      this->write_byte_16(REG_PULL_DOWN_B, temp_pull_down);
    }
  }
}

void SX1509Component::pin_mode(uint8_t pin, uint8_t mode) {
  uint8_t mode_bit;
  if ((mode == OUTPUT) || (mode == ANALOG_OUTPUT) || (mode == BREATHE_OUTPUT))
    mode_bit = 0;
  else
    mode_bit = 1;

  uint16_t temp_reg_dir = 0;
  this->read_byte_16(REG_DIR_B, &temp_reg_dir);

  if (mode_bit)
    temp_reg_dir |= (1 << pin);
  else
    temp_reg_dir &= ~(1 << pin);

  this->write_byte_16(REG_DIR_B, temp_reg_dir);

  if (mode == INPUT_PULLUP)
    digital_write(pin, HIGH);

  if (mode == ANALOG_OUTPUT) {
    led_driver_init(pin);
  }

  if (mode == BREATHE_OUTPUT) {
    breathe(pin, 1000, 1000, 1000, 1000);
  }
}

void SX1509Component::led_driver_init(uint8_t pin, uint8_t freq, bool log) {
  uint16_t temp_word;
  uint8_t temp_byte;

  this->read_byte_16(REG_INPUT_DISABLE_B, &temp_word);
  temp_word |= (1 << pin);
  this->write_byte_16(REG_INPUT_DISABLE_B, temp_word);

  this->read_byte_16(REG_PULL_UP_B, &temp_word);
  temp_word &= ~(1 << pin);
  this->write_byte_16(REG_PULL_UP_B, temp_word);

  this->read_byte_16(REG_DIR_B, &temp_word);
  temp_word &= ~(1 << pin);  // 0=output
  this->write_byte_16(REG_DIR_B, temp_word);

  this->read_byte(REG_CLOCK, &temp_byte);
  temp_byte |= (1 << 6);   // Internal 2MHz oscillator part 1 (set bit 6)
  temp_byte &= ~(1 << 5);  // Internal 2MHz oscillator part 2 (clear bit 5)
  this->write_byte(REG_CLOCK, temp_byte);

  this->read_byte(REG_MISC, &temp_byte);
  if (log) {
    temp_byte |= (1 << 7);  // set logarithmic mode bank B
    temp_byte |= (1 << 3);  // set logarithmic mode bank A
  } else {
    temp_byte &= ~(1 << 7);  // set linear mode bank B
    temp_byte &= ~(1 << 3);  // set linear mode bank A
  }

  if (_clkX == 0)  // Make clckX non-zero
  {
    _clkX = 2000000.0 / (1 << (1 - 1));  // Update private clock variable

    uint8_t freq = (1 & 0x07) << 4;  // freq should only be 3 bits from 6:4
    temp_byte |= freq;
  }
  this->write_byte(REG_MISC, temp_byte);

  this->read_byte_16(REG_LED_DRIVER_ENABLE_B, &temp_word);
  temp_word |= (1 << pin);
  this->write_byte_16(REG_LED_DRIVER_ENABLE_B, temp_word);

  this->read_byte_16(REG_DATA_B, &temp_word);
  temp_word &= ~(1 << pin);
  this->write_byte_16(REG_DATA_B, temp_word);
}

void SX1509Component::breathe(uint8_t pin, unsigned long tOn, unsigned long tOff, unsigned long rise,
                              unsigned long fall, uint8_t onInt, uint8_t offInt, bool log) {
  offInt = constrain(offInt, 0, 7);

  uint8_t onReg = calculate_led_t_register(tOn);
  uint8_t offReg = calculate_led_t_register(tOff);

  uint8_t riseTime = calculate_slope_register(rise, onInt, offInt);
  uint8_t fallTime = calculate_slope_register(fall, onInt, offInt);

  setup_blink(pin, onReg, offReg, onInt, offInt, riseTime, fallTime, log);
}

void SX1509Component::setup_blink(uint8_t pin, uint8_t tOn, uint8_t tOff, uint8_t onIntensity, uint8_t offIntensity,
                                  uint8_t tRise, uint8_t tFall, bool log) {
  led_driver_init(pin, log);

  tOn &= 0x1F;   // tOn should be a 5-bit value
  tOff &= 0x1F;  // tOff should be a 5-bit value
  offIntensity &= 0x07;
  // Write the time on
  this->write_byte(REG_T_ON[pin], tOn);

  this->write_byte(REG_OFF[pin], (tOff << 3) | offIntensity);

  this->write_byte(REG_I_ON[pin], onIntensity);

  tRise &= 0x1F;
  tFall &= 0x1F;

  if (REG_T_RISE[pin] != 0xFF)
    this->write_byte(REG_T_RISE[pin], tRise);

  if (REG_T_FALL[pin] != 0xFF)
    this->write_byte(REG_T_FALL[pin], tFall);
}

void SX1509Component::clock(byte oscSource, byte oscPinFunction, byte oscFreqOut, byte oscDivider) {
  oscSource = (oscSource & 0b11) << 5;         // 2-bit value, bits 6:5
  oscPinFunction = (oscPinFunction & 1) << 4;  // 1-bit value bit 4
  oscFreqOut = (oscFreqOut & 0b1111);          // 4-bit value, bits 3:0
  byte regClock = oscSource | oscPinFunction | oscFreqOut;
  this->write_byte(REG_CLOCK, regClock);

  oscDivider = constrain(oscDivider, 1, 7);
  _clkX = 2000000.0 / (1 << (oscDivider - 1));  // Update private clock variable
  oscDivider = (oscDivider & 0b111) << 4;       // 3-bit value, bits 6:4

  uint8_t regMisc;
  this->read_byte(REG_MISC, &regMisc);
  regMisc &= ~(0b111 << 4);
  regMisc |= oscDivider;
  this->write_byte(REG_MISC, regMisc);
}

void SX1509Component::set_pin_value_(uint8_t pin, uint8_t iOn) {
  ESP_LOGD(TAG, "set_pin_value_ for pin %d to %d", pin, iOn);
  this->write_byte(REG_I_ON[pin], iOn);
}

uint8_t SX1509Component::calculate_led_t_register(uint16_t ms) {
  uint16_t regOn1, regOn2;
  float timeOn1, timeOn2;

  if (_clkX == 0)
    return 0;

  regOn1 = (float) (ms / 1000.0) / (64.0 * 255.0 / (float) _clkX);
  regOn2 = regOn1 / 8;
  regOn1 = constrain(regOn1, 1, 15);
  regOn2 = constrain(regOn2, 16, 31);

  timeOn1 = 64.0 * regOn1 * 255.0 / _clkX * 1000.0;
  timeOn2 = 512.0 * regOn2 * 255.0 / _clkX * 1000.0;

  if (abs(timeOn1 - ms) < abs(timeOn2 - ms))
    return regOn1;
  else
    return regOn2;
}

uint8_t SX1509Component::calculate_slope_register(uint16_t ms, uint8_t onIntensity, uint8_t offIntensity) {
  uint16_t regSlope1, regSlope2;
  float regTime1, regTime2;

  if (_clkX == 0)
    return 0;

  float tFactor = ((float) onIntensity - (4.0 * (float) offIntensity)) * 255.0 / (float) _clkX;
  float timeS = float(ms) / 1000.0;

  regSlope1 = timeS / tFactor;
  regSlope2 = regSlope1 / 16;

  regSlope1 = constrain(regSlope1, 1, 15);
  regSlope2 = constrain(regSlope2, 16, 31);

  regTime1 = regSlope1 * tFactor * 1000.0;
  regTime2 = 16 * regTime1;

  if (abs(regTime1 - ms) < abs(regTime2 - ms))
    return regSlope1;
  else
    return regSlope2;
}

void SX1509Component::setup_keypad(uint8_t rows, uint8_t columns, uint16_t sleepTime, uint8_t scanTime,
                                   uint8_t debounce_time) {
  uint16_t temp_word;
  uint8_t temp_byte;

  // If clock hasn't been set up, set it to internal 2MHz
  if (_clkX == 0)
    clock(INTERNAL_CLOCK_2MHZ);

  // Set regDir 0:7 outputs, 8:15 inputs:
  this->read_byte_16(REG_DIR_B, &temp_word);
  for (int i = 0; i < rows; i++)
    temp_word &= ~(1 << i);
  for (int i = 8; i < (columns * 2); i++)
    temp_word |= (1 << i);
  this->write_byte_16(REG_DIR_B, temp_word);

  // Set regOpenDrain on 0:7:
  this->read_byte(REG_OPEN_DRAIN_A, &temp_byte);
  for (int i = 0; i < rows; i++)
    temp_byte |= (1 << i);
  this->write_byte(REG_OPEN_DRAIN_A, temp_byte);

  // Set regPullUp on 8:15:
  this->read_byte(REG_PULL_UP_B, &temp_byte);
  for (int i = 0; i < columns; i++)
    temp_byte |= (1 << i);
  this->write_byte(REG_PULL_UP_B, temp_byte);

  // Debounce Time must be less than scan time
  debounce_time = constrain(debounce_time, 1, 64);
  scanTime = constrain(scanTime, 1, 128);
  if (debounce_time >= scanTime) {
    debounce_time = scanTime >> 1;  // Force debounce_time to be less than scanTime
  }
  debounce_keypad(debounce_time, rows, columns);

  // Calculate scanTimeBits, based on scanTime
  uint8_t scanTimeBits = 0;
  for (uint8_t i = 7; i > 0; i--) {
    if (scanTime & (1 << i)) {
      scanTimeBits = i;
      break;
    }
  }

  // Calculate sleepTimeBits, based on sleepTime
  uint8_t sleepTimeBits = 0;
  if (sleepTime != 0) {
    for (uint8_t i = 7; i > 0; i--) {
      if (sleepTime & ((unsigned int) 1 << (i + 6))) {
        sleepTimeBits = i;
        break;
      }
    }
    // If sleepTime was non-zero, but less than 128,
    // assume we wanted to turn sleep on, set it to minimum:
    if (sleepTimeBits == 0)
      sleepTimeBits = 1;
  }

  // RegKeyConfig1 sets the auto sleep time and scan time per row
  sleepTimeBits = (sleepTimeBits & 0b111) << 4;
  scanTimeBits &= 0b111;  // Scan time is bits 2:0
  temp_byte = sleepTime | scanTimeBits;
  this->write_byte(REG_KEY_CONFIG_1, temp_byte);

  // RegKeyConfig2 tells the SX1509 how many rows and columns we've got going
  rows = (rows - 1) & 0b111;        // 0 = off, 0b001 = 2 rows, 0b111 = 8 rows, etc.
  columns = (columns - 1) & 0b111;  // 0b000 = 1 column, ob111 = 8 columns, etc.
  this->write_byte(REG_KEY_CONFIG_2, (rows << 3) | columns);
}

void SX1509Component::debounce_config(uint8_t configValue) {
  // First make sure clock is configured
  uint8_t temp_byte;
  this->read_byte(REG_MISC, &temp_byte);
  if ((temp_byte & 0x70) == 0) {
    temp_byte |= (1 << 4);  // Just default to no divider if not set
    this->write_byte(REG_MISC, temp_byte);
  }
  this->read_byte(REG_CLOCK, &temp_byte);
  if ((temp_byte & 0x60) == 0) {
    temp_byte |= (1 << 6);  // default to internal osc.
    this->write_byte(REG_CLOCK, temp_byte);
  }

  configValue &= 0b111;  // 3-bit value
  this->write_byte(REG_DEBOUNCE_CONFIG, configValue);
}

void SX1509Component::debounce_time(uint8_t time) {
  if (_clkX == 0)                   // If clock hasn't been set up.
    clock(INTERNAL_CLOCK_2MHZ, 1);  // Set clock to 2MHz.

  // Debounce time-to-byte map: (assuming fOsc = 2MHz)
  // 0: 0.5ms		1: 1ms
  // 2: 2ms		3: 4ms
  // 4: 8ms		5: 16ms
  // 6: 32ms		7: 64ms
  // 2^(n-1)
  uint8_t configValue = 0;
  // We'll check for the highest set bit position,
  // and use that for debounce_config
  for (int i = 7; i >= 0; i--) {
    if (time & (1 << i)) {
      configValue = i + 1;
      break;
    }
  }
  configValue = constrain(configValue, 0, 7);

  debounce_config(configValue);
}

void SX1509Component::debounce_enable(uint8_t pin) {
  uint16_t debounce_enable;
  this->read_byte_16(REG_DEBOUNCE_ENABLE_B, &debounce_enable);
  debounce_enable |= (1 << pin);
  this->write_byte_16(REG_DEBOUNCE_ENABLE_B, debounce_enable);
}

void SX1509Component::debounce_pin(uint8_t pin) { debounce_enable(pin); }

void SX1509Component::debounce_keypad(uint8_t time, uint8_t numRows, uint8_t numCols) {
  // Set up debounce time:
  debounce_time(time);

  // Set up debounce pins:
  for (uint16_t i = 0; i < numRows; i++)
    debounce_pin(i);
  for (uint16_t i = 0; i < (8 + numCols); i++)
    debounce_pin(i);
}

}  // namespace sx1509
}  // namespace esphome
