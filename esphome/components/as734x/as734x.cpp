#include "as734x.h"
#include "color_helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>
#include <cstdio>

namespace esphome::as734x {

ESPHOME_LOG_TAG(TAG, "as734x");

// Registers and bits shared by both chips
static constexpr uint8_t REG_ENABLE = 0x80;
static constexpr uint8_t REG_ATIME = 0x81;
static constexpr uint8_t REG_ASTATUS = 0x94;
static constexpr uint8_t ENABLE_PON = 0x01;
static constexpr uint8_t ENABLE_SP_EN = 0x02;
static constexpr uint8_t ENABLE_SMUX_EN = 0x10;
static constexpr uint8_t CFG0_REG_BANK = 0x10;
static constexpr uint8_t BANK_SPLIT = 0x80;  // registers below this need REG_BANK set
static constexpr uint8_t STATUS2_AVALID = 0x40;
static constexpr uint8_t ASTATUS_ASAT = 0x80;
static constexpr uint8_t LED_ACT = 0x80;  // LED driver on, in the chip's LED register
static constexpr uint8_t CONFIG_LED_SEL = 0x08;

static constexpr uint8_t AS7341_CFG6 = 0xAF;
static constexpr uint8_t AS7341_SMUX_CMD_WRITE = 0x10;  // copy the SMUX image from RAM to the chain
static constexpr uint8_t AS7341_SMUX_RAM = 0x00;
static constexpr uint8_t AS7343_CHAIN_CMD = 0xE4;
static constexpr uint8_t AS7343_CHAIN_SMUX = 0xE7;
static constexpr uint8_t AS7343_CHAINS = 3;

// Read one field of a ChipInfo kept in flash
template<typename T> static T pgm(const T &field) {
  T value;
  progmem_memcpy(&value, &field, sizeof(T));
  return value;
}

static constexpr uint8_t NO_SLOT = 0xFF;
static constexpr uint8_t CLEAR_BAND = NUM_BANDS - 1;
static constexpr uint8_t MAX_SMUX_LEN = 20;
static constexpr uint8_t MAX_ADC_SLOTS = 18;
static constexpr uint32_t MIN_COLLECTION_TIMEOUT_MS = 30 * 1000;
static constexpr uint32_t MAX_ADC_COUNT = 65535;

PROGMEM_STRING_TABLE(BandNames, "F1", "F2", "FZ", "F3", "F4", "FY", "F5", "FXL", "F6", "F7", "F8", "NIR", "Clear");

#ifdef USE_AS7341
static const char AS7341_NAME[] PROGMEM = "AS7341";
static constexpr uint8_t AS7341_ADC_SLOTS = 6;
static constexpr uint8_t AS7341_SMUX_LEN = 20;
static constexpr uint8_t AS7341_SMUX_STEPS = 2;
static const RegValue AS7341_INIT[] PROGMEM = {{0x70, 0x00}};  // CONFIG: SPM mode, INT_SEL and LED_SEL cleared
static const uint8_t AS7341_SMUX[] PROGMEM = {
    0x30, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x50, 0x00,  // step 0: F1 to F4, NIR, Clear
    0x00, 0x00, 0x20, 0x04, 0x00, 0x30, 0x01, 0x50, 0x00, 0x06,  // step 0, continued
    0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x10, 0x03, 0x50, 0x10,  // step 1: F5 to F8, NIR, Clear
    0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x50, 0x00, 0x06,  // step 1, continued
};
static_assert(sizeof(AS7341_SMUX) == AS7341_SMUX_STEPS * AS7341_SMUX_LEN && AS7341_SMUX_LEN <= MAX_SMUX_LEN);
static constexpr uint8_t N = NO_SLOT;
static const uint8_t AS7341_READOUT[] PROGMEM = {
    0, 1, N, 2, 3, N, N, N, N, N, N, N, N,  // step 0
    N, N, N, N, N, N, 0, N, 1, 2, 3, 5, 4,  // step 1: SMUX routes clear to ADC4 and NIR to ADC5
};
static_assert(sizeof(AS7341_READOUT) == AS7341_SMUX_STEPS * NUM_BANDS && AS7341_ADC_SLOTS <= MAX_ADC_SLOTS);
// This chip has no FZ, FY or FXL band, so those slots are marked absent.
// clang-format off
static const uint16_t AS7341_WAVELENGTHS[] PROGMEM = {
    415, 445, ABSENT_NM, 480, 515, ABSENT_NM, 555, ABSENT_NM, 590, 630, 680, 910, WIDEBAND_NM,
};
// clang-format on
static_assert(sizeof(AS7341_WAVELENGTHS) == NUM_BANDS * sizeof(uint16_t));
// Datasheet values for the golden device: gain correction per gain step, the same for every band.
// The last two entries stay at zero because this chip has no 1024x or 2048x gain.
static const float AS7341_GAIN_CORRECTION[] PROGMEM = {1.0577f, 1.0491f, 1.0479f, 1.0491f, 1.0207f, 1.0158f, 1.0109f,
                                                       1.0000f, 1.0003f, 0.9873f, 0.9593f, 0.0f,    0.0f};
static_assert(sizeof(AS7341_GAIN_CORRECTION) == GAIN_COUNT * sizeof(float));
// clang-format off
// The three bands this chip does not have contribute nothing to the weighted sums.
static const ChannelContribution AS7341_CONTRIBUTIONS[] PROGMEM = {
    {0.005261294f, 5.936230324f, 0.020518273f},     // F1
    {0.242615252f, 3.572528495f, 0.013525490f},     // F2
    {0.0f, 0.0f, 0.0f},                             // FZ, absent
    {0.174482552f, 2.914113437f, 0.011585145f},     // F3
    {1.479499075f, 2.452851227f, 0.010604986f},     // F4
    {0.0f, 0.0f, 0.0f},                             // FY, absent
    {1.906818555f, 1.940656433f, 0.008890531f},     // F5
    {0.0f, 0.0f, 0.0f},                             // FXL, absent
    {1.162058142f, 1.533976606f, 0.007441544f},     // F6
    {0.462374845f, 1.819838250f, 0.009393590f},     // F7
    {-0.017794572f, 1.232942274f, 0.006890812f},    // F8
    {-0.260730901f, -1.102368000f, -0.004647911f},  // NIR
    {-0.022604075f, -0.033304813f, -0.000129106f},  // Clear
};
static const ChannelTristimulus AS7341_TRISTIMULUS[] PROGMEM = {
    {0.39814f, 0.01396f, 1.95010f},     // F1
    {1.29540f, 0.16748f, 6.45490f},     // F2
    {0.0f, 0.0f, 0.0f},                 // FZ, absent
    {0.36956f, 0.23538f, 2.78010f},     // F3
    {0.10902f, 1.42750f, 0.18501f},     // F4
    {0.0f, 0.0f, 0.0f},                 // FY, absent
    {0.71942f, 1.88670f, 0.15325f},     // F5
    {0.0f, 0.0f, 0.0f},                 // FXL, absent
    {1.78180f, 1.14200f, 0.09539f},     // F6
    {1.10110f, 0.46497f, 0.10563f},     // F7
    {-0.03991f, -0.02702f, 0.08866f},   // F8
    {-0.27597f, -0.24468f, -0.61140f},  // NIR
    {-0.02347f, -0.01993f, -0.00938f},  // Clear
};
// clang-format on
static_assert(sizeof(AS7341_CONTRIBUTIONS) == NUM_BANDS * sizeof(ChannelContribution));
static_assert(sizeof(AS7341_TRISTIMULUS) == NUM_BANDS * sizeof(ChannelTristimulus));
const ChipInfo AS7341_CHIP PROGMEM = {
    .name = AS7341_NAME,
    .init = AS7341_INIT,
    .smux = AS7341_SMUX,
    .readout = AS7341_READOUT,
    .wavelengths = AS7341_WAVELENGTHS,
    .gain_correction = AS7341_GAIN_CORRECTION,
    .contributions = AS7341_CONTRIBUTIONS,
    .tristimulus = AS7341_TRISTIMULUS,
    .init_count = 1,
    .astep_reg = 0xCA,
    .cfg0_reg = 0xA9,
    .cfg1_reg = 0xAA,
    .status2_reg = 0xA3,
    .id_reg = 0x92,
    .id_mask = 0xFC,
    .id_value = 0x09 << 2,
    .led_reg = 0x74,
    .led_sel_reg = 0x70,  // CONFIG, whose LED_SEL bit connects the pin to the driver
    .adc_slots = AS7341_ADC_SLOTS,
    .smux_steps = AS7341_SMUX_STEPS,
    .integration_cycles = 1,
    .smux_len = AS7341_SMUX_LEN,
    .auto_smux = false,
    .gain_correction_per_band = false,
    .extra_clear_slot = NO_SLOT,
};
#endif

#ifdef USE_AS7343
static const char AS7343_NAME[] PROGMEM = "AS7343";
static constexpr uint8_t AS7343_ADC_SLOTS = 18;
static constexpr uint8_t AS7343_SMUX_LEN = 10;
static const char TCS3448_NAME[] PROGMEM = "TCS3448";
// CFG6, FD_CFG0, CFG10, CFG8, CFG20 (auto_smux, 18 channels in 3 cycles), AGC_GAIN_MAX, FD_TIME_1, FD_TIME_2
static const RegValue AS7343_INIT[] PROGMEM = {{0xF5, 0x00}, {0xDF, 0xA1}, {0x65, 0xF2}, {0xC9, 0xC8},
                                               {0xD6, 0x62}, {0xD7, 0x99}, {0xE0, 0x64}, {0xE2, 0x21}};
static const uint8_t AS7343_SMUX[] PROGMEM = {0x00, 0x04, 0x65, 0x02, 0x00, 0x05, 0x00, 0x01, 0x00, 0x30,
                                              0x00, 0x00, 0x60, 0x20, 0x04, 0x50, 0x03, 0x00, 0x01, 0x05,
                                              0x05, 0x00, 0x60, 0x00, 0x30, 0x00, 0x40, 0x10, 0x20, 0x00};
// Clear is the average of the cycle 1 (slot 4) and cycle 2 (slot 10) readings; cycle 3 always reads 0.
static const uint8_t AS7343_READOUT[] PROGMEM = {12, 6, 0, 7, 8, 1, 15, 2, 9, 13, 14, 3, 10};
static_assert(sizeof(AS7343_SMUX) == AS7343_CHAINS * AS7343_SMUX_LEN && AS7343_SMUX_LEN <= MAX_SMUX_LEN);
static_assert(sizeof(AS7343_READOUT) == NUM_BANDS && AS7343_ADC_SLOTS <= MAX_ADC_SLOTS);
// clang-format off
static const uint16_t AS7343_WAVELENGTHS[] PROGMEM = {
    405, 425, 450, 475, 515, 555, 550, 600, 640, 690, 745, 855, WIDEBAND_NM,
};
// clang-format on
static_assert(sizeof(AS7343_WAVELENGTHS) == NUM_BANDS * sizeof(uint16_t));
// Datasheet values for the golden device: gain correction per gain step, per band.
// clang-format off
static const float AS7343_GAIN_CORRECTION[] PROGMEM = {
    1.149f, 1.100f, 1.060f, 1.070f, 1.063f, 1.051f, 1.062f, 1.056f, 1.049f, 1.040f, 1.080f, 1.038f, 1.065f,  // 0.5x
    1.090f, 1.128f, 1.064f, 1.071f, 1.063f, 1.050f, 1.068f, 1.055f, 1.047f, 1.039f, 1.075f, 1.038f, 1.085f,  // 1x
    1.083f, 1.086f, 1.062f, 1.070f, 1.062f, 1.049f, 1.057f, 1.053f, 1.045f, 1.038f, 1.063f, 1.037f, 1.069f,  // 2x
    1.059f, 1.068f, 1.056f, 1.066f, 1.058f, 1.046f, 1.051f, 1.051f, 1.044f, 1.036f, 1.059f, 1.035f, 1.053f,  // 4x
    1.100f, 1.109f, 1.096f, 1.108f, 1.099f, 1.089f, 1.091f, 1.092f, 1.082f, 1.078f, 1.100f, 1.076f, 1.088f,  // 8x
    1.099f, 1.109f, 1.096f, 1.108f, 1.099f, 1.089f, 1.091f, 1.092f, 1.082f, 1.078f, 1.100f, 1.075f, 1.087f,  // 16x
    1.088f, 1.096f, 1.085f, 1.097f, 1.087f, 1.078f, 1.079f, 1.080f, 1.071f, 1.067f, 1.087f, 1.064f, 1.076f,  // 32x
    1.083f, 1.091f, 1.078f, 1.090f, 1.079f, 1.072f, 1.072f, 1.073f, 1.064f, 1.062f, 1.080f, 1.057f, 1.069f,  // 64x
    1.076f, 1.084f, 1.072f, 1.085f, 1.074f, 1.066f, 1.062f, 1.067f, 1.055f, 1.056f, 1.074f, 1.051f, 1.061f,  // 128x
    1.067f, 1.074f, 1.063f, 1.075f, 1.064f, 1.059f, 1.055f, 1.058f, 1.049f, 1.051f, 1.064f, 1.044f, 1.053f,  // 256x
    1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f, 1.000f,  // 512x
    1.033303857f, 1.034763813f, 1.029855013f, 1.036911249f, 0.988150537f, 1.029003501f, 1.021363258f,
    0.98762089f, 1.021713376f, 1.040108204f, 0.987417698f, 1.019481421f, 0.987270057f,  // 1024x
    0.986859143f, 0.980877221f, 0.9469769f, 0.985535324f, 0.926243961f, 0.959814787f, 1.007963896f,
    0.942685187f, 1.007630587f, 1.025460005f, 0.928696275f, 1.001296639f, 1.041275263f,  // 2048x
};
// clang-format on
static_assert(sizeof(AS7343_GAIN_CORRECTION) == GAIN_COUNT * NUM_BANDS * sizeof(float));
// clang-format off
static const ChannelContribution AS7343_CONTRIBUTIONS[] PROGMEM = {
    {-0.028559089f, 0.450293995f, 0.001266925f},    // F1
    {-0.042150948f, 3.566379984f, 0.012515925f},    // F2
    {0.000661721f, 2.877424672f, 0.010243005f},     // FZ
    {-0.108791278f, 2.273837581f, 0.009376157f},    // F3
    {-0.167172180f, 1.519298503f, 0.006017556f},    // F4
    {5.726660294f, 4.199345859f, 0.019357905f},     // FY
    {0.0f, 0.0f, 0.0f},                             // F5
    {-0.241492014f, 1.185259160f, 0.006516325f},    // FXL
    {-0.047665296f, 1.759469826f, 0.009780714f},    // F6
    {-0.012516990f, 1.702006611f, 0.009950228f},    // F7
    {-0.026424974f, -1.059196137f, -0.005425656f},  // F8
    {-0.007565221f, -0.064827881f, -0.000232267f},  // NIR
    {0.0f, 0.0f, 0.0f},                             // Clear
};
static const ChannelTristimulus AS7343_TRISTIMULUS[] PROGMEM = {
    {-0.07879f, -0.03269f, -0.31295f},  // F1
    {-0.12235f, -0.01297f, -0.57885f},  // F2
    {1.99879f, -0.04011f, 10.00197f},   // FZ
    {-0.33364f, -0.06889f, -0.31281f},  // F3
    {-0.06795f, -0.17453f, -0.19657f},  // F4
    {-0.45419f, 5.69083f, -0.11077f},   // FY
    {0.0f, 0.0f, 0.0f},                 // F5
    {5.27242f, -0.22956f, -0.06132f},   // FXL
    {-0.05072f, -0.04762f, -0.03536f},  // F6
    {-0.04666f, -0.01295f, -0.06025f},  // F7
    {-0.03931f, -0.02797f, -0.12174f},  // F8
    {0.0f, 0.0f, 0.0f},                 // NIR
    {0.0f, 0.0f, 0.0f},                 // Clear
};
// clang-format on
static_assert(sizeof(AS7343_CONTRIBUTIONS) == NUM_BANDS * sizeof(ChannelContribution));
static_assert(sizeof(AS7343_TRISTIMULUS) == NUM_BANDS * sizeof(ChannelTristimulus));
static constexpr ChipInfo as7343_chip(const char *name) {
  return {
      .name = name,
      .init = AS7343_INIT,
      .smux = AS7343_SMUX,
      .readout = AS7343_READOUT,
      .wavelengths = AS7343_WAVELENGTHS,
      .gain_correction = AS7343_GAIN_CORRECTION,
      .contributions = AS7343_CONTRIBUTIONS,
      .tristimulus = AS7343_TRISTIMULUS,
      .init_count = sizeof(AS7343_INIT) / sizeof(RegValue),
      .astep_reg = 0xD4,
      .cfg0_reg = 0xBF,
      .cfg1_reg = 0xC6,
      .status2_reg = 0x90,
      .id_reg = 0x5A,
      .id_mask = 0xFF,
      .id_value = 0x81,
      .led_reg = 0xCD,
      .led_sel_reg = 0,  // the LED register drives the pin on its own
      .adc_slots = AS7343_ADC_SLOTS,
      .smux_steps = 1,
      .integration_cycles = 3,
      .smux_len = AS7343_SMUX_LEN,
      .auto_smux = true,
      .gain_correction_per_band = true,
      .extra_clear_slot = 4,
  };
}
const ChipInfo AS7343_CHIP PROGMEM = as7343_chip(AS7343_NAME);
const ChipInfo TCS3448_CHIP PROGMEM = as7343_chip(TCS3448_NAME);  // AS7343 register map, address 0x59
#endif

// Registers below 0x80 are only reachable with the REG_BANK bit of CFG0 set; CFG0 itself always answers.
bool AS734XComponent::select_bank_(uint8_t reg) {
  const bool low = reg < BANK_SPLIT;
  if (low == this->bank_low_) {
    return true;
  }
  this->bank_low_ = low;
  return this->write_byte(pgm(this->chip_->cfg0_reg), low ? CFG0_REG_BANK : 0);
}

bool AS734XComponent::update_bits_(uint8_t reg, uint8_t mask, bool value) {
  return this->select_bank_(reg) && this->update_register(reg, mask, value ? mask : 0);
}

// The exposure is applied before every measurement, so a gain, ATIME or ASTEP changed at runtime
// takes effect on the next reading rather than only after a restart.
bool AS734XComponent::write_exposure_() {
  const ChipInfo *chip = this->chip_;
  if (!this->select_bank_(REG_ATIME) || !this->write_byte(REG_ATIME, this->atime_) ||
      !this->write_byte_16(pgm(chip->astep_reg), byteswap(this->astep_)) ||
      !this->write_byte(pgm(chip->cfg1_reg), this->gain_)) {
    return false;
  }
  this->reading_gain_ = this->gain_;
  this->reading_atime_ = this->atime_;
  this->reading_astep_ = this->astep_;
  // One integration cycle lasts (ATIME + 1) * (ASTEP + 1) * 2.78 us.
  const uint32_t cycle_ms = (this->atime_ + 1UL) * (this->astep_ + 1UL) * 139 / 50000;
  this->integration_ms_ = cycle_ms * pgm(chip->integration_cycles);
  this->timeout_ms_ = std::max(MIN_COLLECTION_TIMEOUT_MS, 2 * this->integration_ms_);
  return true;
}

void AS734XComponent::setup() {
  const ChipInfo *chip = this->chip_;
  const uint8_t id_reg = pgm(chip->id_reg);
  uint8_t id = 0;
  // The sensor keeps its bank across an MCU reset, so the first access always writes CFG0
  bool ok = this->select_bank_(REG_ENABLE) && this->write_byte(REG_ENABLE, ENABLE_PON);
  delayMicroseconds(200);  // oscillator start-up
  ok = ok && this->select_bank_(id_reg) && this->read_byte(id_reg, &id);
  if (ok && (id & pgm(chip->id_mask)) != pgm(chip->id_value)) {
    this->mark_failed(LOG_STR("Wrong chip ID"));
    return;
  }
  const RegValue *init = pgm(chip->init);
  for (uint8_t i = 0, n = pgm(chip->init_count); ok && i < n; i++) {
    RegValue rv;
    progmem_memcpy(&rv, &init[i], sizeof(rv));
    ok = this->select_bank_(rv.reg) && this->write_byte(rv.reg, rv.value);
  }
  // Everything from here on, including the measurement registers, is in the high bank.
  ok = ok && this->write_exposure_();
  if (pgm(chip->auto_smux)) {
    const uint8_t *smux = pgm(chip->smux);
    const uint8_t len = pgm(chip->smux_len);
    for (uint8_t i = 0; ok && i < AS7343_CHAINS * len; i++) {
      ok = this->write_byte(AS7343_CHAIN_SMUX, progmem_read_byte(&smux[i])) &&
           this->write_byte(AS7343_CHAIN_CMD, 0x46 + (i / len) * 0x10);
    }
  }
  if (!ok) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->disable_loop();
}

void AS734XComponent::dump_config() {
  const float gain = this->gain_ == GAIN_0_5X ? 0.5f : static_cast<float>(1 << (this->gain_ - 1));
  ESP_LOGCONFIG(TAG,
                "AS734x:\n"
                "  Chip: %s\n"
                "  Gain: %gx\n"
                "  ATIME: %u\n"
                "  ASTEP: %u\n"
                "  Glass attenuation factor: %.3f",
                LOG_STR_ARG(pgm(this->chip_->name)), gain, this->atime_, this->astep_, this->glass_attenuation_factor_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    char label[24];
    const char *name = LOG_STR_ARG(BandNames::get_log_str(band, BandNames::LAST_INDEX));
    snprintf(label, sizeof(label), "Counts %s", name);
    sensor::log_sensor(TAG, "  ", label, this->sensors_[band]);
    snprintf(label, sizeof(label), "Basic counts %s", name);
    sensor::log_sensor(TAG, "  ", label, this->basic_counts_sensors_[band]);
  }
  LOG_SENSOR("  ", "Saturation level", this->saturation_level_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Irradiance photopic", this->irradiance_photopic_sensor_);
  LOG_SENSOR("  ", "Irradiance PAR", this->irradiance_par_sensor_);
  LOG_SENSOR("  ", "PPFD", this->ppfd_sensor_);
  LOG_SENSOR("  ", "Color temperature", this->color_temperature_sensor_);
#ifdef USE_AS734X_RGB
  LOG_TEXT_SENSOR("  ", "RGB hex", this->rgb_hex_sensor_);
#endif
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void AS734XComponent::enable_led(bool enable) {
  const uint8_t led_reg = pgm(this->chip_->led_reg);
  if (led_reg == 0) {
    return;
  }
  // The LED register only drives the LED while LED_SEL is set, so the pin is handed over before
  // switching on and released only after switching off. The other order would leave the LED lit
  // with nothing able to turn it off.
  const uint8_t led_sel_reg = pgm(this->chip_->led_sel_reg);
  bool ok = true;
  if (enable && led_sel_reg != 0) {
    ok = this->update_bits_(led_sel_reg, CONFIG_LED_SEL, true);
  }
  ok = this->update_bits_(led_reg, LED_ACT, enable) && ok;
  if (!enable && led_sel_reg != 0) {
    ok = this->update_bits_(led_sel_reg, CONFIG_LED_SEL, false) && ok;
  }
  // The measurement path assumes the high bank, so leave the chip there for a reading in flight
  ok = this->select_bank_(REG_ENABLE) && ok;
  if (!ok) {
    ESP_LOGW(TAG, "Failed to switch the LED");
  }
}

void AS734XComponent::update() {
  if (this->state_ != State::STATE_IDLE) {
    ESP_LOGW(TAG, "Skipping update, previous measurement still running");
    return;
  }
  if (!this->write_exposure_()) {
    this->abort_measurement_(LOG_STR("Failed to apply measurement settings"));
    return;
  }
  this->step_ = 0;
  this->peak_raw_count_ = 0;
  this->raw_counts_.fill(0);
  this->enable_loop();
  if (!this->start_step_()) {
    this->abort_measurement_(LOG_STR("Failed to start measurement"));
  }
}

bool AS734XComponent::start_step_() {
  this->start_ms_ = App.get_loop_component_start_time();
  if (pgm(this->chip_->auto_smux)) {
    this->state_ = State::STATE_READ_DATA;
    return this->write_byte(REG_ENABLE, ENABLE_PON | ENABLE_SP_EN);
  }
  // Load this step's image into SMUX RAM; SMUX_EN copies it to the chain and clears itself when done.
  std::array<uint8_t, MAX_SMUX_LEN> image{};
  const uint8_t len = pgm(this->chip_->smux_len);
  progmem_memcpy(image.data(), pgm(this->chip_->smux) + this->step_ * len, len);
  this->state_ = State::STATE_WAIT_SMUX;
  return this->write_byte(AS7341_CFG6, AS7341_SMUX_CMD_WRITE) &&
         this->write_bytes(AS7341_SMUX_RAM, image.data(), len) &&
         this->write_byte(REG_ENABLE, ENABLE_PON | ENABLE_SMUX_EN);
}

void AS734XComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  uint8_t value = 0;
  switch (this->state_) {
    case State::STATE_WAIT_SMUX:
      // A failed read counts as busy, so the timeout below catches a chip that stops answering.
      if (this->read_byte(REG_ENABLE, &value) && !(value & ENABLE_SMUX_EN)) {
        this->start_ms_ = now;
        this->state_ = State::STATE_READ_DATA;
        if (!this->write_byte(REG_ENABLE, ENABLE_PON | ENABLE_SP_EN)) {
          this->abort_measurement_(LOG_STR("Failed to start integration"));
        }
        return;
      }
      break;
    case State::STATE_READ_DATA:
      if (now - this->start_ms_ < this->integration_ms_) {
        return;
      }
      if (this->read_byte(pgm(this->chip_->status2_reg), &value) && (value & STATUS2_AVALID)) {
        this->read_step_();
        return;
      }
      break;
    case State::STATE_IDLE:
      this->disable_loop();
      return;
  }
  if (now - this->start_ms_ > this->timeout_ms_) {
    this->abort_measurement_(LOG_STR("Measurement timeout"));
  }
}

void AS734XComponent::read_step_() {
  // Reading ASTATUS latches the data, which is only consistent within one burst starting there.
  std::array<uint8_t, 1 + 2 * MAX_ADC_SLOTS> frame{};
  if (!this->read_bytes(REG_ASTATUS, frame.data(), 1 + 2 * pgm(this->chip_->adc_slots)) ||
      !this->write_byte(REG_ENABLE, ENABLE_PON)) {
    this->abort_measurement_(LOG_STR("Failed to read channel data"));
    return;
  }
  if (frame[0] & ASTATUS_ASAT) {
    ESP_LOGV(TAG, "Step %u saturated", this->step_);
  }
  auto slot_value = [&frame](uint8_t slot) { return encode_uint16(frame[2 + 2 * slot], frame[1 + 2 * slot]); };
  const uint8_t *readout = pgm(this->chip_->readout) + this->step_ * NUM_BANDS;
  const uint8_t extra_clear = pgm(this->chip_->extra_clear_slot);
  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    const uint8_t slot = progmem_read_byte(&readout[band]);
    if (slot == NO_SLOT) {
      continue;
    }
    const uint16_t raw = slot_value(slot);
    this->peak_raw_count_ = std::max(this->peak_raw_count_, raw);
    uint16_t counts = raw;
    if (band == CLEAR_BAND && extra_clear != NO_SLOT) {
      const uint16_t extra = slot_value(extra_clear);
      this->peak_raw_count_ = std::max(this->peak_raw_count_, extra);
      counts = (static_cast<uint32_t>(raw) + extra) / 2;
    }
    this->raw_counts_[band] = counts;
  }
  if (++this->step_ < pgm(this->chip_->smux_steps)) {
    if (!this->start_step_()) {
      this->abort_measurement_(LOG_STR("Failed to configure SMUX"));
    }
    return;
  }
  this->publish_measurement_();
  this->status_clear_warning();
  this->state_ = State::STATE_IDLE;
  this->disable_loop();
}

// A basic count is the raw count scaled to one unit of gain and one millisecond of integration
// time, which makes readings taken with different settings comparable. The manufacturer's
// conversion factors are calibrated against that definition.
void AS734XComponent::calculate_basic_counts_() {
  this->basic_counts_.fill(0.0f);
  this->max_basic_count_ = 0.0f;
  this->max_band_basic_count_ = 0.0f;
  this->clear_basic_count_ = 0.0f;

  // The gain enum counts in powers of two starting at 0.5x, so the multiplier is 2^gain / 2.
  const float gain_x = static_cast<float>(1 << this->reading_gain_) / 2.0f;
  const float integration_ms = (1.0f + this->reading_atime_) * (1.0f + this->reading_astep_) * 2.78e-3f;
  if (gain_x <= 0.0f || integration_ms <= 0.0f) {
    return;
  }
  const float inv_exposure = 1.0f / (gain_x * integration_ms);
  const ChipInfo *chip = this->chip_;
  const uint16_t *wavelengths = pgm(chip->wavelengths);
  const float *gain_correction = pgm(chip->gain_correction);
  const bool per_band = pgm(chip->gain_correction_per_band);
  const uint8_t correction_base = per_band ? this->reading_gain_ * NUM_BANDS : this->reading_gain_;

  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    const uint16_t wavelength = pgm(wavelengths[band]);
    if (wavelength == ABSENT_NM) {
      continue;
    }
    float basic_count = this->raw_counts_[band] * inv_exposure;
    basic_count -= this->dark_current_[band];
    basic_count = std::max(basic_count, 0.0f);
    basic_count *= pgm(gain_correction[per_band ? correction_base + band : correction_base]);
    basic_count *= this->channel_correction_[band];
    basic_count *= this->glass_attenuation_factor_;

    this->basic_counts_[band] = basic_count;
    this->max_basic_count_ = std::max(this->max_basic_count_, basic_count);
    if (wavelength == WIDEBAND_NM) {
      this->clear_basic_count_ = basic_count;
    } else if (wavelength >= VISIBLE_MIN_NM && wavelength <= VISIBLE_MAX_NM) {
      this->max_band_basic_count_ = std::max(this->max_band_basic_count_, basic_count);
    }
  }
}

