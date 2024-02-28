#include "as7343.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include "as7343_calibration.h"

namespace esphome {
namespace as7343 {

static const char *const TAG = "as7343";

static constexpr float CONST_H = 6.6260695e-34f;
static constexpr float CONST_C = 299792458;

static constexpr uint8_t NUM_USEFUL_CHANNELS = 13;
static constexpr uint8_t CHANNEL_IDX[NUM_USEFUL_CHANNELS] = {
    AS7343_CHANNEL_405_F1, AS7343_CHANNEL_425_F2,  AS7343_CHANNEL_450_FZ,  AS7343_CHANNEL_475_F3, AS7343_CHANNEL_515_F4,
    AS7343_CHANNEL_555_FY, AS7343_CHANNEL_550_F5,  AS7343_CHANNEL_600_FXL, AS7343_CHANNEL_640_F6, AS7343_CHANNEL_690_F7,
    AS7343_CHANNEL_745_F8, AS7343_CHANNEL_855_NIR, AS7343_CHANNEL_CLEAR};
static constexpr float CHANNEL_SENS[NUM_USEFUL_CHANNELS] = {0.19402, 0.26647, 0.35741, 0.41753, 0.52235,
                                                            0.59633, 0.56242, 0.65645, 0.68882, 0.79980,
                                                            0.70423, 0.40366, 0.38516};
static constexpr float CHANNEL_NM[NUM_USEFUL_CHANNELS] = {405, 425, 450, 475, 515, 555, 550,
                                                          600, 640, 690, 745, 855, 718};
static constexpr float CHANNEL_NM_WIDTH[NUM_USEFUL_CHANNELS] = {30, 22, 55, 30, 40, 100, 35, 80, 50, 55, 60, 54, 0};

static constexpr float CHANNEL_IRRAD_PER_BASIC_COUNT[NUM_USEFUL_CHANNELS] = {
    767.5101757, 2512.765376, 2034.308898, 5730.41039,  1404.780643, 1177.586336, 2803.31385,
    923.8726968, 1322.666667, 811.8520699, 5106.962963, 417.0131368, 4416.832833};

// E = h*c/lambda
static constexpr float PHOTON_ENERGIES[NUM_USEFUL_CHANNELS] = {
    4.9048E-19f,  4.67399E-19f, 4.41432E-19f, 4.18199E-19f, 3.85718E-19f, 3.57918E-19f, 3.61172E-19f,
    3.31074E-19f, 3.10382E-19f, 2.87891E-19f, 2.66637E-19f, 2.32333E-19f, 2.76664E-19f};

// constexpr std::array<float, NUM_USEFUL_CHANNELS> fill_photon_energy() {
//     std::array<float, NUM_USEFUL_CHANNELS> v{0};
//     for(int i = 0; i < NUM_USEFUL_CHANNELS; ++i) {
//         v[i] =  CHANNEL_NM[i]> 0 ? CONST_H * CONST_C / (CHANNEL_NM[i] * 1e9) : 0;
//     }
//     return v;
// }

//  constexpr std::array<float, NUM_USEFUL_CHANNELS> v = fill_photon_energy();

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
}

float AS7343Component::get_setup_priority() const { return setup_priority::DATA; }

void AS7343Component::update() {
  this->read_channels(this->channel_readings_);

  AS7343Gain gain = get_gain();
  uint8_t atime = get_atime();
  uint16_t astep = get_astep();

  float tint_ms = (1 + atime) * (1 + astep) * 2.78 / 1000;
  float gain_x = get_gain_multiplier(gain);

  ESP_LOGD(TAG, "  Gain : %.1fX", gain_x);
  ESP_LOGD(TAG, "  ATIME: %u", atime);
  ESP_LOGD(TAG, "  ASTEP: %u", astep);
  ESP_LOGD(TAG, "  TINT : %.2f", tint_ms);

  ESP_LOGD(TAG, "nm: %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, %.0f, ", CHANNEL_NM[0],
           CHANNEL_NM[1], CHANNEL_NM[2], CHANNEL_NM[3], CHANNEL_NM[4], CHANNEL_NM[5], CHANNEL_NM[6], CHANNEL_NM[7],
           CHANNEL_NM[8], CHANNEL_NM[9], CHANNEL_NM[10], CHANNEL_NM[11], CHANNEL_NM[12]);
  ESP_LOGD(TAG, "counts: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, ", this->channel_readings_[CHANNEL_IDX[0]],
           this->channel_readings_[CHANNEL_IDX[1]], this->channel_readings_[CHANNEL_IDX[2]],
           this->channel_readings_[CHANNEL_IDX[3]], this->channel_readings_[CHANNEL_IDX[4]],
           this->channel_readings_[CHANNEL_IDX[5]], this->channel_readings_[CHANNEL_IDX[6]],
           this->channel_readings_[CHANNEL_IDX[7]], this->channel_readings_[CHANNEL_IDX[8]],
           this->channel_readings_[CHANNEL_IDX[9]], this->channel_readings_[CHANNEL_IDX[10]],
           this->channel_readings_[CHANNEL_IDX[11]], this->channel_readings_[CHANNEL_IDX[12]]);

  float irradiance = this->calculate_par_v2(tint_ms, gain_x);

  if (this->illuminance_ != nullptr) {
    this->illuminance_->publish_state(irradiance / 0.0079);
  }

  if (this->irradiance_ != nullptr) {
    this->irradiance_->publish_state(irradiance);
  }

  if (this->f1_ != nullptr) {
    this->f1_->publish_state(this->channel_readings_[AS7343_CHANNEL_405_F1]);
  }
  if (this->f2_ != nullptr) {
    this->f2_->publish_state(this->channel_readings_[AS7343_CHANNEL_425_F2]);
  }
  if (this->fz_ != nullptr) {
    this->fz_->publish_state(this->channel_readings_[AS7343_CHANNEL_450_FZ]);
  }
  if (this->f3_ != nullptr) {
    this->f3_->publish_state(this->channel_readings_[AS7343_CHANNEL_475_F3]);
  }
  if (this->f4_ != nullptr) {
    this->f4_->publish_state(this->channel_readings_[AS7343_CHANNEL_515_F4]);
  }
  if (this->fy_ != nullptr) {
    this->fy_->publish_state(this->channel_readings_[AS7343_CHANNEL_555_FY]);
  }
  if (this->f5_ != nullptr) {
    this->f5_->publish_state(this->channel_readings_[AS7343_CHANNEL_550_F5]);
  }
  if (this->fxl_ != nullptr) {
    this->fxl_->publish_state(this->channel_readings_[AS7343_CHANNEL_600_FXL]);
  }
  if (this->f6_ != nullptr) {
    this->f6_->publish_state(this->channel_readings_[AS7343_CHANNEL_640_F6]);
  }
  if (this->f7_ != nullptr) {
    this->f7_->publish_state(this->channel_readings_[AS7343_CHANNEL_690_F7]);
  }
  if (this->f8_ != nullptr) {
    this->f8_->publish_state(this->channel_readings_[AS7343_CHANNEL_745_F8]);
  }
  if (this->nir_ != nullptr) {
    this->nir_->publish_state(this->channel_readings_[AS7343_CHANNEL_855_NIR]);
  }
  if (this->clear_ != nullptr) {
    float clear = (this->channel_readings_[AS7343_CHANNEL_CLEAR] + this->channel_readings_[AS7343_CHANNEL_CLEAR_0] +
                   this->channel_readings_[AS7343_CHANNEL_CLEAR_1]) /
                  3;
    this->clear_->publish_state(clear);
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
void AS7343Component::set_gain(esphome::optional<unsigned int> g) {
  if (g.has_value()) {
    this->set_gain(g.value());
  }
}

bool AS7343Component::setup_gain(AS7343Gain gain) { return this->write_byte((uint8_t) AS7343Registers::CFG1, gain); }

bool AS7343Component::setup_atime(uint8_t atime) { return this->write_byte((uint8_t) AS7343Registers::ATIME, atime); }

bool AS7343Component::setup_astep(uint16_t astep) {
  return this->write_byte_16((uint8_t) AS7343Registers::ASTEP_LSB, swap_bytes(astep));
}

float AS7343Component::calculate_par_v1() {
  float par = 0;
  for (uint8_t i = 0; i < NUM_USEFUL_CHANNELS; i++) {
    par += this->channel_readings_[CHANNEL_IDX[i]] * CHANNEL_SENS[i];
  }
  return par;
}

float AS7343Component::calculate_par_v2(float tint_ms, float gain_x) {
  float irradiance = 0;

  for (uint8_t i = 0; i < NUM_USEFUL_CHANNELS; i++) {
    float basic_count = this->channel_readings_[CHANNEL_IDX[i]] / (gain_x * tint_ms);
    irradiance += basic_count * CHANNEL_IRRAD_PER_BASIC_COUNT[i];
  }
  ESP_LOGD(TAG, "  Irradiance: %f W/m^2", irradiance / 1000);
  ESP_LOGD(TAG, "  Lux solar: %f lx", irradiance / 1000 / 0.0079);
  return irradiance;
}

bool AS7343Component::read_channels(uint16_t *data) {
  this->enable_spectral_measurement(true);
  this->wait_for_data();

  return this->read_bytes_16((uint8_t) AS7343Registers::DATA_O, this->channel_readings_, AS7343_NUM_CHANNELS);
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

bool AS7343Component::is_data_ready() { return this->read_register_bit((uint8_t) AS7343Registers::STATUS2, 6); }

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

}  // namespace as7343
}  // namespace esphome
