#include "as7343.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include "as7343_calibration.h"

namespace esphome {
namespace as7343 {

static const char *const TAG = "as7343";

// pimoroni?
static constexpr float CHANNEL_COMPENSATION_GAIN[AS7343_NUM_CHANNELS] = {1.84, 6.03, 4.88, 13.74, 3.37, 2.82, 6.72,
                                                                         2.22, 3.17, 1.95, 12.25, 1.00, 1};

static constexpr float CHANNEL_SENS[AS7343_NUM_CHANNELS] = {0.19402, 0.26647, 0.35741, 0.41753, 0.52235,
                                                            0.59633, 0.56242, 0.65645, 0.68882, 0.79980,
                                                            0.70423, 0.40366, 0.38516};

//
// 1. AS7343 Datasheet-based
//
// 1.1 Names
// static constexpr char *CHANNEL_NAMES[AS7343_NUM_CHANNELS] = {"F1",  "F2", "FZ", "F3", "F4",  "FY",   "F5",
//                                                              "FXL", "F6", "F7", "F8", "NIR", "Clear"};

static const std::array<const char *, AS7343_NUM_CHANNELS> CHANNEL_NAMES = {
    "F1", "F2", "FZ", "F3", "F4", "FY", "F5", "FXL", "F6", "F7", "F8", "NIR", "Clear"};
// 1.2 Each channel central wavelength in nm
static const std::array<float, AS7343_NUM_CHANNELS> CHANNEL_NM = {405, 425, 450, 475, 515, 555, 550,
                                                                  600, 640, 690, 745, 855, 718};
// 1.3 Each channel width in nm
static const std::array<float, AS7343_NUM_CHANNELS> CHANNEL_NM_WIDTH = {30, 22, 55, 30, 40, 100, 35,
                                                                        80, 50, 55, 60, 54, 0};

// 1.4 Gain correctoin for each channel
static constexpr float CHANNEL_BASIC_CORRECTIONS[AS7343_NUM_CHANNELS] = {
    1.055464349, 1.043509797, 1.029576268, 1.0175052,   1.00441899,  0.987356499, 0.957597044,
    0.995863485, 1.014628964, 0.996500814, 0.933072749, 1.052236338, 0.999570232};

// 1.5 Irradiation in mW/m² per basic count ?
static constexpr float CHANNEL_IRRAD_MW_PER_BASIC_COUNT[AS7343_NUM_CHANNELS] = {
    767.5101757, 2512.765376, 2034.308898, 5730.41039,  1404.780643, 1177.586336, 2803.31385,
    923.8726968, 1322.666667, 811.8520699, 5106.962963, 417.0131368, 78.70319635};

// 1.6 band energy contribution precalculated based on band widths and central wavelengths (photon energy)
// value = band energy / sum of all bands energies, except for VIS
static constexpr float CHANNEL_ENERGY_CONTRIBUTION[AS7343_NUM_CHANNELS] = {0.069385773,
                                                                           0.04848841,
                                                                           0.114486525,
                                                                           0.059160501,
                                                                           0.072754014,
                                                                           0.168776203,
                                                                           0.059608686,
                                                                           0.124894391,
                                                                           0.073180307,
                                                                           0.074665125,
                                                                           0.075439565,
                                                                           0.059160501,
                                                                           0};

//
// 2. Scientific data
//
// 2.1 Photon energy for selected bands
// E = h*c/lambda
static constexpr float PHOTON_ENERGIES[AS7343_NUM_CHANNELS] = {
    4.9048E-19f,  4.67399E-19f, 4.41432E-19f, 4.18199E-19f, 3.85718E-19f, 3.57918E-19f, 3.61172E-19f,
    3.31074E-19f, 3.10382E-19f, 2.87891E-19f, 2.66637E-19f, 2.32333E-19f, 2.76664E-19f};

// 2.2 CIE 1923 Photopic Luminosity Function for selected bands
static constexpr float CHANNEL_PHOTOPIC_LUMINOSITY[AS7343_NUM_CHANNELS] = {0.0006400000000,
                                                                           0.0073000000000,
                                                                           0.0380000000000,
                                                                           0.1126000000000,
                                                                           0.6082000000000,
                                                                           0.9949501000000,
                                                                           1.0000000000000,
                                                                           0.6310000000000,
                                                                           0.1750000000000,
                                                                           0.0082100000000,
                                                                           0.0001719000000,
                                                                           0,
                                                                           0};

// static constexpr float CHANNEL_CONTRIB[NUM_USEFUL_CHANNELS] = {
//     0.0603622, 0.0442656, 0.110664, 0.0603622, 0.0804829, 0.201207, 0.0704225, 0.160966, 0.100604, 0.110664, 0};

void AS7343Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS7343...");
  LOG_I2C_DEVICE(this);

