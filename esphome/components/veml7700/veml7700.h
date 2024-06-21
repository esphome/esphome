#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

namespace esphome {
namespace veml7700 {

using esphome::i2c::ErrorCode;

//
// Datasheet: https://www.vishay.com/docs/84286/veml7700.pdf
//

enum class CommandRegisters : uint8_t {
  ALS_CONF_0 = 0x00,  // W: ALS gain, integration time, interrupt, and shutdown
  ALS_WH = 0x01,      // W: ALS high threshold window setting
  ALS_WL = 0x02,      // W: ALS low threshold window setting
  PWR_SAVING = 0x03,  // W: Set (15 : 3) 0000 0000 0000 0b
  ALS = 0x04,         // R: MSB, LSB data of whole ALS 16 bits
  WHITE = 0x05,       // R: MSB, LSB data of whole WHITE 16 bits
  ALS_INT = 0x06      // R: ALS INT trigger event
};

enum Gain : uint16_t {
  X_1 = 0,
  X_2 = 1,
  X_1_8 = 2,
  X_1_4 = 3,
};
const uint8_t GAINS_COUNT = 4;

enum IntegrationTime : uint16_t {
  INTEGRATION_TIME_25MS = 0b1100,
  INTEGRATION_TIME_50MS = 0b1000,
  INTEGRATION_TIME_100MS = 0b0000,
  INTEGRATION_TIME_200MS = 0b0001,
  INTEGRATION_TIME_400MS = 0b0010,
  INTEGRATION_TIME_800MS = 0b0011,
};
const uint8_t INTEGRATION_TIMES_COUNT = 6;

enum Persistence : uint16_t {
  PERSISTENCE_1 = 0,
  PERSISTENCE_2 = 1,
  PERSISTENCE_4 = 2,
  PERSISTENCE_8 = 3,
};

enum PSMMode : uint16_t {
  PSM_MODE_1 = 0,
  PSM_MODE_2 = 1,
  PSM_MODE_3 = 2,
  PSM_MODE_4 = 3,
};

// The following section with bit-fields brings GCC compilation 'notes' about padding bytes due to bug in older GCC back
// in 2009 "Packed bit-fields of type char were not properly bit-packed on many targets prior to GCC 4.4" Even more to
// this - this message can't be disabled with "#pragma GCC diagnostic ignored" due to another bug which was only fixed
// in GCC 13 in 2022 :) No actions required, it is just a note. The code is correct.

//
// VEML7700_CR_ALS_CONF_0 Register (0x00)
//
union ConfigurationRegister {
  uint16_t raw;
  uint8_t raw_bytes[2];
  struct {
    bool ALS_SD : 1;             // ALS shut down setting: 0 = ALS power on, 1 = ALS shut
                                 // down
    bool ALS_INT_EN : 1;         // ALS interrupt enable setting: 0 = ALS INT disable, 1
                                 // = ALS INT enable
    bool reserved_2 : 1;         // 0
    bool reserved_3 : 1;         // 0
    Persistence ALS_PERS : 2;    // 00 - 1, 01- 2, 10 - 4, 11 - 8
    IntegrationTime ALS_IT : 4;  // ALS integration time setting
    bool reserved_10 : 1;        // 0
    Gain ALS_GAIN : 2;           // Gain selection
    bool reserved_13 : 1;        // 0
    bool reserved_14 : 1;        // 0
    bool reserved_15 : 1;        // 0
  } __attribute__((packed));
};

//
// Power Saving Mode: PSM Register (0x03)
//
union PSMRegister {
  uint16_t raw;
  uint8_t raw_bytes[2];
  struct {
    bool PSM_EN : 1;
    PSMMode PSM : 2;
    uint16_t reserved : 13;
  } __attribute__((packed));
};

class VEML7700Component : public PollingComponent, public i2c::I2CDevice {
 public:
  //
  // EspHome framework functions
  //
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  //
  // Configuration setters
  //
  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_integration_time(IntegrationTime time) { this->integration_time_ = time; }
  void set_enable_automatic_mode(bool enable) { this->automatic_mode_enabled_ = enable; }
  void set_enable_lux_compensation(bool enable) { this->lux_compensation_enabled_ = enable; }
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  void set_ambient_light_sensor(sensor::Sensor *sensor) { this->ambient_light_sensor_ = sensor; }
  void set_ambient_light_counts_sensor(sensor::Sensor *sensor) { this->ambient_light_counts_sensor_ = sensor; }
  void set_white_sensor(sensor::Sensor *sensor) { this->white_sensor_ = sensor; }
  void set_white_counts_sensor(sensor::Sensor *sensor) { this->white_counts_sensor_ = sensor; }
  void set_infrared_sensor(sensor::Sensor *sensor) { this->fake_infrared_sensor_ = sensor; }
  void set_actual_gain_sensor(sensor::Sensor *sensor) { this->actual_gain_sensor_ = sensor; }
  void set_actual_integration_time_sensor(sensor::Sensor *sensor) { this->actual_integration_time_sensor_ = sensor; }

