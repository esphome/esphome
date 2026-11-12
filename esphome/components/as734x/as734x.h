#pragma once

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_AS734X_RGB
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::as734x {

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
  GAIN_1024X,  // AS7343 only
  GAIN_2048X,  // AS7343 only
};

static constexpr uint8_t GAIN_COUNT = 13;  // entries in Gain, for sizing the correction tables
static constexpr uint8_t NUM_BANDS = 13;   // band order of BANDS_43 in __init__.py

// What the published basic counts are divided by. The clear channel is wideband, so it is roughly
// the size of all the bands together, and the near infrared channel outgrows them under daylight.
// Dividing by the largest of every channel therefore leaves the visible bands squeezed into the
// bottom of the range.
enum class Normalization : uint8_t {
  NORMALIZATION_NONE,   // publish absolute basic counts
  NORMALIZATION_ALL,    // largest of every channel
  NORMALIZATION_BANDS,  // largest of the visible bands only
  NORMALIZATION_CLEAR,  // the wideband clear channel
};

// Nominal centre wavelengths in nanometres bound the visible bands. The clear channel is wideband
// and has no single centre wavelength, so it is marked with zero instead.
static constexpr uint16_t VISIBLE_MIN_NM = 380;
static constexpr uint16_t VISIBLE_MAX_NM = 700;
static constexpr uint16_t WIDEBAND_NM = 0;
static constexpr uint16_t ABSENT_NM = 0xFFFF;  // band this chip does not have

// Luminous efficacy at 555 nm, for turning photopic irradiance into lux.
static constexpr float LUMENS_PER_WATT = 683.002f;

// How much one basic count of a band contributes to each integrated quantity. The factors come
// from the manufacturer's golden-device calibration, convolved for a reconstructed spectrum.
struct ChannelContribution {
  float irradiance_photopic;  // weighted by the CIE 1931 V(lambda) curve
  float irradiance_par;       // 400-700 nm only
  float ppfd;                 // 400-700 nm only, scaled by 1e3 before publishing
};

// CIE 1931 tristimulus contribution of one basic count of a band.
struct ChannelTristimulus {
  float x;
  float y;
  float z;
};

/// Per-chip constants, stored in flash. The pointed-to tables are in flash too.
struct ChipInfo {
  const char *name;
  const i2c::I2CRegValue *init;              // written in order; the bank is selected per register address
  const uint8_t *smux;                       // AS7341: SMUX RAM image per step; AS7343: the three chain images
  const uint8_t *readout;                    // per step, the ADC slot of each band (0xFF: not read in this step)
  const uint16_t *wavelengths;               // per band, WIDEBAND_NM for clear and ABSENT_NM for a missing band
  const float *gain_correction;              // GAIN_COUNT entries, or GAIN_COUNT * NUM_BANDS when per band
  const ChannelContribution *contributions;  // per band
  const ChannelTristimulus *tristimulus;     // per band
  uint8_t init_count;
  uint8_t astep_reg;
  uint8_t cfg0_reg;
  uint8_t cfg1_reg;
  uint8_t status2_reg;
  uint8_t id_reg;
  uint8_t id_mask;
  uint8_t id_value;
  uint8_t led_reg;      // LED driver register (0: this chip has no LED control)
  uint8_t led_sel_reg;  // register whose LED_SEL bit hands the pin to the driver (0: not needed)
  uint8_t adc_slots;    // 16-bit data registers read after ASTATUS
  uint8_t smux_steps;   // measurements per update
  uint8_t integration_cycles;
  uint8_t smux_len;               // bytes per SMUX image
  bool auto_smux;                 // the chip cycles its SMUX itself after a one-time chain setup
  bool gain_correction_per_band;  // the correction table is indexed by gain and band, not gain alone
  uint8_t extra_clear_slot;       // averaged into the clear band (0xFF: none)
};