float AS734XComponent::normalization_divisor_() const {
  float divisor = 1.0f;
  switch (this->normalization_) {
    case Normalization::NORMALIZATION_ALL:
      divisor = this->max_basic_count_;
      break;
    case Normalization::NORMALIZATION_BANDS:
      divisor = this->max_band_basic_count_;
      break;
    case Normalization::NORMALIZATION_CLEAR:
      divisor = this->clear_basic_count_;
      break;
    case Normalization::NORMALIZATION_NONE:
      return 1.0f;
  }
  // In the dark the reference can be zero, and dividing by that would publish infinities.
  return divisor > 0.0f ? divisor : 1.0f;
}

void AS734XComponent::publish_basic_counts_() {
  // Normalization describes the shape of the spectrum rather than its absolute level. Channels
  // outside the chosen reference can exceed 1.0 and are left that way rather than clipped: with
  // BANDS the near infrared channel legitimately sits well above the visible bands.
  const float scale = 1.0f / this->normalization_divisor_();
  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    if (this->basic_counts_sensors_[band] != nullptr) {
      this->basic_counts_sensors_[band]->publish_state(this->basic_counts_[band] * scale);
    }
  }
}

// Each band contributes a fixed amount per basic count to every integrated quantity, so the
// totals are a weighted sum over the bands.
// The weighted sums can go negative under light the tables were not fitted for, such as pure blue or
// far red, so the published values are floored at zero.
void AS734XComponent::publish_light_metrics_() {
  float irradiance_photopic = 0.0f;
  float irradiance_par = 0.0f;
  float ppfd = 0.0f;

  const ChannelContribution *contributions = pgm(this->chip_->contributions);
  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    const float basic_count = this->basic_counts_[band];
    const ChannelContribution contribution = pgm(contributions[band]);

    irradiance_photopic += contribution.irradiance_photopic * basic_count;
    irradiance_par += contribution.irradiance_par * basic_count;
    ppfd += contribution.ppfd * basic_count;
  }
  irradiance_photopic = std::max(irradiance_photopic, 0.0f);
  irradiance_par = std::max(irradiance_par, 0.0f);
  ppfd = std::max(ppfd, 0.0f) * 1e3f;
  const float illuminance = irradiance_photopic * LUMENS_PER_WATT;

  ESP_LOGV(TAG, "Photopic irradiance %.4f, PAR %.4f, PPFD %.4f, illuminance %.2f lx", irradiance_photopic,
           irradiance_par, ppfd, illuminance);
  if (this->illuminance_sensor_ != nullptr) {
    this->illuminance_sensor_->publish_state(illuminance);
  }
  if (this->irradiance_photopic_sensor_ != nullptr) {
    this->irradiance_photopic_sensor_->publish_state(irradiance_photopic);
  }
  if (this->irradiance_par_sensor_ != nullptr) {
    this->irradiance_par_sensor_->publish_state(irradiance_par);
  }
  if (this->ppfd_sensor_ != nullptr) {
    this->ppfd_sensor_->publish_state(ppfd);
  }
}

