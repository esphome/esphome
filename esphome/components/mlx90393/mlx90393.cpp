#include "mlx90393.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90393 {

static const char *TAG = "mlx90393";
static const uint8_t MLX90393_COMMAND_SB = 0x10;
static const uint8_t MLX90393_COMMAND_SW = 0x20;
static const uint8_t MLX90393_COMMAND_SM = 0x30;
static const uint8_t MLX90393_COMMAND_RM = 0x40;
static const uint8_t MLX90393_COMMAND_RR = 0x50;
static const uint8_t MLX90393_COMMAND_WR = 0x60;
static const uint8_t MLX90393_COMMAND_EX = 0x80;
static const uint8_t MLX90393_COMMAND_HR = 0xD0;
static const uint8_t MLX90393_COMMAND_HS = 0xE0;
static const uint8_t MLX90393_COMMAND_RT = 0xF0;
static const uint8_t MLX90393_REGISTER_CONFIG_1 = 0x00;
static const uint8_t MLX90393_REGISTER_CONFIG_2 = 0x01;
static const uint8_t MLX90393_REGISTER_CONFIG_3 = 0x02;
static const uint8_t MLX90393_REGISTER_SENS_TC = 0x03;
static const uint8_t MLX90393_REGISTER_OFFSET_X = 0x04;
static const uint8_t MLX90393_REGISTER_OFFSET_Y = 0x05;
static const uint8_t MLX90393_REGISTER_OFFSET_Z = 0x06;
static const uint8_t MLX90393_REGISTER_WOXY_THRESHOLD = 0x07;
static const uint8_t MLX90393_REGISTER_WOZ_THRESHOLD = 0x08;
static const uint8_t MLX90393_REGISTER_WOT_THRESHOLD = 0x08;
static const uint8_t MLX90393_REGISTER_TREF = 0x24;  // From the app notes
static const uint8_t MLX90393_REQUEST_ZYXT = 0b1111;
static const uint8_t MLX90393_STATUS_ERROR = 0b00010000;
static const uint8_t MLX90393_STATUS_RESET_PERFORMED = 0b00000100;
static const uint8_t MLX90393_CONFIG_3_DIG_FILT = 7;  // 128x

// µT/bit
static const float SENS_XY[] = {0.751, 0.601, 0.451, 0.376, 0.300, 0.250, 0.200, 0.150};
static const float SENS_Z[] = {1.210, 0.968, 0.726, 0.605, 0.484, 0.403, 0.323, 0.242};

static const char *gain_to_str(MLX90393Gain gain) {
  switch (gain) {
    case MLX9039_GAIN_1:
      return "1x";
    case MLX9039_GAIN_1P25:
      return "1.25x";
    case MLX9039_GAIN_1P67:
      return "1.67x";
    case MLX9039_GAIN_2:
      return "2x";
    case MLX9039_GAIN_2P5:
      return "2.5x";
    case MLX9039_GAIN_3:
      return "3x";
    case MLX9039_GAIN_3P75:
      return "3.75x";
    case MLX9039_GAIN_5:
      return "5x";
    default:
      return "UNKNOWN";
  }
}

static const char *oversampling_to_str(MLX90393Oversampling oversampling) {
  switch (oversampling) {
    case MLX9039_OVERSAMPLING_NONE:
      return "OFF";
    case MLX9039_OVERSAMPLING_2X:
      return "2x";
    case MLX9039_OVERSAMPLING_4X:
      return "4x";
    case MLX9039_OVERSAMPLING_8X:
      return "8x";
    default:
      return "UNKNOWN";
  }
}

static const char *range_to_str(MLX90393Range range) {
  switch (range) {
    case MLX9039_RANGE_16BIT:
      return "16bit";
    case MLX9039_RANGE_17BIT:
      return "17bit";
    case MLX9039_RANGE_18BIT:
      return "18bit";
    case MLX9039_RANGE_19BIT:
      return "19bit";
    default:
      return "UNKNOWN";
  }
}