  // Verify device ID
  this->set_bank_for_reg_(AS7343Registers::ID);
  uint8_t id = this->reg((uint8_t) AS7343Registers::ID).get();
  ESP_LOGCONFIG(TAG, "  Read ID: 0x%X", id);
  if (id != AS7343_CHIP_ID) {
    this->mark_failed();
    ESP_LOGE(TAG, "  Invalid chip ID: 0x%X", id);
    return;
  }

  this->set_bank_for_reg_(AS7343Registers::ENABLE);
  // Power on (enter IDLE state)
  if (!this->enable_power(true)) {
    ESP_LOGE(TAG, "  Power on failed!");
    this->mark_failed();
    return;
  }

  // Set configuration
  AS7343RegCfg20 cfg20;
  cfg20.raw = this->reg((uint8_t) AS7343Registers::CFG20).get();
  cfg20.auto_smux = 0b11;
  this->reg((uint8_t) AS7343Registers::CFG20) = cfg20.raw;

  this->direct_config_3_chain_();

  this->setup_atime(this->atime_);
  this->setup_astep(this->astep_);
  this->setup_gain(this->gain_);

  // enable led false ?
}

void AS7343Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS7343:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with AS7343 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Gain: %.1f", get_gain_multiplier(get_gain()));
  ESP_LOGCONFIG(TAG, "  ATIME: %u", get_atime());
  ESP_LOGCONFIG(TAG, "  ASTEP: %u", get_astep());
  ESP_LOGCONFIG(TAG, "  Glass attenuation factor: %f", this->glass_attenuation_factor_);

  LOG_SENSOR("  ", "F1", this->f1_);
  LOG_SENSOR("  ", "F2", this->f2_);
  LOG_SENSOR("  ", "FZ", this->fz_);
  LOG_SENSOR("  ", "F3", this->f3_);
  LOG_SENSOR("  ", "F4", this->f4_);
  LOG_SENSOR("  ", "FY", this->fy_);
  LOG_SENSOR("  ", "F5", this->f5_);
  LOG_SENSOR("  ", "FXL", this->fxl_);
  LOG_SENSOR("  ", "F6", this->f6_);
  LOG_SENSOR("  ", "F7", this->f7_);
  LOG_SENSOR("  ", "F8", this->f8_);
  LOG_SENSOR("  ", "NIR", this->nir_);
  LOG_SENSOR("  ", "Clear", this->clear_);
  LOG_SENSOR("  ", "Clear", this->clear_);
  LOG_SENSOR("  ", "Clear", this->clear_);
}

float AS7343Component::get_setup_priority() const { return setup_priority::DATA; }

