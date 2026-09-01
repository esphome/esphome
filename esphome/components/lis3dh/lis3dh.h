#pragma once

#include "esphome/components/motion/motion_component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::lis3dh {

//  Register map (see ST datasheet DocID18198)
static constexpr uint8_t LIS3DH_REG_OUT_ADC3_L = 0x0C;  // temperature (ADC3) low byte
static constexpr uint8_t LIS3DH_REG_WHO_AM_I = 0x0F;
static constexpr uint8_t LIS3DH_REG_TEMP_CFG = 0x1F;
static constexpr uint8_t LIS3DH_REG_CTRL_REG1 = 0x20;  // ODR / low-power / axis enable
static constexpr uint8_t LIS3DH_REG_CTRL_REG2 = 0x21;  // high-pass filter
static constexpr uint8_t LIS3DH_REG_CTRL_REG3 = 0x22;  // INT1 pad routing
static constexpr uint8_t LIS3DH_REG_CTRL_REG4 = 0x23;  // BDU / full-scale / high-resolution
static constexpr uint8_t LIS3DH_REG_CTRL_REG5 = 0x24;  // interrupt latching
static constexpr uint8_t LIS3DH_REG_CTRL_REG6 = 0x25;  // INT2 pad routing / polarity
static constexpr uint8_t LIS3DH_REG_REFERENCE = 0x26;  // high-pass filter reference
static constexpr uint8_t LIS3DH_REG_OUT_X_L = 0x28;    // acceleration data block (auto-increment)
static constexpr uint8_t LIS3DH_REG_INT1_CFG = 0x30;
static constexpr uint8_t LIS3DH_REG_INT1_SRC = 0x31;
static constexpr uint8_t LIS3DH_REG_INT1_THS = 0x32;
static constexpr uint8_t LIS3DH_REG_INT1_DURATION = 0x33;

static constexpr uint8_t LIS3DH_WHO_AM_I_VALUE = 0x33;

// Sub-address auto-increment bit: OR into the register for multi-byte reads.
static constexpr uint8_t LIS3DH_AUTO_INCREMENT = 0x80;

// CTRL_REG1
static constexpr uint8_t LIS3DH_CTRL_REG1_AXES_EN = 0x07;  // Xen | Yen | Zen
static constexpr uint8_t LIS3DH_CTRL_REG1_LPEN = 0x08;     // low-power enable
// CTRL_REG2
static constexpr uint8_t LIS3DH_CTRL_REG2_HP_IA1 = 0x01;  // high-pass filter the IA1 interrupt
// CTRL_REG4
static constexpr uint8_t LIS3DH_CTRL_REG4_HR = 0x08;   // high-resolution enable
static constexpr uint8_t LIS3DH_CTRL_REG4_BDU = 0x80;  // block data update
// CTRL_REG5
static constexpr uint8_t LIS3DH_CTRL_REG5_LIR_INT1 = 0x08;  // latch IA1 interrupt
// CTRL_REG3 / CTRL_REG6 interrupt routing
static constexpr uint8_t LIS3DH_CTRL_REG3_I1_IA1 = 0x40;        // route IA1 to INT1 pad
static constexpr uint8_t LIS3DH_CTRL_REG6_I2_IA1 = 0x40;        // route IA1 to INT2 pad
static constexpr uint8_t LIS3DH_CTRL_REG6_INT_POLARITY = 0x02;  // 1 = active low
// INT1_CFG high-event bits
static constexpr uint8_t LIS3DH_INT_CFG_XHIE = 0x02;
static constexpr uint8_t LIS3DH_INT_CFG_YHIE = 0x08;
static constexpr uint8_t LIS3DH_INT_CFG_ZHIE = 0x20;
// TEMP_CFG_REG
static constexpr uint8_t LIS3DH_TEMP_CFG_ADC_EN = 0x80;
static constexpr uint8_t LIS3DH_TEMP_CFG_TEMP_EN = 0x40;

// The temperature sensor is uncalibrated and reports relative changes only,
// so this fixed reference is just a rough starting point for the °C output.
static constexpr float LIS3DH_TEMP_REFERENCE_C = 25.0f;