void AS734XComponent::publish_color_() {
  float tri_x = 0.0f;
  float tri_y = 0.0f;
  float tri_z = 0.0f;

  const ChannelTristimulus *table = pgm(this->chip_->tristimulus);
  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    const float basic_count = this->basic_counts_[band];
    const ChannelTristimulus tristimulus = pgm(table[band]);

    tri_x += tristimulus.x * basic_count;
    tri_y += tristimulus.y * basic_count;
    tri_z += tristimulus.z * basic_count;
  }

  const float color_temperature = tristimulus_to_cct(tri_x, tri_y, tri_z);
  ESP_LOGV(TAG, "XYZ %.4f, %.4f, %.4f -> CCT %.0f K", tri_x, tri_y, tri_z, color_temperature);
  if (this->color_temperature_sensor_ != nullptr) {
    this->color_temperature_sensor_->publish_state(color_temperature);
  }

#ifdef USE_AS734X_RGB
  if (this->rgb_hex_sensor_ == nullptr) {
    return;
  }
  // Convert to chromaticity first, so the colour describes the hue rather than the brightness.
  // Absolute tristimulus values clamp to white under anything but dim light.
  char rgb_hex[7] = "000000";  // no usable chromaticity reads as black rather than an empty string
  const float sum = tri_x + tri_y + tri_z;
  if (sum > MIN_TRISTIMULUS_SUM) {
    uint8_t rgb[3];
    tristimulus_to_rgb(tri_x / sum, tri_y / sum, tri_z / sum, rgb[0], rgb[1], rgb[2]);
    format_hex_to(rgb_hex, rgb, sizeof(rgb));
  }
  this->rgb_hex_sensor_->publish_state(rgb_hex);