static uint32_t measurement_time(MLX90393Oversampling oversampling) {
  // T_stby + T_active + T_conv_end
  uint32_t time_usec = 264 + 432 + 120;
  // 3 * T_convm
  time_usec += 3 * (67 + 64 * (1 << uint32_t(oversampling)) * (2 + (1 << MLX90393_CONFIG_3_DIG_FILT)));
  // T_convt
  time_usec += 67 + 192 * (1 << uint32_t(oversampling));
  // Oscillator has +/- 10% accuracy (thermal and trimming)
  time_usec *= 110;
  time_usec /= 100;
  return time_usec / 1000 + 1;
}

static float convert_raw(uint16_t raw, MLX90393Range range, float sensitivity) {
  // The raw 19 bit value has 0G = 0x20000, so it needs special handling when bit 17 is visible
  switch (range) {
    default:
      return (int16_t(raw) * (1 << int32_t(range))) * sensitivity;
    case MLX9039_RANGE_18BIT:
      return ((int32_t(raw) - 0x8000) * (1 << int32_t(range))) * sensitivity;
    case MLX9039_RANGE_19BIT:
      return ((int32_t(raw) - 0x4000) * (1 << int32_t(range))) * sensitivity;
  }
}

void MLX90393Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90393...");
  uint8_t status;

  if (!this->read_byte(MLX90393_COMMAND_EX, &status)) {
    ESP_LOGE(TAG, "Error issuing exit mode command!");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  if ((status & MLX90393_STATUS_ERROR)) {
    ESP_LOGE(TAG, "Exit mode command failed!");
    this->error_code_ = RESET_FAILED;
    this->mark_failed();
    return;
  }

  if (!this->read_byte(MLX90393_COMMAND_RT, &status, 2)) {
    ESP_LOGE(TAG, "Error issuing reset command!");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  if ((status & MLX90393_STATUS_ERROR) || !(status & MLX90393_STATUS_RESET_PERFORMED)) {
    ESP_LOGE(TAG, "Device not reset!");
    this->error_code_ = RESET_FAILED;
    this->mark_failed();
    return;
  }

  optional<uint16_t> config1;
  if (!(config1 = this->read_register_(MLX90393_REGISTER_CONFIG_1))) {
    ESP_LOGE(TAG, "Error reading configuration register!");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  *config1 &= 0xFE00;  // Preserve ANA_RESERVED_LOW
  *config1 |= 0x0C;    // Recommended HALLCONF
  *config1 |= uint16_t(this->gain_) << 4;
  if (!this->write_register_(MLX90393_REGISTER_CONFIG_1, *config1)) {
    ESP_LOGE(TAG, "Error writing configuration register!");
    this->error_code_ = CONFIGURE_FAILED;
    this->mark_failed();
    return;
  }

  // COMM_MODE(I2C)
  uint16_t config2 = 0x6000;
  if (!this->write_register_(MLX90393_REGISTER_CONFIG_2, config2)) {
    ESP_LOGE(TAG, "Error writing configuration register!");
    this->error_code_ = CONFIGURE_FAILED;
    this->mark_failed();
    return;
  }

  uint16_t config3 = MLX90393_CONFIG_3_DIG_FILT << 2;
  config3 |= uint16_t(this->oversampling_);        // Magnetic
  config3 |= uint16_t(this->oversampling_) << 11;  // Temperature
  config3 |= uint16_t(this->range_) << 5;          // X
  config3 |= uint16_t(this->range_) << 7;          // Y
  config3 |= uint16_t(this->range_) << 9;          // Z
  if (!this->write_register_(MLX90393_REGISTER_CONFIG_3, config3)) {
    ESP_LOGE(TAG, "Error writing configuration register!");
    this->error_code_ = CONFIGURE_FAILED;
    this->mark_failed();
    return;
  }

  optional<uint16_t> tref;
  if (!(tref = this->read_register_(MLX90393_REGISTER_TREF))) {
    ESP_LOGE(TAG, "Error reading T_REF!");
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  this->tref_ = *tref;
}
void MLX90393Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90393:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with MLX90393 failed!");
      break;
    case RESET_FAILED:
      ESP_LOGE(TAG, "MLX90393 failed to reset properly!");
      break;
    case CONFIGURE_FAILED:
      ESP_LOGE(TAG, "Configuration of MLX90393 failed!");
      break;
    case NONE:
    default:
      break;
  }
  ESP_LOGCONFIG(TAG, "  Gain: %s", gain_to_str(this->gain_));
  ESP_LOGCONFIG(TAG, "  Range: %s", range_to_str(this->range_));
  ESP_LOGCONFIG(TAG, "  Oversampling: %s", oversampling_to_str(this->oversampling_));
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}
float MLX90393Component::get_setup_priority() const { return setup_priority::DATA; }
void MLX90393Component::update() {
  uint8_t status;

  if (!this->read_byte(MLX90393_COMMAND_SM | MLX90393_REQUEST_ZYXT, &status)) {
    ESP_LOGW(TAG, "Error issuing start measurement command.");
    this->status_set_warning();
    return;
  }
  ESP_LOGV(TAG, "Start measurement status = %02X", status);
  if (status & MLX90393_STATUS_ERROR) {
    ESP_LOGW(TAG, "Start measurement command failed.");
    this->status_set_warning();
    return;
  }

  this->set_timeout("data", measurement_time(this->oversampling_), [this] {
    uint8_t data[9];

    data[0] = MLX90393_COMMAND_RM | MLX90393_REQUEST_ZYXT;
    if (!this->write_bytes_raw(data, 1)) {
      ESP_LOGW(TAG, "Error sending read measurement command.");
      this->status_set_warning();
      return;
    }
    if (!this->read_bytes_raw(data, 9)) {
      ESP_LOGW(TAG, "Unable to read measurement data.");
      this->status_set_warning();
      return;
    }
    ESP_LOGV(TAG, "Read measurement status = %02X", data[0]);
    if (data[0] & MLX90393_STATUS_ERROR) {
      ESP_LOGW(TAG, "Error reading measurement, data is probably not ready.");
      this->status_set_warning();
      return;
    }

    this->status_clear_warning();

    const float t = 35.0f + (((data[1] << 8) | data[2]) - this->tref_) / 45.2f;
    const float x = convert_raw((data[3] << 8) | data[4], this->range_, SENS_XY[this->gain_]);
    const float y = convert_raw((data[5] << 8) | data[6], this->range_, SENS_XY[this->gain_]);
    const float z = convert_raw((data[7] << 8) | data[8], this->range_, SENS_Z[this->gain_]);

    ESP_LOGD(TAG, "Got x=%0.1fµT y=%0.1fµT z=%0.1fµT temperature=%.1f°C", x, y, z, t);

    if (this->x_sensor_ != nullptr)
      this->x_sensor_->publish_state(x);
    if (this->y_sensor_ != nullptr)
      this->y_sensor_->publish_state(y);
    if (this->z_sensor_ != nullptr)
      this->z_sensor_->publish_state(z);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(t);
  });
}
bool MLX90393Component::write_register_(uint8_t a_register, uint16_t value) {
  uint8_t data[4];
  data[0] = MLX90393_COMMAND_WR;
  data[1] = (value & 0xFF00) >> 8;
  data[2] = (value & 0xFF);
  data[3] = (a_register & 0x3F) << 2;

  ESP_LOGV(TAG, "Write 0x%02X = 0x%04X", a_register, value);
  if (!this->write_bytes_raw(data, 4)) {
    ESP_LOGV(TAG, "  Write command failed");
    return false;
  }
  if (!this->read_bytes_raw(data, 1)) {
    ESP_LOGV(TAG, "  Read status failed");
    return false;
  }
  ESP_LOGV(TAG, "  Write status = 0x%02X", data[0]);

  return (data[0] & MLX90393_STATUS_ERROR) == 0;
}
optional<uint16_t> MLX90393Component::read_register_(uint8_t a_register) {
  uint8_t data[3];
  data[0] = MLX90393_COMMAND_RR;
  data[1] = (a_register & 0x3F) << 2;

  ESP_LOGV(TAG, "Read 0x%02X", a_register);
  if (!this->write_bytes_raw(data, 2)) {
    ESP_LOGV(TAG, "  Read command failed");
    return {};
  }
  if (!this->read_bytes_raw(data, 3)) {
    ESP_LOGV(TAG, "  Read result failed");
    return {};
  }
  ESP_LOGV(TAG, "  Read status = 0x%02X", data[0]);

  if (data[0] & MLX90393_STATUS_ERROR)
    return {};
  uint16_t value = (data[1] << 8) | data[2];
  ESP_LOGV(TAG, "  Value = 0x%04X", value);
  return {value};
}

}  // namespace mlx90393
}  // namespace esphome
