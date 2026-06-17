#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::as7341 {

static const uint8_t AS7341_CHIP_ID = 0x09;

static const uint8_t AS7341_CONFIG = 0x70;
static const uint8_t AS7341_LED = 0x74;

static const uint8_t AS7341_ENABLE = 0x80;
static const uint8_t AS7341_ATIME = 0x81;

static const uint8_t AS7341_WTIME = 0x83;

static const uint8_t AS7341_AUXID = 0x90;
static const uint8_t AS7341_REVID = 0x91;
static const uint8_t AS7341_ID = 0x92;
static const uint8_t AS7341_STATUS = 0x93;
static const uint8_t AS7341_ASTATUS = 0x94;

static const uint8_t AS7341_CH0_DATA_L = 0x95;
static const uint8_t AS7341_CH0_DATA_H = 0x96;
static const uint8_t AS7341_CH1_DATA_L = 0x97;
static const uint8_t AS7341_CH1_DATA_H = 0x98;
static const uint8_t AS7341_CH2_DATA_L = 0x99;
static const uint8_t AS7341_CH2_DATA_H = 0x9A;
static const uint8_t AS7341_CH3_DATA_L = 0x9B;
static const uint8_t AS7341_CH3_DATA_H = 0x9C;
static const uint8_t AS7341_CH4_DATA_L = 0x9D;
static const uint8_t AS7341_CH4_DATA_H = 0x9E;
static const uint8_t AS7341_CH5_DATA_L = 0x9F;
static const uint8_t AS7341_CH5_DATA_H = 0xA0;

static const uint8_t AS7341_STATUS2 = 0xA3;

static const uint8_t AS7341_CFG1 = 0xAA;  ///< Controls ADC Gain

static const uint8_t AS7341_CFG6 = 0xAF;  // Stores SMUX command
static const uint8_t AS7341_CFG8 = 0xB1;  // AGC
static const uint8_t AS7341_CFG9 = 0xB2;  // Config for system interrupts (SMUX, Flicker detection)

static const uint8_t AS7341_ASTEP = 0xCA;      // LSB
static const uint8_t AS7341_ASTEP_MSB = 0xCB;  // MSB

enum AS7341AdcChannel {
  AS7341_ADC_CHANNEL_0,
  AS7341_ADC_CHANNEL_1,
  AS7341_ADC_CHANNEL_2,
  AS7341_ADC_CHANNEL_3,
  AS7341_ADC_CHANNEL_4,
  AS7341_ADC_CHANNEL_5,
};

enum AS7341SmuxCommand {
  AS7341_SMUX_CMD_ROM_RESET,  ///< ROM code initialization of SMUX
  AS7341_SMUX_CMD_READ,       ///< Read SMUX configuration to RAM from SMUX chain
  AS7341_SMUX_CMD_WRITE,      ///< Write SMUX configuration from RAM to SMUX chain
};

enum AS7341Gain {
  AS7341_GAIN_0_5X,
  AS7341_GAIN_1X,
  AS7341_GAIN_2X,
  AS7341_GAIN_4X,
  AS7341_GAIN_8X,
  AS7341_GAIN_16X,
  AS7341_GAIN_32X,
  AS7341_GAIN_64X,
  AS7341_GAIN_128X,
  AS7341_GAIN_256X,
  AS7341_GAIN_512X,
  AS7341_GAIN_COUNT,
};

union AS7341RegAStatus {
  uint8_t raw;
  struct {
    AS7341Gain again_status : 4;
    uint8_t reserved_4_6 : 3;
    uint8_t asat_status : 1;
  } __attribute__((packed));
};

union AS7341RegStatus2 {
  uint8_t raw;
  struct {
    bool fdsat_digital : 1;
    bool fdsat_analog : 1;
    bool reserved_2 : 1;
    bool asat_analog : 1;
    bool asat_digital : 1;
    bool reserved_5 : 1;
    bool avalid : 1;
    bool reserved_7 : 1;
  } __attribute__((packed));
};

union As7341Cfg8 {
  uint8_t raw;
  struct {
    uint8_t reserved_0_1 : 2;
    uint8_t sp_agc_enable : 1;  // Spectral AGC enable
    uint8_t fd_agc_enable : 1;  // Flicker detection AGC enable
    uint8_t reserved_4_5 : 2;
    uint8_t fifo_th : 2;
  } __attribute__((packed));
};

constexpr uint16_t AS7341_NUM_CHANNELS = 12;

using ChannelValuesString = std::array<const char *, AS7341_NUM_CHANNELS>;
using ChannelValuesUint16 = std::array<uint16_t, AS7341_NUM_CHANNELS>;
using ChannelValuesFloat = std::array<float, AS7341_NUM_CHANNELS>;
using CalibrationCoeffs = std::array<float, AS7341_NUM_CHANNELS - 2>;

using SensorArray = std::array<sensor::Sensor *, AS7341_NUM_CHANNELS>;