void log13_f(const char *TAG, const char *str, const std::array<float, 13> &arr) {
  ESP_LOGD(TAG, "%s, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, ", str, arr[0],
           arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

void log13_d(const char *TAG, const char *str, const std::array<uint16_t, 13> &arr) {
  ESP_LOGD(TAG, "%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ", str, arr[0], arr[1], arr[2], arr[3], arr[4],
           arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

void log13_s(const char *TAG, const char *str, const std::array<const char *, 13> &arr) {
  ESP_LOGD(TAG, "%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ", str, arr[0], arr[1], arr[2], arr[3], arr[4],
           arr[5], arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12]);
}

void AS7343Component::update() {
  this->enable_spectral_measurement(true);
  this->read_all_channels();
  this->enable_spectral_measurement(false);
  this->calculate_basic_counts();

  log13_s(TAG, "Channel", CHANNEL_NAMES);
  log13_f(TAG, "Nm", CHANNEL_NM);
  log13_d(TAG, "Counts", this->readings_.raw_counts);

  uint16_t max_adc = this->get_maximum_spectral_adc_();
  uint16_t highest_adc = this->get_highest_value(this->readings_.raw_counts);

  if (highest_adc >= max_adc) {
    ESP_LOGW(TAG, "Max ADC: %u, Highest reading: %u", max_adc, highest_adc);
  } else {
    ESP_LOGD(TAG, "Max ADC: %u, Highest reading: %u", max_adc, highest_adc);
  }

  ESP_LOGD(TAG, "  ,Gain , %.1f,X", this->readings_.gain_x);
  ESP_LOGD(TAG, "  ,ATIME, %u,", this->readings_.atime);
  ESP_LOGD(TAG, "  ,ASTEP, %u,", this->readings_.astep);
  ESP_LOGD(TAG, "  ,TINT , %.2f,", this->readings_.t_int);

  // ESP_LOGD(TAG, ",nm, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, ", CHANNEL_NM[0],
  //          CHANNEL_NM[1], CHANNEL_NM[2], CHANNEL_NM[3], CHANNEL_NM[4], CHANNEL_NM[5], CHANNEL_NM[6], CHANNEL_NM[7],
  //          CHANNEL_NM[8], CHANNEL_NM[9], CHANNEL_NM[10], CHANNEL_NM[11], CHANNEL_NM[12]);
  // ESP_LOGD(TAG, ",counts, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ",
  //          this->channel_readings_[CHANNEL_IDX[0]], this->channel_readings_[CHANNEL_IDX[1]],
  //          this->channel_readings_[CHANNEL_IDX[2]], this->channel_readings_[CHANNEL_IDX[3]],
  //          this->channel_readings_[CHANNEL_IDX[4]], this->channel_readings_[CHANNEL_IDX[5]],
  //          this->channel_readings_[CHANNEL_IDX[6]], this->channel_readings_[CHANNEL_IDX[7]],
  //          this->channel_readings_[CHANNEL_IDX[8]], this->channel_readings_[CHANNEL_IDX[9]],
  //          this->channel_readings_[CHANNEL_IDX[10]], this->channel_readings_[CHANNEL_IDX[11]],
  //          this->channel_readings_[CHANNEL_IDX[12]]);

  float irradiance;
  float irradiance_photopic;
  float lux;
  float ppfd;

  this->calculate_irradiance(irradiance, irradiance_photopic, lux);
  this->calculate_ppfd(ppfd);

  ESP_LOGD(TAG, "BEFORE GLASS ATTENUATION");
  ESP_LOGD(TAG, "  ,Irradiance          , %f, W/m²", irradiance);
  ESP_LOGD(TAG, "  ,Irradiance(photopic), %f, W/m²", irradiance_photopic);
  ESP_LOGD(TAG, "  ,Lux(solar coeff)    , %f, lx", lux);
  ESP_LOGD(TAG, "  ,PPFD                , %.2f, µmol/s⋅m²", ppfd);

  irradiance *= this->glass_attenuation_factor_;
  irradiance_photopic *= this->glass_attenuation_factor_;
  lux *= this->glass_attenuation_factor_;
  ppfd *= this->glass_attenuation_factor_;

  ESP_LOGD(TAG, "AFTER GLASS ATTENUATION");
  ESP_LOGD(TAG, "  ,Irradiance          , %f, W/m²", irradiance);
  ESP_LOGD(TAG, "  ,Irradiance(photopic), %f, W/m²", irradiance_photopic);
  ESP_LOGD(TAG, "  ,Lux(solar coeff)    , %f, lx", lux);
  ESP_LOGD(TAG, "  ,PPFD                , %.2f, µmol/s⋅m²", ppfd);

  if (this->illuminance_ != nullptr) {
    this->illuminance_->publish_state(lux);
  }

  if (this->irradiance_ != nullptr) {
    this->irradiance_->publish_state(irradiance);
  }

  if (this->ppfd_ != nullptr) {
    this->ppfd_->publish_state(ppfd);
  }

  uint8_t i = 0;
  float max_val = 0;
  float normalized_readings[AS7343_NUM_CHANNELS];
  for (i = 0; i < AS7343_NUM_CHANNELS; i++) {
    normalized_readings[i] = (float) this->readings_.basic_counts[i] / CHANNEL_SENS[i];
    if (max_val < normalized_readings[i]) {
      max_val = normalized_readings[i];
    }
  }

  for (i = 0; i < AS7343_NUM_CHANNELS; i++) {
    normalized_readings[i] = normalized_readings[i] * 100 / max_val;
  }

  if (this->f1_ != nullptr) {
    this->f1_->publish_state(normalized_readings[0]);
  }
  if (this->f2_ != nullptr) {
    this->f2_->publish_state(normalized_readings[1]);
  }
  if (this->fz_ != nullptr) {
    this->fz_->publish_state(normalized_readings[2]);
  }
  if (this->f3_ != nullptr) {
    this->f3_->publish_state(normalized_readings[3]);
  }
  if (this->f4_ != nullptr) {
    this->f4_->publish_state(normalized_readings[4]);
  }
  if (this->fy_ != nullptr) {
    this->fy_->publish_state(normalized_readings[5]);
  }
  if (this->f5_ != nullptr) {
    this->f5_->publish_state(normalized_readings[6]);
  }
  if (this->fxl_ != nullptr) {
    this->fxl_->publish_state(normalized_readings[7]);
  }
  if (this->f6_ != nullptr) {
    this->f6_->publish_state(normalized_readings[8]);
  }
  if (this->f7_ != nullptr) {
    this->f7_->publish_state(normalized_readings[9]);
  }
  if (this->f8_ != nullptr) {
    this->f8_->publish_state(normalized_readings[10]);
  }
  if (this->nir_ != nullptr) {
    this->nir_->publish_state(normalized_readings[11]);
  }
  // if (this->clear_ != nullptr) {
  //   float clear = (this->channel_readings_[AS7343_CHANNEL_CLEAR] + this->channel_readings_[AS7343_CHANNEL_CLEAR_0] +
  //                  this->channel_readings_[AS7343_CHANNEL_CLEAR_1]) /
  //                 3;
  //   this->clear_->publish_state(clear);
  // }
  if (this->saturated_ != nullptr) {
    this->saturated_->publish_state(this->readings_saturated_);
  }
}

AS7343Gain AS7343Component::get_gain() {
  uint8_t data;
  this->read_byte((uint8_t) AS7343Registers::CFG1, &data);
  return (AS7343Gain) data;
}

uint8_t AS7343Component::get_atime() {
  uint8_t data;
  this->read_byte((uint8_t) AS7343Registers::ATIME, &data);
  return data;
}

uint16_t AS7343Component::get_astep() {
  uint16_t data;
  this->read_byte_16((uint8_t) AS7343Registers::ASTEP_LSB, &data);
  return this->swap_bytes(data);
}

float AS7343Component::get_gain_multiplier(AS7343Gain gain) {
  float gainx = ((uint16_t) 1 << (uint8_t) gain);
  return gainx / 2;
}

bool AS7343Component::setup_gain(AS7343Gain gain) {
  ESP_LOGD(TAG, "Setup gain %u", (uint8_t) gain);
  return this->write_byte((uint8_t) AS7343Registers::CFG1, gain);
}

bool AS7343Component::setup_atime(uint8_t atime) {
  ESP_LOGD(TAG, "Setup atime %u", atime);
  return this->write_byte((uint8_t) AS7343Registers::ATIME, atime);
}

bool AS7343Component::setup_astep(uint16_t astep) {
  ESP_LOGD(TAG, "Setup astep %u", astep);
  return this->write_byte_16((uint8_t) AS7343Registers::ASTEP_LSB, swap_bytes(astep));
}

bool AS7343Component::change_gain(AS7343Gain gain) {
  this->gain_ = gain;
  this->enable_spectral_measurement(false);
  return this->write_byte((uint8_t) AS7343Registers::CFG1, gain);
}

void AS7343Component::calculate_basic_counts() {
  float divisor = this->readings_.gain_x * this->readings_.t_int;
  for (size_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
    // 1. raw -> basic
    float basic_count = this->readings_.raw_counts[i] / divisor;
    // 2. gain correction
    basic_count *= AS7343_GAIN_CORRECTION[(uint8_t) this->readings_.gain][i];

    this->readings_.basic_counts[i] = basic_count;
  }
}

void AS7343Component::calculate_ppfd(float &ppfd) {
  /*
  Given the spectral irradiance Iλ, defined as the radiant flux
  per unit wavelength and unit area, the photon flux density
  in the PAR wavelength range can be calculated using the
  following expression:

  φ = ∫ Iλ(λ) * dλ / (hc/λ)                             (1)

  where h is Planck’s constant, c is the speed of light, and λ is the wavelength.

  The PPFD is simply the photon density flux measured in units of
  micro-moles of photons per square meter per second (µmol m−2 s−1),
  where a mole of photons is defined as NA = 6.022×10^23 (Avogadro’s number) photons.

  PPFD (or Qpar) can be calculated as
  Qpar = φ / (NA * 10^6)                                (2)

  Combining (1) and (2) gives the following expression for PPFD:

  Qpar = ∫ Iλ(λ) * dλ / (hc/λ * NA * 10^6)              (3)

  where 1/hc/NA*10^6 is a constant equal to 0.836e-2.

  */
  ppfd = 0;

  // assume we integrate using rectangles - mid point is channel wavelength, width is channel width in nm
  for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
    if (CHANNEL_NM[i] < 400 || CHANNEL_NM[i] > 700) {
      continue;
    }

    // Iλ(λ)
    float irradiance_in_w_per_m2 = this->readings_.basic_counts[i] * CHANNEL_IRRAD_MW_PER_BASIC_COUNT[i] / 1000;
    float photon_flux = irradiance_in_w_per_m2 * CHANNEL_NM[i] * 0.836e-2;

    // assume channels cover whole range
    ppfd += photon_flux * CHANNEL_NM_WIDTH[i] / 1e9f;  // nm to meters
  }
}

void AS7343Component::calculate_irradiance(float &irradiance_in_w_per_m2, float &irradiance_in_w_per_m2_photopic,
                                           float &lux) {
  // Total irradiance in a whole wavelenght interval
  float irr_band;
  float photo_band;
  irradiance_in_w_per_m2 = 0;
  irradiance_in_w_per_m2_photopic = 0;
  lux = 0;

  // walk through all bands except for Clear (VIS)
  for (uint8_t i = 0; i < AS7343_NUM_CHANNELS - 1; i++) {
    irr_band = this->readings_.basic_counts[i] * CHANNEL_IRRAD_MW_PER_BASIC_COUNT[i] / 1000;
    irradiance_in_w_per_m2 += irr_band;

    // photo_band *= CHANNEL_ENERGY_CONTRIBUTION[i]; ?? ideas
    // let's make model assumption bands are adjacent and cover whole range
    photo_band = irr_band * CHANNEL_PHOTOPIC_LUMINOSITY[i];
    irradiance_in_w_per_m2_photopic += photo_band;
  }
  // sunlight equivalent
  // 1 W/m2 = 116 ± 3 lx solar
  // https://www.extrica.com/article/21667/pdf
  lux = irradiance_in_w_per_m2_photopic * 116;
}

bool AS7343Component::read_all_channels() {
  static constexpr uint8_t CHANNEL_MAP[AS7343_NUM_CHANNELS] = {
      AS7343_CHANNEL_405_F1, AS7343_CHANNEL_425_F2, AS7343_CHANNEL_450_FZ, AS7343_CHANNEL_475_F3,
      AS7343_CHANNEL_515_F4, AS7343_CHANNEL_555_FY, AS7343_CHANNEL_550_F5, AS7343_CHANNEL_600_FXL,
      AS7343_CHANNEL_640_F6, AS7343_CHANNEL_690_F7, AS7343_CHANNEL_745_F8, AS7343_CHANNEL_855_NIR,
      AS7343_CHANNEL_CLEAR};

  std::array<uint16_t, AS7343_NUM_CHANNELS_MAX> data;

  this->wait_for_data();

  AS7343RegStatus status{0};
  status.raw = this->reg((uint8_t) AS7343Registers::STATUS).get();
  ESP_LOGD(TAG, "Status 0x%02x, sint %d, fint %d, aint %d, asat %d", status.raw, status.sint, status.fint, status.aint,
           status.asat);
  this->reg((uint8_t) AS7343Registers::STATUS) = status.raw;

  AS7343RegAStatus astatus{0};
  astatus.raw = this->reg((uint8_t) AS7343Registers::ASTATUS).get();
  ESP_LOGD(TAG, "AStatus 0x%02x, again_status %d, asat_status %d", astatus.raw, astatus.again_status,
           astatus.asat_status);

  if (astatus.asat_status) {
    ESP_LOGW(TAG, "AS7343 affected by analog or digital saturation. Readings are not reliable.");
  }

  auto ret = this->read_bytes_16((uint8_t) AS7343Registers::DATA_O, data.data(), AS7343_NUM_CHANNELS_MAX);

  for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
    this->readings_.raw_counts[i] = data[CHANNEL_MAP[i]];
  }

  this->readings_.gain = astatus.again_status;
  this->readings_.gain_x = get_gain_multiplier(this->readings_.gain);
  this->readings_.atime = get_atime();
  this->readings_.astep = get_astep();
  this->readings_.t_int = (1 + this->readings_.atime) * (1 + this->readings_.astep) * 2.78 / 1000;  // us to ms

  this->readings_saturated_ = astatus.asat_status;

  return ret;
}

bool AS7343Component::wait_for_data(uint16_t timeout) {
  for (uint16_t time = 0; time < timeout; time++) {
    if (this->is_data_ready()) {
      return true;
    }

    delay(1);
  }

  return false;
}

bool AS7343Component::is_data_ready() {
  AS7343RegStatus2 status2{0};
  status2.raw = this->reg((uint8_t) AS7343Registers::STATUS2).get();
  ESP_LOGD(TAG, "Status2 0x%02x, avalid %d, asat_digital %d, asat_analog %d", status2.raw, status2.avalid,
           status2.asat_digital, status2.asat_analog);
  this->reg((uint8_t) AS7343Registers::STATUS2) = status2.raw;

  //  return this->read_register_bit((uint8_t) AS7343Registers::STATUS2, 6);
  return status2.avalid;
}

void AS7343Component::set_bank_for_reg_(AS7343Registers reg) {
  bool bank = (uint8_t) reg < 0x80;
  if (bank == this->bank_) {
    return;
  }
  this->write_register_bit((uint8_t) AS7343Registers::CFG0, bank, AS7343_CFG0_REG_BANK_BIT);
  this->bank_ = bank;
}

bool AS7343Component::enable_power(bool enable) {
  return this->write_register_bit((uint8_t) AS7343Registers::ENABLE, enable, AS7343_ENABLE_PON_BIT);
}

bool AS7343Component::enable_spectral_measurement(bool enable) {
  return this->write_register_bit((uint8_t) AS7343Registers::ENABLE, enable, AS7343_ENABLE_SP_EN_BIT);
}

bool AS7343Component::read_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  bool bit = (data & (1 << bit_position)) > 0;
  return bit;
}