#endif
}

void AS734XComponent::publish_measurement_() {
  this->calculate_basic_counts_();
  for (uint8_t band = 0; band < NUM_BANDS; band++) {
    if (this->sensors_[band] != nullptr) {
      this->sensors_[band]->publish_state(this->raw_counts_[band]);
    }
  }
  this->publish_basic_counts_();
  this->publish_light_metrics_();
  this->publish_color_();
  if (this->saturation_level_sensor_ != nullptr) {
    const uint32_t max_adc =
        std::min<uint32_t>((this->reading_atime_ + 1UL) * (this->reading_astep_ + 1UL), MAX_ADC_COUNT);
    const float level = max_adc == 0 ? 0.0f : 100.0f * this->peak_raw_count_ / max_adc;
    ESP_LOGV(TAG, "Highest ADC count %u of %u (%.1f%%)", this->peak_raw_count_, static_cast<unsigned>(max_adc), level);
    this->saturation_level_sensor_->publish_state(level);
  }
}

void AS734XComponent::abort_measurement_(const LogString *reason) {
  ESP_LOGW(TAG, "%s", LOG_STR_ARG(reason));
  this->write_byte(REG_ENABLE, ENABLE_PON);
  this->status_set_warning(reason);
  this->state_ = State::STATE_IDLE;
  this->disable_loop();
}

}  // namespace esphome::as734x