// Full-scale range: value is the index into the sensitivity tables and the
// FS field of CTRL_REG4 (shifted left by 4).
enum LIS3DHRange : uint8_t {
  LIS3DH_RANGE_2G = 0,
  LIS3DH_RANGE_4G = 1,
  LIS3DH_RANGE_8G = 2,
  LIS3DH_RANGE_16G = 3,
};

// Output data rate: value is the ODR field of CTRL_REG1 (bits 7:4).
enum LIS3DHDataRate : uint8_t {
  LIS3DH_DATA_RATE_1HZ = 0x1,
  LIS3DH_DATA_RATE_10HZ = 0x2,
  LIS3DH_DATA_RATE_25HZ = 0x3,
  LIS3DH_DATA_RATE_50HZ = 0x4,
  LIS3DH_DATA_RATE_100HZ = 0x5,
  LIS3DH_DATA_RATE_200HZ = 0x6,
  LIS3DH_DATA_RATE_400HZ = 0x7,
  LIS3DH_DATA_RATE_1620HZ = 0x8,  // low-power mode only
  LIS3DH_DATA_RATE_1344HZ = 0x9,  // 1344 Hz normal/HR, 5376 Hz low-power
};

// Operating mode: selects the resolution (and thus power usage).
enum LIS3DHOperatingMode : uint8_t {
  LIS3DH_MODE_LOW_POWER = 0,        // 8-bit
  LIS3DH_MODE_NORMAL = 1,           // 10-bit
  LIS3DH_MODE_HIGH_RESOLUTION = 2,  // 12-bit
};

// Physical interrupt pad the motion interrupt is routed to.
enum LIS3DHInterruptPin : uint8_t {
  LIS3DH_INT_PIN_INT1 = 0,
  LIS3DH_INT_PIN_INT2 = 1,
};

class LIS3DHComponent : public motion::MotionComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_range(LIS3DHRange range) { this->range_ = range; }
  void set_data_rate(LIS3DHDataRate data_rate) { this->data_rate_ = data_rate; }
  void set_operating_mode(LIS3DHOperatingMode mode) { this->operating_mode_ = mode; }
  void set_interrupt(LIS3DHInterruptPin pin, float threshold_g, uint8_t duration, bool x, bool y, bool z, bool latched,
                     bool active_high, bool high_pass) {
    this->interrupt_enabled_ = true;
    this->interrupt_pin_ = pin;
    this->interrupt_threshold_g_ = threshold_g;
    this->interrupt_duration_ = duration;
    this->interrupt_axes_cfg_ =
        (x ? LIS3DH_INT_CFG_XHIE : 0) | (y ? LIS3DH_INT_CFG_YHIE : 0) | (z ? LIS3DH_INT_CFG_ZHIE : 0);
    this->interrupt_latched_ = latched;
    this->interrupt_active_high_ = active_high;
    this->interrupt_high_pass_ = high_pass;
  }

  template<typename F> void add_temperature_listener(F &&cb) { this->temperature_callback_.add(std::forward<F>(cb)); }

 protected:
  bool update_data(motion::MotionData &data) override;
  bool setup_accelerometer_();
  bool setup_temperature_();
  bool setup_interrupt_();
  void publish_temperature_();

  LIS3DHRange range_{LIS3DH_RANGE_2G};
  LIS3DHDataRate data_rate_{LIS3DH_DATA_RATE_100HZ};
  uint8_t interrupt_threshold_raw_{0};  // what the g threshold became, for dump_config
  LIS3DHOperatingMode operating_mode_{LIS3DH_MODE_HIGH_RESOLUTION};

  // Motion (activity) interrupt configuration.
  bool interrupt_enabled_{false};
  LIS3DHInterruptPin interrupt_pin_{LIS3DH_INT_PIN_INT1};
  float interrupt_threshold_g_{0.0f};
  uint8_t interrupt_duration_{0};
  uint8_t interrupt_axes_cfg_{0};
  bool interrupt_latched_{true};
  bool interrupt_active_high_{true};
  bool interrupt_high_pass_{false};

  LazyCallbackManager<void(float)> temperature_callback_{};
};

}  // namespace esphome::lis3dh