bool AS7343Component::write_register_bit(uint8_t address, bool value, uint8_t bit_position) {
  if (value) {
    return this->set_register_bit(address, bit_position);
  }

  return this->clear_register_bit(address, bit_position);
}

bool AS7343Component::set_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data |= (1 << bit_position);
  return this->write_byte(address, data);
}

bool AS7343Component::clear_register_bit(uint8_t address, uint8_t bit_position) {
  uint8_t data;
  this->read_byte(address, &data);
  data &= ~(1 << bit_position);
  return this->write_byte(address, data);
}

uint16_t AS7343Component::swap_bytes(uint16_t data) { return (data >> 8) | (data << 8); }

void AS7343Component::setup_tint_(float TINT) {
  ESP_LOGD(TAG, "Setup tint %.2f", TINT);
  uint8_t ATIME = 0x00;
  uint16_t ASTEP = 0x0000;
  while (true) {
    ASTEP = ((TINT / (double) (ATIME + 1)) * 720.0 / 2.0);

    if (abs(((ATIME + 1) * (ASTEP + 1) * 2 / 720) - (uint16_t) TINT) <= 1) {
      break;
    } else {
      ATIME += 1;
    }
  }

  this->setup_atime(ATIME);
  this->setup_astep(ASTEP);

  // this->write_byte((uint8_t)AS7343_ATIME, ATIME);
  // this->write_byte((uint8_t)AS7343_ASTEP_LSB, (uint8_t) (ASTEP & 0xFF));
  // this->write_byte((uint8_t)AS7343_ASTEP_MSB, (uint8_t) (ASTEP >> 8));
}