#ifdef USE_AS7341
extern const ChipInfo AS7341_CHIP;
#endif
#ifdef USE_AS7343
extern const ChipInfo AS7343_CHIP;
extern const ChipInfo TCS3448_CHIP;
#endif

class AS734XComponent : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_AS734X_RGB
  SUB_TEXT_SENSOR(rgb_hex)
#endif

 public:
  // Channel correction is multiplicative, so it starts at unity rather than zero in case no
  // calibration is configured.
  explicit AS734XComponent(const ChipInfo *chip) : chip_(chip) { this->channel_correction_.fill(1.0f); }

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_atime(uint8_t atime) { this->atime_ = atime; }
  void set_astep(uint16_t astep) { this->astep_ = astep; }

  /// Turns the sensor's LED driver on or off. Meant for lambdas.
  void enable_led(bool enable);

  // Calibration applied while turning raw counts into basic counts.
  void set_dark_current(const std::array<float, NUM_BANDS> &values) { this->dark_current_ = values; }
  void set_channel_correction(const std::array<float, NUM_BANDS> &values) { this->channel_correction_ = values; }
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  /// Chooses what the published basic counts are divided by.
  void set_basic_counts_normalization(Normalization normalization) { this->normalization_ = normalization; }

  void set_counts_sensor(sensor::Sensor *sensor, uint8_t band) { this->sensors_[band] = sensor; }
  void set_basic_counts_sensor(sensor::Sensor *sensor, uint8_t band) { this->basic_counts_sensors_[band] = sensor; }

  SUB_SENSOR(saturation_level)
  SUB_SENSOR(illuminance)
  SUB_SENSOR(irradiance_photopic)
  SUB_SENSOR(irradiance_par)
  SUB_SENSOR(ppfd)
  SUB_SENSOR(color_temperature)

 protected:
  bool select_bank_(uint8_t reg);
  bool update_bits_(uint8_t reg, uint8_t mask, bool value);
  bool write_exposure_();
  bool start_step_();
  void read_step_();
  void abort_measurement_(const LogString *reason);
  void calculate_basic_counts_();
  void publish_light_metrics_();
  void publish_color_();
  void publish_measurement_();
  void publish_basic_counts_();
  float normalization_divisor_() const;

  std::array<sensor::Sensor *, NUM_BANDS> sensors_{};
  std::array<sensor::Sensor *, NUM_BANDS> basic_counts_sensors_{};
  std::array<float, NUM_BANDS> dark_current_{};
  std::array<float, NUM_BANDS> channel_correction_;
  std::array<float, NUM_BANDS> basic_counts_{};
  std::array<uint16_t, NUM_BANDS> raw_counts_{};
  const ChipInfo *chip_;  // in flash, read field by field with progmem_memcpy
  uint32_t start_ms_{0};
  uint32_t integration_ms_{0};
  uint32_t timeout_ms_{0};
  float glass_attenuation_factor_{1.0f};
  float max_basic_count_{0.0f};       // largest of every channel
  float max_band_basic_count_{0.0f};  // largest of the visible bands only
  float clear_basic_count_{0.0f};     // the wideband clear channel
  uint16_t astep_{0};
  uint16_t peak_raw_count_{0};  // highest ADC value of this measurement, before the clear channels are averaged
  // What the running measurement was started with, so the maths matches the data even if the
  // configuration is changed while a measurement is in flight.
  uint16_t reading_astep_{0};
  Gain reading_gain_{GAIN_1X};
  uint8_t reading_atime_{0};
  enum class State : uint8_t { STATE_IDLE, STATE_WAIT_SMUX, STATE_READ_DATA } state_{State::STATE_IDLE};
  Normalization normalization_{Normalization::NORMALIZATION_NONE};
  Gain gain_{GAIN_1X};
  uint8_t atime_{0};
  uint8_t step_{0};
  bool bank_low_{true};  // unknown until setup() writes CFG0
};

}  // namespace esphome::as734x
