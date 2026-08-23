#pragma once

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_AS734X_RGB
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::as734x {

enum class Model : uint8_t {
  AS7341 = 41,
  AS7343 = 43,
  TCS3448 = 48,  // AS7343 register map and calibration, different I2C address
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

// Number of entries in Gain, for sizing the per-gain correction tables.
constexpr uint8_t GAIN_COUNT = 13;

// What the published basic counts are divided by. The clear channel is wideband, so it is roughly
// the size of all the bands together, and the near infrared channel outgrows them under daylight.
// Dividing by the largest of every channel therefore leaves the visible bands squeezed into the
// bottom of the range.
enum class Normalization : uint8_t {
  NONE,   // publish absolute basic counts
  ALL,    // largest of every channel
  BANDS,  // largest of the visible bands only
  CLEAR,  // the wideband clear channel
};

// Nominal centre wavelengths in nanometres bound the visible bands. The clear channel is wideband
// and has no single centre wavelength, so it is marked with zero instead.
constexpr uint16_t VISIBLE_MIN_NM = 380;
constexpr uint16_t VISIBLE_MAX_NM = 700;
constexpr uint16_t WIDEBAND_NM = 0;

constexpr uint8_t MAX_CHANNELS = 13;  // AS7343 reports 13 bands, AS7341 reports 10

using ChannelValuesUint16 = std::array<uint16_t, MAX_CHANNELS>;
using ChannelValuesFloat = std::array<float, MAX_CHANNELS>;
using SensorArray = std::array<sensor::Sensor *, MAX_CHANNELS>;

// Luminous efficacy at 555 nm, for turning photopic irradiance into lux.
constexpr float LUMENS_PER_WATT = 683.002f;

// How much one basic count of a channel contributes to each integrated quantity. The factors come
// from the manufacturer's golden-device calibration, convolved for a reconstructed spectrum.
struct ChannelContribution {
  float irradiance_photopic;  // weighted by the CIE 1931 V(lambda) curve
  float irradiance_par;       // 400-700 nm only
  float ppfd;                 // 400-700 nm only, scaled by 1e3 before publishing
};

// CIE 1931 tristimulus contribution of one basic count of a channel.
struct ChannelTristimulus {
  float x;
  float y;
  float z;
};

union RegAStatus {
  uint8_t raw;
  struct {
    Gain again_status : 4;
    uint8_t reserved_4_6 : 3;
    uint8_t asat_status : 1;
  } __attribute__((packed));
};

struct RegisterMap {
  uint8_t bank_low_min;  // CFG0's bank bit enables this window; other registers answer from either bank
  uint8_t bank_low_max;
  uint8_t astep;
  uint8_t atime;
  uint8_t cfg0;
  uint8_t cfg0_reg_bank_bit;
  uint8_t cfg1;
  uint8_t enable;
  uint8_t enable_pon_bit;
  uint8_t enable_sp_en_bit;
  uint8_t enable_smux_en_bit;
  uint8_t led;
  uint8_t led_act_bit;
  uint8_t status2;
  uint8_t status2_avalid_bit;
};

class AS734xBase {
 public:
  AS734xBase(i2c::I2CDevice *i2c_device, uint8_t number_of_channels);
  virtual ~AS734xBase() = default;

  uint8_t get_number_of_channels() const { return this->number_of_channels_; }

  virtual bool verify_device_id() = 0;
  virtual bool write_default_config() = 0;

  bool write_gain(Gain gain);
  bool write_atime(uint8_t atime);
  bool write_astep(uint16_t astep);

  bool enable_power(bool enable);
  bool enable_spectral_measurement(bool enable);
  virtual bool enable_led(bool enable);

  // Per-gain correction factor for a channel, from the chip's own calibration table.
  virtual float get_gain_correction(uint8_t channel, Gain gain) const = 0;

  // Nominal centre wavelength of a channel in nanometres, or WIDEBAND_NM for the clear channel.
  virtual uint16_t get_channel_wavelength(uint8_t channel) const = 0;
  virtual ChannelContribution get_channel_contribution(uint8_t channel) const = 0;
  virtual ChannelTristimulus get_channel_tristimulus(uint8_t channel) const = 0;

  virtual uint8_t get_number_of_smux_steps() const = 0;
  virtual uint8_t get_integration_cycles() const = 0;
  virtual bool prepare_for_smux_step(uint8_t step) = 0;

  virtual bool enable_smux();
  virtual bool is_smux_busy();
  virtual bool is_data_ready();

  virtual bool read_channels(uint8_t step, ChannelValuesUint16 &values, bool &saturated) = 0;

  // Highest ADC value of the last read, taken before the AS7343 averages its two clear cycles.
  uint16_t get_peak_raw_count() const { return this->peak_raw_count_; }

 protected:
  virtual const RegisterMap &registers() const = 0;

  uint16_t peak_raw_count_{0};
  i2c::I2CDevice *i2c_device_ = nullptr;
  uint8_t number_of_channels_;

  bool read_byte_(uint8_t address, uint8_t *value);
  bool write_byte_(uint8_t address, uint8_t value);
  bool select_low_bank_(bool low);  // a low register reached from bank 0 is acked but never written
  bool needs_low_bank_(uint8_t address) const {
    return address >= this->registers().bank_low_min && address <= this->registers().bank_low_max;
  }

  inline uint16_t swap_bytes_(uint16_t data) { return (data >> 8) | (data << 8); }
  bool read_register_bit_(uint8_t address, uint8_t bit_position, bool &bit_value);
  bool write_register_bit_(uint8_t address, bool value, uint8_t bit_position);
  bool set_register_bit_(uint8_t address, uint8_t bit_position);
  bool clear_register_bit_(uint8_t address, uint8_t bit_position);
  bool update_register_bit_(uint8_t address, uint8_t bit_position, bool value);
};

class AS734XComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  // Channel correction is multiplicative, so it has to start at unity rather than zero in case no
  // calibration is configured.
  AS734XComponent() { this->channel_correction_.fill(1.0f); }

  void setup_model(Model model);

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_atime(uint8_t atime) { this->atime_ = atime; }
  void set_astep(uint16_t astep) { this->astep_ = astep; }

  // Turns the sensor's LED driver on or off. Meant for lambdas.
  void enable_led(bool enable);

  // Calibration applied while turning raw counts into basic counts.
  void set_dark_current(const ChannelValuesFloat &values) { this->dark_current_ = values; }
  void set_channel_correction(const ChannelValuesFloat &values) { this->channel_correction_ = values; }
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }

  // Chooses what the published basic counts are divided by.
  void set_normalize_basic_counts(Normalization normalization) { this->normalization_ = normalization; }

#ifdef USE_SENSOR
  SUB_SENSOR(saturation_level)
  SUB_SENSOR(illuminance)
  SUB_SENSOR(irradiance_photopic)
  SUB_SENSOR(irradiance_par)
  SUB_SENSOR(ppfd)
  SUB_SENSOR(color_temperature)

 protected:
  SensorArray band_counts_sensors_{};
  SensorArray band_basic_counts_sensors_{};

 public:
  void set_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < this->band_counts_sensors_.size()) {
      this->band_counts_sensors_[channel] = sensor;
    }
  }
  void set_basic_counts_sensor(sensor::Sensor *sensor, uint8_t channel) {
    if (channel < this->band_basic_counts_sensors_.size()) {
      this->band_basic_counts_sensors_[channel] = sensor;
    }
  }