float AS7343Component::get_tint_() {
  uint16_t ASTEP = this->get_astep();
  uint8_t ATIME = this->get_atime();

  double TINT = (ASTEP + 1) * (ATIME + 1) * (2.0 / 720.0);

  return TINT;
}

void AS7343Component::optimizer_(float max_TINT) {
  // uint8_t currentGain = 12;

  // uint16_t FSR = 65535;
  // float TINT = 182.0;
  // // AS7343_set_TINT(handle, TINT);
  // this->setup_tint_(TINT);

  // uint16_t max_count;
  // uint16_t min_count;

  // while (true) {
  //   max_count = 0;
  //   min_count = 0xffff;
  //   this->setup_gain((AS7343Gain) currentGain);

  //   uint16_t data[18];
  //   this->enable_spectral_measurement(true);
  //   this->read_18_channels(data);
  //   this->enable_spectral_measurement(false);

  //   for (uint8_t i = 0; i < 18; i++) {
  //     if (i == 5 || i == 11 || i == 17) {
  //       continue;
  //     }
  //     if (data[i] > max_count) {
  //       max_count = data[i];
  //     }
  //     if (data[i] < min_count) {
  //       min_count = data[i];
  //     }
  //   }

  //   if (max_count > 0xE665) {
  //     if (currentGain == 0) {
  //       // TODO: send optimizer failed due to saturation message
  //       break;
  //     }
  //     currentGain -= 1;
  //     continue;
  //   }

  //   else if (min_count == 0) {
  //     if (currentGain == 12) {
  //       // TODO: send optimizer failed due to saturation message
  //       break;
  //     }
  //     currentGain += 1;
  //     continue;
  //   }

  //   else {
  //     break;
  //   }
  // }

  // float counts_expected = (float) max_count;
  // float multiplier = 0.90;

  // while (true) {
  //   // set to loop once only, might change the algorithm in the future
  //   max_count = 0;
  //   float exp = (multiplier * (float) FSR - counts_expected);
  //   if (exp < 0) {
  //     break;
  //   }
  //   float temp_TINT = TINT + pow(2, log((multiplier * (float) FSR - counts_expected)) / log(2)) * (2.0 / 720.0);

  //   if (temp_TINT > max_TINT) {
  //     break;
  //   }

  //   this->setup_tint_(temp_TINT);

  //   std::array<uint16_t,AS7343_NUM_CHANNELS> data;
  //   this->enable_spectral_measurement(true);
  //   this->read_18_channels(data.data());
  //   this->enable_spectral_measurement(false);

  //   for (uint8_t i = 0; i < 18; i++) {
  //     if (i == 5 || i == 11 || i == 17) {
  //       continue;
  //     }
  //     if (data[i] > max_count) {
  //       max_count = data[i];
  //     }
  //   }

  //   if (max_count >= multiplier * 0xFFEE) {
  //     multiplier = multiplier - 0.05;
  //     continue;
  //   } else {
  //     TINT = temp_TINT;
  //   }
  //   break;
  // }
  // // this->set_gain(currentGain);
  // this->setup_gain((AS7343Gain) currentGain);
  // this->setup_tint_(TINT);
}

