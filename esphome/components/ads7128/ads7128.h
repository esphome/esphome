#pragma once

// Texas Instruments ADS7128 Small, 8-Channel, 12-Bit ADC With I2C Interface, GPIOs, and CRC
// Datasheet: https://www.ti.com/lit/ds/symlink/ads7128.pdf

#include <deque>

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ads7128 {

enum ADS7128Oversamping {
  ADS7128_OVERSAMPLING_1 = 0b000,
  ADS7128_OVERSAMPLING_2 = 0b001,
  ADS7128_OVERSAMPLING_4 = 0b010,
  ADS7128_OVERSAMPLING_8 = 0b011,
  ADS7128_OVERSAMPLING_16 = 0b100,
  ADS7128_OVERSAMPLING_32 = 0b101,
  ADS7128_OVERSAMPLING_64 = 0b110,
  // ADS7128_OVERSAMPLING_128 = 0b111,
};

enum ADS7128CycleTime {
  ADS7128_CYCLE_TIME_1 = 0b00000,
  ADS7128_CYCLE_TIME_1P5 = 0b00001,
  ADS7128_CYCLE_TIME_2 = 0b00010,
  ADS7128_CYCLE_TIME_3 = 0b00011,
  ADS7128_CYCLE_TIME_4 = 0b00100,
  ADS7128_CYCLE_TIME_6 = 0b00101,
  ADS7128_CYCLE_TIME_8 = 0b00110,
  ADS7128_CYCLE_TIME_12 = 0b00111,
  ADS7128_CYCLE_TIME_16 = 0b01000,
  ADS7128_CYCLE_TIME_24 = 0b01001,
  ADS7128_CYCLE_TIME_32 = 0b10000,
  ADS7128_CYCLE_TIME_48 = 0b10001,
  ADS7128_CYCLE_TIME_64 = 0b10010,
  ADS7128_CYCLE_TIME_96 = 0b10011,
  ADS7128_CYCLE_TIME_128 = 0b10100,
  ADS7128_CYCLE_TIME_192 = 0b10101,
  ADS7128_CYCLE_TIME_256 = 0b10110,
  ADS7128_CYCLE_TIME_384 = 0b10111,
  ADS7128_CYCLE_TIME_512 = 0b11000,
  ADS7128_CYCLE_TIME_768 = 0b11001,
  ADS7128_CYCLE_TIME_1024 = 0b11010,
  ADS7128_CYCLE_TIME_1536 = 0b11011,
  ADS7128_CYCLE_TIME_2048 = 0b11100,
  ADS7128_CYCLE_TIME_3072 = 0b11101,
  ADS7128_CYCLE_TIME_4096 = 0b11110,
  ADS7128_CYCLE_TIME_6144 = 0b11111,
};

enum ADS7128RmsSamples {
  ADS7128_RMS_SAMPLES_1024 = 0b00,
  ADS7128_RMS_SAMPLES_4096 = 0b01,
  ADS7128_RMS_SAMPLES_16384 = 0b10,
  ADS7128_RMS_SAMPLES_65536 = 0b11,
};

class ADS7128Sensor;

// Core ADS7128 device class
class ADS7128Component : public Component, public i2c::I2CDevice {
 public:
  ADS7128Component() = default;

  float get_setup_priority() const override { return setup_priority::IO; }

  void setup() override;
  void digital_setup(uint8_t pin, gpio::Flags flags);

  void dump_config() override;

  void loop() override;

  bool digital_read(uint8_t pin) { return (this->gpi_value_ & (1 << pin)) != 0; }
  void digital_write(uint8_t pin, bool value);
  void sensor_update(ADS7128Sensor *sensor);

 protected:
  // Last-fetched state of GPI pins
  bool any_gpi_ = false;
  uint8_t gpi_value_ = 0;

  // Analog sensors to process
  std::deque<ADS7128Sensor *> sensors_;

  // Current analog read state
  bool is_waiting_ = false;
  uint32_t wait_time_ms_ = 0;

  // Register constants
  enum class Register : uint8_t {
    SYSTEM_STATUS = 0x00,
    GENERAL_CFG = 0x01,
    DATA_CFG = 0x02,
    OSR_CFG = 0x03,
    OPMODE_CFG = 0x04,
    PIN_CFG = 0x05,
    GPIO_CFG = 0x07,
    GPO_DRIVE_CFG = 0x09,
    GPO_VALUE = 0x0B,
    GPI_VALUE = 0x0D,
    ZCD_BLANKING_CFG = 0x0F,
    SEQUENCE_CFG = 0x10,
    CHANNEL_SEL = 0x11,
    AUTO_SEQ_CH_SEL = 0x12,
    ALERT_CH_SEL = 0x14,
    ALERT_MAP = 0x16,
    ALERT_PIN_CFG = 0x17,
    EVENT_FLAG = 0x18,
    EVENT_HIGH_FLAG = 0x1A,
    EVENT_LOW_FLAG = 0x1C,
    EVENT_RGN = 0x1E,
    HYSTERESIS_CH0 = 0x20,
    HIGH_TH_CH0 = 0x21,
    EVENT_COUNT_CH0 = 0x22,
    LOW_TH_CH0 = 0x23,
    HYSTERESIS_CH1 = 0x24,
    HIGH_TH_CH1 = 0x25,
    EVENT_COUNT_CH1 = 0x26,
    LOW_TH_CH1 = 0x27,
    HYSTERESIS_CH2 = 0x28,
    HIGH_TH_CH2 = 0x29,
    EVENT_COUNT_CH2 = 0x2A,
    LOW_TH_CH2 = 0x2B,
    HYSTERESIS_CH3 = 0x2C,
    HIGH_TH_CH3 = 0x2D,
    EVENT_COUNT_CH3 = 0x2E,
    LOW_TH_CH3 = 0x2F,
    HYSTERESIS_CH4 = 0x30,
    HIGH_TH_CH4 = 0x31,
    EVENT_COUNT_CH4 = 0x32,
    LOW_TH_CH4 = 0x33,
    HYSTERESIS_CH5 = 0x34,
    HIGH_TH_CH5 = 0x35,
    EVENT_COUNT_CH5 = 0x36,
    LOW_TH_CH5 = 0x37,
    HYSTERESIS_CH6 = 0x38,
    HIGH_TH_CH6 = 0x39,
    EVENT_COUNT_CH6 = 0x3A,
    LOW_TH_CH6 = 0x3B,
    HYSTERESIS_CH7 = 0x3C,
    HIGH_TH_CH7 = 0x3D,
    EVENT_COUNT_CH7 = 0x3E,
    LOW_TH_CH7 = 0x3F,
    MAX_CH0_LSB = 0x60,
    MAX_CH0_MSB = 0x61,
    MAX_CH1_LSB = 0x62,
    MAX_CH1_MSB = 0x63,
    MAX_CH2_LSB = 0x64,
    MAX_CH2_MSB = 0x65,
    MAX_CH3_LSB = 0x66,
    MAX_CH3_MSB = 0x67,
    MAX_CH4_LSB = 0x68,
    MAX_CH4_MSB = 0x69,
    MAX_CH5_LSB = 0x6A,
    MAX_CH5_MSB = 0x6B,
    MAX_CH6_LSB = 0x6C,
    MAX_CH6_MSB = 0x6D,
    MAX_CH7_LSB = 0x6E,
    MAX_CH7_MSB = 0x6F,
    MIN_CH0_LSB = 0x80,
    MIN_CH0_MSB = 0x81,
    MIN_CH1_LSB = 0x82,
    MIN_CH1_MSB = 0x83,
    MIN_CH2_LSB = 0x84,
    MIN_CH2_MSB = 0x85,
    MIN_CH3_LSB = 0x86,
    MIN_CH3_MSB = 0x87,
    MIN_CH4_LSB = 0x88,
    MIN_CH4_MSB = 0x89,
    MIN_CH5_LSB = 0x8A,
    MIN_CH5_MSB = 0x8B,
    MIN_CH6_LSB = 0x8C,
    MIN_CH6_MSB = 0x8D,
    MIN_CH7_LSB = 0x8E,
    MIN_CH7_MSB = 0x8F,
    RECENT_CH0_LSB = 0xA0,
    RECENT_CH0_MSB = 0xA1,
    RECENT_CH1_LSB = 0xA2,
    RECENT_CH1_MSB = 0xA3,
    RECENT_CH2_LSB = 0xA4,
    RECENT_CH2_MSB = 0xA5,
    RECENT_CH3_LSB = 0xA6,
    RECENT_CH3_MSB = 0xA7,
    RECENT_CH4_LSB = 0xA8,
    RECENT_CH4_MSB = 0xA9,
    RECENT_CH5_LSB = 0xAA,
    RECENT_CH5_MSB = 0xAB,
    RECENT_CH6_LSB = 0xAC,
    RECENT_CH6_MSB = 0xAD,
    RECENT_CH7_LSB = 0xAE,
    RECENT_CH7_MSB = 0xAF,
    RMS_CFG = 0xC0,
    RMS_LSB = 0xC1,
    RMS_MSB = 0xC2,
    GPO0_TRIG_EVENT_SEL = 0xC3,
    GPO1_TRIG_EVENT_SEL = 0xC5,
    GPO2_TRIG_EVENT_SEL = 0xC7,
    GPO3_TRIG_EVENT_SEL = 0xC9,
    GPO4_TRIG_EVENT_SEL = 0xCB,
    GPO5_TRIG_EVENT_SEL = 0xCD,
    GPO6_TRIG_EVENT_SEL = 0xCF,
    GPO7_TRIG_EVENT_SEL = 0xD1,
    GPO_VALUE_ZCD_CFG_CH0_CH3 = 0xE3,
    GPO_VALUE_ZCD_CFG_CH4_CH7 = 0xE4,
    GPO_ZCD_UPDATE_EN = 0xE7,
    GPO_TRIGGER_CFG = 0xE9,
    GPO_VALUE_TRIG = 0xEB,
  };

  // Register access
  uint8_t read_register_(Register r);
  void write_register_(Register r, uint8_t value);
  void set_register_bits_(Register r, uint8_t mask);
  void clear_register_bits_(Register r, uint8_t mask);

  // Read an analog sensor - returns true if complete, false if should be called again with the same sensor to complete
  // the read later
  bool read_sensor_(ADS7128Sensor *sensor);

  // Accessor type for a single bit field of a register
  template<Register R, uint8_t I> struct RegisterBit {
    ADS7128Component &parent;
    RegisterBit(ADS7128Component &parent) : parent{parent} {}
    bool get() { return parent.read_register_(R) & (1 << I); }
    operator bool() { return get(); }
    void set() { parent.set_register_bits_(R, 1 << I); }
    void clear() { parent.clear_register_bits_(R, 1 << I); }
    RegisterBit &operator=(bool value) {
      if (value) {
        set();
      } else {
        clear();
      }
      return *this;
    }
  };

  // Accessor type for int field of a register
  template<Register R, uint8_t I, uint8_t BIT_COUNT> struct RegisterInt {
    static constexpr uint8_t MASK = 0xFF >> (8 - BIT_COUNT) << I;
    ADS7128Component &parent;
    RegisterInt(ADS7128Component &parent) : parent{parent} {}
    uint8_t get() { return (parent.read_register_(R) & MASK) >> I; }
    operator uint8_t() { return get(); }
    void set(uint8_t value) {
      uint8_t r = parent.read_register_(R);
      r &= ~MASK;
      r |= value << I;
      parent.write_register_(R, r);
    }
    RegisterInt &operator=(uint8_t value) {
      set(value);
      return *this;
    }
  };

  // Register field accessor definitions
  using SYSTEM_STATUS_RSVD = RegisterBit<Register::SYSTEM_STATUS, 7>;
  using SEQ_STATUS = RegisterBit<Register::SYSTEM_STATUS, 6>;
  using I2C_SPEED = RegisterBit<Register::SYSTEM_STATUS, 5>;
  using RMS_DONE = RegisterBit<Register::SYSTEM_STATUS, 4>;
  using OSR_DONE = RegisterBit<Register::SYSTEM_STATUS, 3>;
  using CRC_ERR_FUSE = RegisterBit<Register::SYSTEM_STATUS, 2>;
  using CRC_ERR_IN = RegisterBit<Register::SYSTEM_STATUS, 1>;
  using BOR = RegisterBit<Register::SYSTEM_STATUS, 0>;

  using RMS_EN = RegisterBit<Register::GENERAL_CFG, 7>;
  using CRC_EN = RegisterBit<Register::GENERAL_CFG, 6>;
  using STATS_EN = RegisterBit<Register::GENERAL_CFG, 5>;
  using DWC_EN = RegisterBit<Register::GENERAL_CFG, 4>;
  using CNVST = RegisterBit<Register::GENERAL_CFG, 3>;
  using CH_RST = RegisterBit<Register::GENERAL_CFG, 2>;
  using CAL = RegisterBit<Register::GENERAL_CFG, 1>;
  using RST = RegisterBit<Register::GENERAL_CFG, 0>;

  using FIX_PAT = RegisterBit<Register::DATA_CFG, 7>;
  using APPEND_STATUS = RegisterInt<Register::DATA_CFG, 4, 2>;

  using OSR = RegisterInt<Register::OSR_CFG, 0, 3>;

  using CONV_ON_ERR = RegisterBit<Register::OPMODE_CFG, 7>;
  using CONV_MODE = RegisterInt<Register::OPMODE_CFG, 5, 2>;
  using OSC_SEL = RegisterInt<Register::OPMODE_CFG, 4, 1>;
  using CLK_DIV = RegisterInt<Register::OPMODE_CFG, 0, 4>;

  using SEQ_START = RegisterBit<Register::SEQUENCE_CFG, 4>;
  using SEQ_MODE = RegisterInt<Register::SEQUENCE_CFG, 0, 2>;

  using ZCD_CHID = RegisterInt<Register::CHANNEL_SEL, 4, 4>;
  using MANUAL_CHID = RegisterInt<Register::CHANNEL_SEL, 0, 4>;

  using RMS_CHID = RegisterInt<Register::RMS_CFG, 4, 4>;
  using RMS_DC_SUB = RegisterBit<Register::RMS_CFG, 2>;
  using RMS_SAMPLES = RegisterInt<Register::RMS_CFG, 0, 2>;

  void reset_device_() {
    RST(*this).set();
    while (RST(*this)) {
    }  // Wait for reset
    BOR(*this).set();
  }
};

/// Helper class to expose an ADS7128 pin as a GPIO pin.
class ADS7128GPIOPin : public GPIOPin, public Parented<ADS7128Component> {
 public:
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  void pin_mode(gpio::Flags flags) override { this->parent_->digital_setup(this->pin_, flags); }
  void setup() override { this->pin_mode(this->flags_); }

  std::string dump_summary() const override;

  bool digital_read() override { return this->parent_->digital_read(this->pin_); }
  void digital_write(bool value) override { this->parent_->digital_write(this->pin_, value); }

 protected:
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

/// Helper class to expose an ADS7128 pin as an analog sensor
class ADS7128Sensor : public PollingComponent, public sensor::Sensor, public Parented<ADS7128Component> {
 public:
  uint8_t get_channel() { return this->channel_; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  uint8_t get_cycle_time() { return uint8_t(this->cycle_time_); }
  uint8_t get_osc_sel() { return (this->get_cycle_time() & 0x10) >> 4; }
  uint8_t get_clk_div() { return this->get_cycle_time() & 0x0F; }
  uint32_t get_cycle_time_ns() {
    switch (this->cycle_time_) {
      case ADS7128_CYCLE_TIME_1:
        return 1 * 1000;
      case ADS7128_CYCLE_TIME_1P5:
        return 1500;
      case ADS7128_CYCLE_TIME_2:
        return 2 * 1000;
      case ADS7128_CYCLE_TIME_3:
        return 3 * 1000;
      case ADS7128_CYCLE_TIME_4:
        return 4 * 1000;
      case ADS7128_CYCLE_TIME_6:
        return 6 * 1000;
      case ADS7128_CYCLE_TIME_8:
        return 8 * 1000;
      case ADS7128_CYCLE_TIME_12:
        return 12 * 1000;
      case ADS7128_CYCLE_TIME_16:
        return 16 * 1000;
      case ADS7128_CYCLE_TIME_24:
        return 24 * 1000;
      case ADS7128_CYCLE_TIME_32:
        return 32 * 1000;
      case ADS7128_CYCLE_TIME_48:
        return 48 * 1000;
      case ADS7128_CYCLE_TIME_64:
        return 64 * 1000;
      case ADS7128_CYCLE_TIME_96:
        return 96 * 1000;
      case ADS7128_CYCLE_TIME_128:
        return 128 * 1000;
      case ADS7128_CYCLE_TIME_192:
        return 192 * 1000;
      case ADS7128_CYCLE_TIME_256:
        return 256 * 1000;
      case ADS7128_CYCLE_TIME_384:
        return 384 * 1000;
      case ADS7128_CYCLE_TIME_512:
        return 512 * 1000;
      case ADS7128_CYCLE_TIME_768:
        return 768 * 1000;
      case ADS7128_CYCLE_TIME_1024:
        return 1024 * 1000;
      case ADS7128_CYCLE_TIME_1536:
        return 1536 * 1000;
      case ADS7128_CYCLE_TIME_2048:
        return 2048 * 1000;
      case ADS7128_CYCLE_TIME_3072:
        return 3072 * 1000;
      case ADS7128_CYCLE_TIME_4096:
        return 4096 * 1000;
      case ADS7128_CYCLE_TIME_6144:
        return 6144 * 1000;
    }
    return -1;
  }
  void set_cycle_time(ADS7128CycleTime value) { this->cycle_time_ = value; }

  uint8_t get_oversampling() { return uint8_t(this->oversampling_); }
  uint8_t get_oversampling_count() { return 1 << this->get_oversampling(); }
  void set_oversampling(ADS7128Oversamping value) {
    this->oversampling_ = value;
    this->set_accuracy_decimals(this->calculate_accuracy_decimals());
  }

  uint32_t get_sample_time_ns() { return this->get_oversampling_count() * this->get_cycle_time_ns(); }

  bool get_rms() { return this->rms_; }
  void set_rms(bool value) {
    this->rms_ = value;
    this->set_accuracy_decimals(this->calculate_accuracy_decimals());
  }

  uint8_t get_rms_samples() { return uint8_t(this->rms_samples_); }
  uint32_t get_rms_samples_count() {
    switch (this->rms_samples_) {
      case ADS7128_RMS_SAMPLES_1024:
        return 1024;
      case ADS7128_RMS_SAMPLES_4096:
        return 4096;
      case ADS7128_RMS_SAMPLES_16384:
        return 16384;
      case ADS7128_RMS_SAMPLES_65536:
        return 65536;
    }
    return -1;
  }
  void set_rms_samples(ADS7128RmsSamples value) { this->rms_samples_ = value; }

  uint32_t get_rms_us() { return uint64_t(this->get_rms_samples_count()) * this->get_sample_time_ns() / 1000; }
  uint32_t get_rms_read_time_us() {
    return uint64_t(this->get_rms_samples_count() + 40) * this->get_sample_time_ns() / 1000;
  }

  int8_t calculate_accuracy_decimals() {
    return (this->rms_ ? 16 : 12 + std::min(this->get_oversampling(), uint8_t(4))) / std::log2(10);
  }

  void update() override { this->parent_->sensor_update(this); }

  void setup() override { this->update(); }

  void dump_config() override;

 protected:
  uint8_t channel_;
  ADS7128CycleTime cycle_time_;
  ADS7128Oversamping oversampling_;
  bool rms_;
  ADS7128RmsSamples rms_samples_;
};

}  // namespace ads7128
}  // namespace esphome