#endif

#ifdef USE_AS734X_RGB
 protected:
  text_sensor::TextSensor *rgb_hex_sensor_{nullptr};
  char rgb_hex_[7]{"000000"};  // valid before the first measurement completes

 public:
  void set_rgb_hex_sensor(text_sensor::TextSensor *sensor) { this->rgb_hex_sensor_ = sensor; }
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
    CALCULATE,
    READY_TO_PUBLISH,
  } state_{State::NOT_INITIALIZED};

  uint16_t astep_{0};
  Gain gain_{GAIN_1X};
  uint8_t atime_{0};

  float glass_attenuation_factor_{1.0f};
  Normalization normalization_{Normalization::NONE};
  ChannelValuesFloat dark_current_{};
  ChannelValuesFloat channel_correction_{};

  struct {
    ChannelValuesUint16 raw_counts{};
    Gain gain{GAIN_1X};
    uint8_t atime{0};
    uint16_t astep{0};
    uint32_t millis_start{0};
    uint32_t timeout_ms{0};
    uint8_t smux_step{0};
  } readings_;

  struct {
    ChannelValuesFloat basic_counts{};
    float max_basic_count{0.0f};       // largest of every channel
    float max_band_basic_count{0.0f};  // largest of the visible bands only
    float clear_basic_count{0.0f};     // the wideband clear channel
    float saturation_level{0.0f};
    float irradiance_photopic{0.0f};
    float irradiance_par{0.0f};
    float ppfd{0.0f};
    float illuminance{0.0f};
    float color_temperature{0.0f};
  } calculated_;

  void calculate_basic_counts_();
  void calculate_saturation_level_();
  void calculate_light_metrics_();
  void calculate_color_();

  void publish_channel_readings_();
  void abort_measurement_(const char *reason);
  void publish_basic_counts_();
  float normalization_divisor_() const;
  void publish_light_metrics_();
};

}  // namespace esphome::as734x