#define SUB_SENSOR_ARRAY

class AS7341Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

  void set_gain(AS7341Gain gain) { gain_ = gain; }
  void set_atime(uint8_t atime) { atime_ = atime; }
  void set_astep(uint16_t astep) { astep_ = astep; }

  // calibration coeefs
  void set_dark_current_calibration(const CalibrationCoeffs &vals);
  void set_channel_correction(const CalibrationCoeffs &vals);
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  // hardware
  AS7341Gain get_gain();
  uint8_t get_atime();
  uint16_t get_astep();

  // hardware registers update
  bool setup_gain(AS7341Gain gain);
  bool setup_atime(uint8_t atime);
  bool setup_astep(uint16_t astep);

  bool enable_power(bool enable);

#ifdef USE_SENSOR
 protected:
  SensorArray band_counts_sensors_{};
  SensorArray band_basic_counts_sensors_{};

 public:
  void set_band_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < AS7341_NUM_CHANNELS) {
      band_counts_sensors_[channel] = sensor;
    }
  }
  void set_band_basic_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < AS7341_NUM_CHANNELS) {
      band_basic_counts_sensors_[channel] = sensor;
    }
  }

  SUB_SENSOR(illuminance);
  SUB_SENSOR(irradiance);
  SUB_SENSOR(irradiance_photopic);
  SUB_SENSOR(irradiance_par);
  SUB_SENSOR(ppfd);
  SUB_SENSOR(color_temperature);
  SUB_SENSOR(saturation_level);
#endif

 protected:
  uint16_t astep_;
  AS7341Gain gain_;
  uint8_t atime_;

  ChannelValuesFloat dark_current_offset_{};
  ChannelValuesFloat channel_correction_{
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  };
  float glass_attenuation_factor_{1.0f};
  float corr_lx_y_cie1931_{683.002f};

  enum class State : uint8_t {
    NOT_INITIALIZED,
    INITIAL_SETUP_COMPLETED,
    IDLE,
    START_MEASUREMENT,
    ENABLE_SMUX_LOW,
    WAIT_SMUX_LOW,
    READ_SMUX_LOW,
    ENABLE_SMUX_HIGH,
    WAIT_SMUX_HIGH,
    READ_SMUX_HIGH,
    DATA_COLLECTED,
    CALCULATE_CIE,
    CALCULATE_SPECTRAL,
    VALIDATE_VALUES,
    READY_TO_PUBLISH_PART_1,
    READY_TO_PUBLISH_PART_2,
    READY_TO_PUBLISH_PART_3
  } state_{State::NOT_INITIALIZED};

  struct {
    uint32_t millis_start;
    uint8_t atime;
    uint16_t astep;
    bool first_run{true};

    ChannelValuesUint16 raw_counts{};

    AS7341Gain low_gain;
    bool low_saturated{false};
    bool low_success{false};
    AS7341Gain high_gain;
    bool high_saturated{false};
    bool high_success{false};

    AS7341Gain gain{};
    float gain_x{};
    float t_int_us{};

  } readings_;

  struct {
    ChannelValuesFloat basic_counts{};
    ChannelValuesFloat mW_per_band{};
    float irradiance;
    float irradiance_photopic;
    float irradiance_par;
    float ppfd;
    float cct;
    float duv;
    float saturation_level;
    float lux_from_irradiance;
    float lux_from_xyz;
    float lux;
  } calculated_values_;

  float get_gain_multiplier_(AS7341Gain gain);

  void calculate_();
  void calculate_basic_counts_();
  void calculate_spectral_(float &lux, float &par, float &ppfd, float &irradiance, float &irradiance_photopic);
  void calculate_color_(float &ct, float &duv, float &lux);

  void publish_channel_readings_();
  void publish_basic_counts_();
  void publish_derived_readings_();

  uint16_t get_maximum_spectral_adc_();
  uint16_t get_maximum_spectral_adc_(uint16_t atime, uint16_t astep);

  template<typename T, size_t N> T get_highest_value_(std::array<T, N> &data);
#ifdef USE_SENSOR
  void publish_sensor_(sensor::Sensor *sensor, float value) {
    if (sensor != nullptr) {
      sensor->publish_state(value);
    }
  }
#endif

 protected:
  bool set_smux_command_(AS7341SmuxCommand command);
  void configure_smux_low_channels_();
  void configure_smux_high_channels_();
  bool enable_smux_();

  bool is_data_ready_();
  bool enable_spectral_measurement_(bool enable);

  bool read_low_channels_();
  bool read_high_channels_();
  bool read_and_discard_channels_();

  bool read_register_bit_(uint8_t address, uint8_t bit_position);
  bool write_register_bit_(uint8_t address, bool value, uint8_t bit_position);
  bool set_register_bit_(uint8_t address, uint8_t bit_position);
  bool clear_register_bit_(uint8_t address, uint8_t bit_position);
  uint16_t swap_bytes_(uint16_t data);
};

}  // namespace esphome::as7341