void AS7343Component::direct_config_3_chain_() {
  this->write_byte((uint8_t) AS7343Registers::CFG6, 0x0);
  this->write_byte((uint8_t) AS7343Registers::FD_CFG0, 0xa1);
  this->write_byte((uint8_t) AS7343Registers::CFG10, 0xf2);

  this->write_byte((uint8_t) AS7343Registers::CFG0, 0x10);
  this->write_byte((uint8_t) AS7343Registers::CFG1, 0x0c);
  this->write_byte((uint8_t) AS7343Registers::CFG8, 0xc8);
  this->write_byte((uint8_t) AS7343Registers::CFG20, 0x62);
  this->write_byte((uint8_t) AS7343Registers::AGC_GAIN_MAX, 0x99);
  this->write_byte((uint8_t) AS7343Registers::FD_TIME_1, 0x64);
  this->write_byte((uint8_t) AS7343Registers::FD_TIME_2, 0x21);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x04);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x65);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x02);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x05);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x01);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x30);
  this->write_byte((uint8_t) 0xe4, 0x46);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x60);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x20);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x04);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x50);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x03);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x01);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x05);
  this->write_byte((uint8_t) 0xe4, 0x56);

  this->write_byte((uint8_t) 0xe7, 0x05);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x60);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x30);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x40);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x10);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x20);
  this->write_byte((uint8_t) 0xe4, 0x66);

  this->write_byte((uint8_t) 0xe7, 0x00);
  this->write_byte((uint8_t) 0xe4, 0x66);

  //	this->write_byte((uint8_t)0x80, 0x11);
}

