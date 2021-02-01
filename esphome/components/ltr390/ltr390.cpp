#include "ltr390.h"
#include "esphome/core/log.h"
#include <bitset>

namespace esphome {
namespace ltr390 {

static const char *TAG = "ltr390";

static const float GAINVALUES[5] = {1.0, 3.0, 6.0, 9.0, 18.0};
static const float RESOLUTIONVALUE[6] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
static const uint32_t MODEADDRESSES[2] = {0x0D, 0x10};

bool LTR390Component::enabled_() {
  std::bitset<8> crtl_value(this->ctrl_reg_->get());
  return (bool) crtl_value[LTR390_CTRL_EN];
}

void LTR390Component::enable_(bool en) {
  std::bitset<8> crtl_value(this->ctrl_reg_->get());
  crtl_value[LTR390_CTRL_EN] = en;
  *this->ctrl_reg_ = crtl_value.to_ulong();
}

bool LTR390Component::reset_() {
  std::bitset<8> crtl_value(this->ctrl_reg_->get());

  crtl_value[LTR390_CTRL_RST] = true;
  *this->ctrl_reg_ = crtl_value.to_ulong();

  delay(10);

  // Read after reset
  crtl_value = std::bitset<8>(this->ctrl_reg_->get());

  return !(bool) crtl_value.to_ulong();
}

void LTR390Component::set_mode_(LTR390MODE mode) {
  std::bitset<8> crtl_value(this->ctrl_reg_->get());
  crtl_value[LTR390_CTRL_MODE] = mode;
  *this->ctrl_reg_ = crtl_value.to_ulong();
}

LTR390MODE LTR390Component::get_mode_() {
  std::bitset<8> crtl_value(this->ctrl_reg_->get());
  return (LTR390MODE)(int) crtl_value[LTR390_CTRL_MODE];
}

void LTR390Component::set_gain_(LTR390GAIN gain) { *this->gain_reg_ = gain; }

LTR390GAIN LTR390Component::get_gain_() {
  std::bitset<8> gain_value(this->gain_reg_->get());
  return (LTR390GAIN) gain_value.to_ulong();
}

void LTR390Component::set_resolution_(LTR390RESOLUTION res) {
  std::bitset<8> res_value(this->res_reg_->get());

  std::bitset<3> new_res_value(res);

  for (int i = 0; i < 3; i++) {
    res_value[4 + i] = new_res_value[i];
  }

  *this->res_reg_ = res_value.to_ulong();
}

LTR390RESOLUTION LTR390Component::get_resolution_() {
  std::bitset<8> res_value(this->res_reg_->get());

  std::bitset<3> output_value(0);
  for (int i = 0; i < 3; i++) {
    output_value[i] = res_value[4 + i];
  }

  return (LTR390RESOLUTION) output_value.to_ulong();
}

bool LTR390Component::new_data_available_() {
  std::bitset<8> status_value(this->status_reg_->get());
  return (bool) status_value[3];
}

uint32_t little_endian_bytes_to_int(const uint8_t *buffer, uint8_t num_bytes) {
  uint32_t value = 0;

  for (int i = 0; i < num_bytes; i++) {
    value <<= 8;
    value |= buffer[num_bytes - i - 1];
  }

  return value;
}

uint32_t LTR390Component::read_sensor_data_(LTR390MODE mode) {
  const uint8_t num_bytes = 3;
  uint8_t buffer[num_bytes];

  while (!this->new_data_available_()) {
    ESP_LOGD(TAG, "WAITING FOR DATA");
    delay(2);
  }

  this->read_bytes(MODEADDRESSES[mode], buffer, num_bytes);

  return little_endian_bytes_to_int(buffer, num_bytes);
}

void LTR390Component::read_als_() {
  uint32_t als = this->read_sensor_data_(LTR390_MODE_ALS);

  if (this->light_sensor_ != nullptr) {
    float lux = (0.6 * als) / (GAINVALUES[this->gain_] * RESOLUTIONVALUE[this->res_]) * this->wfac_;
    this->light_sensor_->publish_state(lux);
  }

  if (this->als_sensor_ != nullptr) {
    this->als_sensor_->publish_state(als);
  }
}

void LTR390Component::read_uvs_() {
  uint32_t uv = this->read_sensor_data_(LTR390_MODE_UVS);

  if (this->uvi_sensor_ != nullptr) {
    this->uvi_sensor_->publish_state(uv / LTR390_SENSITIVITY * this->wfac_);
  }

  if (this->uv_sensor_ != nullptr) {
    this->uv_sensor_->publish_state(uv);
  }
}

void LTR390Component::read_mode_(int mode_index) {
  // Set mode
  this->set_mode_(std::get<0>(this->mode_funcs_->at(mode_index)));

  // After the sensor integration time do the following
  this->set_timeout_(RESOLUTIONVALUE[this->res_] * 100, [this, mode_index]() {
    // Read from the sensor
    std::get<1>(this->mode_funcs_->at(mode_index))();

    // If there are more modes to read then begin the next
    // otherwise stop
    if (mode_index + 1 < this->mode_funcs_->size()) {
      this->read_mode_(mode_index + 1);
    } else {
      this->reading_ = false;
    }
  });
}

void LTR390Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ltr390...");

  this->ctrl_reg_ = new i2c::I2CRegister(this, LTR390_MAIN_CTRL);
  this->status_reg_ = new i2c::I2CRegister(this, LTR390_MAIN_STATUS);
  this->gain_reg_ = new i2c::I2CRegister(this, LTR390_GAIN);
  this->res_reg_ = new i2c::I2CRegister(this, LTR390_MEAS_RATE);

  this->reset_();

  this->enable_(true);
  ESP_LOGD(TAG, "%s", this->enabled() ? "ENABLED" : "DISABLED");
  if (!this->enabled_()) {
    this->mark_failed();
    return;
  }

  // Set gain
  this->set_gain_(this->gain_);

  // Set resolution
  this->set_resolution_(this->res_);

  // Set sensor read state
  this->reading_ = false;

  // Create a list of modes and corresponding read functions
  this->mode_funcs_ = new std::vector<std::tuple<LTR390MODE, std::function<void(void)> > >();

  // If we need the light sensor then add to the list
  if (this->light_sensor_ != nullptr || this->als_sensor_ != nullptr) {
    this->mode_funcs_->push_back(std::make_tuple(LTR390_MODE_ALS, std::bind(&LTR390Component::read_als_, this)));
  }

  // If we need the UV sensor then add to the list
  if (this->uvi_sensor_ != nullptr || this->uv_sensor_ != nullptr) {
    this->mode_funcs_->push_back(std::make_tuple(LTR390_MODE_UVS, std::bind(&LTR390Component::read_uvs_, this)));
  }
}

void LTR390Component::dump_config() { LOG_I2C_DEVICE(this); }

void LTR390Component::update() {
  if (!this->reading_) {
    this->reading_ = true;
    this->read_mode_(0);
  }
}

}  // namespace ltr390
}  // namespace esphome
