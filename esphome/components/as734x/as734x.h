#pragma once

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::as734x {

enum class Model : uint8_t {
  AS7341 = 41,
  AS7343 = 43,
};

enum Gain : uint8_t {
  GAIN_0_5X,
  GAIN_1X,
  GAIN_2X,
  GAIN_4X,
  GAIN_8X,
  GAIN_16X,
  GAIN_32X,
  GAIN_64X,
  GAIN_128X,
  GAIN_256X,
  GAIN_512X,
  GAIN_1024X,  // only for AS7343
  GAIN_2048X,  // only for AS7343
};

constexpr uint8_t MAX_CHANNELS = 13;  // AS7343 reports 13 bands, AS7341 reports 10

using ChannelValuesUint16 = std::array<uint16_t, MAX_CHANNELS>;
using SensorArray = std::array<sensor::Sensor *, MAX_CHANNELS>;

union RegAStatus {
  uint8_t raw;
  struct {
    Gain again_status : 4;
    uint8_t reserved_4_6 : 3;
    uint8_t asat_status : 1;
  } __attribute__((packed));
};

struct RegisterMap {
  // Address window that the CFG0 bank bit maps in. Registers outside it are always reachable.
  uint8_t BANK_LOW_MIN;
  uint8_t BANK_LOW_MAX;
  uint8_t ASTEP;
  uint8_t ATIME;
  uint8_t CFG0;
  uint8_t CFG0_REG_BANK_BIT;
  uint8_t CFG1;
  uint8_t ENABLE;
  uint8_t ENABLE_PON_BIT;
  uint8_t ENABLE_SP_EN_BIT;
  uint8_t ENABLE_SMUX_EN_BIT;
  uint8_t STATUS2;
  uint8_t STATUS2_AVALID_BIT;
};

////////////////////////////////////////////////////////////////////////////////
//
// AS734xBase
//
// This is the base class for AS7341 and AS7343 devices.
// It handles the I2C communication and provides common functionality
// for both devices.
//
////////////////////////////////////////////////////////////////////////////////

class AS734xBase {
 public:
  AS734xBase(i2c::I2CDevice *i2c_device, uint8_t number_of_channels);
  virtual ~AS734xBase() = default;

  uint8_t get_number_of_channels() const { return this->number_of_channels_; }

  virtual bool verify_device_id() = 0;
  virtual void write_default_config() = 0;

  bool write_gain(Gain gain);
  bool write_atime(uint8_t atime);
  bool write_astep(uint16_t astep);

  bool enable_power(bool enable);
  bool enable_spectral_measurement(bool enable);

  virtual uint8_t get_number_of_smux_steps() const = 0;
  virtual bool prepare_for_smux_step(uint8_t step) = 0;

  virtual bool enable_smux();
  virtual bool is_smux_busy();
  virtual bool is_data_ready();

  virtual bool read_channels(uint8_t step, ChannelValuesUint16 &values, Gain &gain, bool &saturated) = 0;

 protected:
  virtual const RegisterMap &registers() const = 0;

  i2c::I2CDevice *i2c_device_ = nullptr;
  uint8_t number_of_channels_;

  bool bank_low_{false};
  void set_bank_(bool low);  // default bank = 0 (for regs 0x80+)
  void set_bank_for_reg_(uint8_t reg);

  inline uint16_t swap_bytes_(uint16_t data) { return (data >> 8) | (data << 8); }
  bool read_register_bit_(uint8_t address, uint8_t bit_position);
  bool write_register_bit_(uint8_t address, bool value, uint8_t bit_position);
  bool set_register_bit_(uint8_t address, uint8_t bit_position);
  bool clear_register_bit_(uint8_t address, uint8_t bit_position);
};

class AS734XComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup_model(Model model);

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_atime(uint8_t atime) { this->atime_ = atime; }
  void set_astep(uint16_t astep) { this->astep_ = astep; }

#ifdef USE_SENSOR
 protected:
  SensorArray band_counts_sensors_{};

 public:
  void set_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < this->band_counts_sensors_.size()) {
      this->band_counts_sensors_[channel] = sensor;
    }
  }
#endif

 protected:
  Model model_{Model::AS7343};
  AS734xBase *device_{nullptr};

  //
  // Internal state machine, used to split all the actions into
  // small steps in loop() to make sure we are not blocking execution
  //
  enum class State : uint8_t {
    NOT_INITIALIZED,
    IDLE,
    START_MEASUREMENT,
    CONFIGURE_SMUX,
    WAIT_SMUX,
    READ_DATA,
    READY_TO_PUBLISH,
  } state_{State::NOT_INITIALIZED};

  uint16_t astep_;
  Gain gain_;
  uint8_t atime_;

  struct {
    ChannelValuesUint16 raw_counts{};
    Gain gain;
    uint32_t millis_start;
    uint8_t smux_step;
    bool first_run{true};
  } readings_;

  void publish_channel_readings_();
};

}  // namespace esphome::as734x