#define MIN_ASTEP 1
#define MAX_ASTEP 65534
#define MIN_ITIME_US 6
#define MAX_ITIME_US 46602667
#define CONVERSION_FACTOR_MS_TO_US 1000

#define INTEGRATION_TIME_STEP_US_FACTOR 2000
#define INTEGRATION_TIME_STEP_US_DIVIDER 720

/*! Use this macro for signed 64 Bit divisions */
#define DIV64_S64(s64dividend, s64divisor) (s64dividend / s64divisor)
/*! Use this macro for unsigned 64 Bit divisions */
#define DIV64_U64(u64dividend, u64divisor) (u64dividend / u64divisor)

bool AS7343Component::as7352_set_integration_time_us(uint32_t time_us) {
  bool result;
  int64_t time;
  uint8_t atime = 0;
  uint16_t astep = 0xFFFF / 2;
  int64_t astep_i64;

  if (MIN_ITIME_US > time_us || MAX_ITIME_US < time_us) {
    return false;
  }

  time = DIV64_S64((int64_t) time_us * INTEGRATION_TIME_STEP_US_DIVIDER, INTEGRATION_TIME_STEP_US_FACTOR);
  time = DIV64_S64(time, ((int64_t) astep + 1));
  time -= 1;

  if (0 > time) {
    atime = 0;
  } else if (255 < time) {
    atime = 255;
  } else {
    atime = (uint8_t) time;
  }

  astep_i64 = DIV64_S64((int64_t) time_us * INTEGRATION_TIME_STEP_US_DIVIDER * 10, INTEGRATION_TIME_STEP_US_FACTOR);
  astep_i64 = DIV64_S64(astep_i64, ((int64_t) atime + 1)) + 5;
  astep_i64 = DIV64_S64(astep_i64, 10);
  astep_i64 -= 1;

  if (MIN_ASTEP > astep_i64 || MAX_ASTEP < astep_i64) {
    return false;
  } else {
    astep = (uint16_t) astep_i64;
  }

  auto max_adc = this->get_maximum_spectral_adc_(atime, astep);
  ESP_LOGD(TAG, "for itime %u : atime %u, astep %u, max_adc: %u", time_us, atime, astep, max_adc);

  this->set_astep(astep);
  this->set_atime(atime);

  this->setup_atime(atime);
  this->setup_astep(astep);

  return result;
}

uint16_t AS7343Component::get_maximum_spectral_adc_() {
  //
  return this->get_maximum_spectral_adc_(this->atime_, this->astep_);
};