 protected:
  //
  // Internal state machine, used to split all the actions into
  // small steps in loop() to make sure we are not blocking execution
  //
  enum class State : uint8_t {
    NOT_INITIALIZED,
    INITIAL_SETUP_COMPLETED,
    IDLE,
    COLLECTING_DATA,
    COLLECTING_DATA_AUTO,
    DATA_COLLECTED,
    ADJUSTMENT_NEEDED,
    ADJUSTMENT_IN_PROGRESS,
    READY_TO_APPLY_ADJUSTMENTS,
    READY_TO_PUBLISH_PART_1,
    READY_TO_PUBLISH_PART_2,
    READY_TO_PUBLISH_PART_3
  } state_{State::NOT_INITIALIZED};

  //
  // Current measurements data
  //
  struct Readings {
    uint16_t als_counts{0};
    uint16_t white_counts{0};
    IntegrationTime actual_time{INTEGRATION_TIME_100MS};
    Gain actual_gain{X_1_8};
    float als_lux{0};
    float white_lux{0};
    float fake_infrared_lux{0};
    ErrorCode err{i2c::ERROR_OK};
  } readings_;

  //
  // Device interaction
  //
  ErrorCode configure_();
  ErrorCode reconfigure_time_and_gain_(IntegrationTime time, Gain gain, bool shutdown);
  ErrorCode read_sensor_output_(Readings &data);

  //
  // Working with the data
  //
  bool are_adjustments_required_(Readings &data);
  void apply_lux_calculation_(Readings &data);
  void apply_lux_compensation_(Readings &data);
  void apply_glass_attenuation_(Readings &data);
  void publish_data_part_1_(Readings &data);
  void publish_data_part_2_(Readings &data);
  void publish_data_part_3_(Readings &data);

  //
  // Component configuration
  //
  bool automatic_mode_enabled_{true};
  bool lux_compensation_enabled_{true};
  float glass_attenuation_factor_{1.0};
  IntegrationTime integration_time_{INTEGRATION_TIME_100MS};
  Gain gain_{X_1};

  //
  //   Sensors for publishing data
  //
  sensor::Sensor *ambient_light_sensor_{nullptr};            // Human eye range 500-600 nm, lx
  sensor::Sensor *ambient_light_counts_sensor_{nullptr};     // Raw counts
  sensor::Sensor *white_sensor_{nullptr};                    // Wide range 450-950 nm, lx
  sensor::Sensor *white_counts_sensor_{nullptr};             // Raw counts
  sensor::Sensor *fake_infrared_sensor_{nullptr};            // Artificial. = WHITE lx - ALS lx.
  sensor::Sensor *actual_gain_sensor_{nullptr};              // Actual gain multiplier for the measurement
  sensor::Sensor *actual_integration_time_sensor_{nullptr};  // Actual integration time for the measurement
};

}  // namespace veml7700
}  // namespace esphome