static constexpr uint16_t MAX_ADC_COUNT = 65535;

uint16_t AS7343Component::get_maximum_spectral_adc_(uint16_t atime, uint16_t astep) {
  uint32_t value = (atime + 1) * (astep + 1);
  if (value > MAX_ADC_COUNT) {
    value = MAX_ADC_COUNT;
  }
  return value;
}

// uint16_t AS7343Component::get_highest_value(std::array<uint16_t, AS7343_NUM_CHANNELS> &data) {
template<typename T, size_t N> T AS7343Component::get_highest_value(std::array<T, N> &data) {
  T max = 0;
  for (const auto &v : data) {
    if (v > max) {
      max = v;
    }
  }
  return max;
}

#define LOW_AUTO_GAIN_VALUE 3
#define AUTO_GAIN_DIVIDER 2
#define IS_SATURATION 1
#define SATURATION_LOW_PERCENT 80
#define SATURATION_HIGH_PERCENT 100
#define ADC_SATURATED_VALUE 65535

bool AS7343Component::spectral_post_process_(bool fire_at_will) {
  bool need_to_repeat = false;
  // uint16_t highest_value, maximum_adc;
  // bool is_saturation{false};
  // uint8_t current_gain, new_gain;

  // uint16_t max_adc = this->get_maximum_spectral_adc_();
  // uint16_t highest_adc = this->get_highest_value(this->channel_readings_);

  // current_gain = this->readings_gain_;
  // new_gain = current_gain;
  // this->get_optimized_gain_(max_adc, highest_adc, AS7343Gain::AS7343_GAIN_0_5X, AS7343Gain::AS7343_GAIN_128X,
  // new_gain,
  //                           is_saturation);
  // if (new_gain != current_gain) {
  //   if (fire_at_will) {
  //     // need to repeat the measurement
  //     this->set_gain((AS7343Gain) new_gain);
  //     this->setup_gain((AS7343Gain) new_gain);
  //   }
  //   need_to_repeat = true;
  // } else if (is_saturation) {
  //   // digital saturation
  //   // but can't change gain? try change time ?
  //   ESP_LOGW(TAG, "Spectral post process: OPTIMIZE saturation detected");
  // }
  // if (!is_saturation) {
  //   // no saturation
  //   for (uint8_t i = 0; i < AS7343_NUM_CHANNELS; i++) {
  //     // todo - update reading with gain factor first, then compare
  //     if (this->channel_readings_[i] >= max_adc) {  // check both values - before and after gain factor application
  //       this->channel_readings_[i] = ADC_SATURATED_VALUE;
  //       is_saturation = true;
  //     }
  //   }
  //   if (is_saturation) {
  //     ESP_LOGW(TAG, "Spectral post process: CHANNEL saturation detected");
  //   }
  // }
  /// what to do with saturation and !need_to_repeat ?
  // ESP_LOGW(TAG, "Spectral post process: gain %u, saturation %u, need to repeat %u", new_gain, is_saturation,
  //          need_to_repeat);
  return need_to_repeat;
}

static uint8_t find_highest_bit(uint32_t value) {
  uint8_t i = 0;
  uint8_t order = 0;

  for (i = 0; i < 32; i++) {
    if (value == 0) {
      break;
    } else if ((value >> i) & 1) {
      order = i;
    }
  }

  return order;
}

void AS7343Component::get_optimized_gain_(uint16_t maximum_adc, uint16_t highest_adc, uint8_t lower_gain_limit,
                                          uint8_t upper_gain_limit, uint8_t &out_gain, bool &out_saturation) {
  uint32_t gain_change;

  if (highest_adc == 0) {
    highest_adc = 1;
  }

  if (highest_adc >= maximum_adc) {
    /* saturation detected */
    if (out_gain > LOW_AUTO_GAIN_VALUE) {
      out_gain /= AUTO_GAIN_DIVIDER;
    } else {
      out_gain = lower_gain_limit;
    }
    out_saturation = true;
  } else {
    /* value too low, increase the gain */
    gain_change =
        (SATURATION_LOW_PERCENT * (uint32_t) maximum_adc) / (SATURATION_HIGH_PERCENT * (uint32_t) highest_adc);
    if (gain_change == 0 && out_gain != 0) {
      (out_gain)--;
    } else {
      gain_change = find_highest_bit(gain_change);
      if (((uint32_t) (out_gain) + gain_change) > upper_gain_limit) {
        out_gain = upper_gain_limit;
      } else {
        out_gain += (uint8_t) gain_change;
      }
    }
    out_saturation = false;
  }

  if (lower_gain_limit > out_gain) {
    out_gain = lower_gain_limit;
  }
}

}  // namespace as7343
}  // namespace esphome
