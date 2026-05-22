#include "ufm01.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace esphome {
namespace ufm01 {

static const char *const TAG = "ufm_01";

static const float UNKNOWN = std::numeric_limits<float>::quiet_NaN();
static const float L_PER_M3 = 1000.0;
static const float M3_PER_L = 1.0 / L_PER_M3;

static const std::vector<uint8_t> ACTIVE_MODE = std::vector<uint8_t>{0xFE, 0xFE, 0x11, 0x5C, 0x00, 0x5C, 0x16};
static const std::vector<uint8_t> CLEAR_ACCUMULATED_FLOW =
    std::vector<uint8_t>{0xFE, 0xFE, 0x11, 0x5A, 0xFD, 0x57, 0x16};
static const std::vector<uint8_t> RESET_DEVICE = std::vector<uint8_t>{0xFE, 0xFE, 0x11, 0x5D, 0xFD, 0x5A, 0x16};

static float to_float(uint8_t data) { return (data >> 4) * 10 + (data & 0x0F); }

static bool log_problem(const char *log, uint8_t expected, uint8_t real) {
  ESP_LOGW(TAG, "%s - expected 0x%02X, but was 0x%02X", log, expected, real);
  return true;
}

static bool validate_data(uint8_t data[32]) {
  uint8_t sum = 0;
  for (int i = 0; i < 30; ++i)
    sum += data[i];
  return (data[0] == 0x3C || !log_problem("bit 0 exception", 0x3C, data[0])) &&
         (data[1] == 0x32 || !log_problem("bit 1 exception", 0x32, data[1])) &&
         (data[7] == 0x01 || !log_problem("bit 7 exception", 0x01, data[7])) &&
         (data[15] == 0x0B || !log_problem("bit 15 exception", 0x0B, data[15])) &&
         (data[21] == 0x0C || !log_problem("bit 21 exception", 0x0C, data[21])) &&
         (data[24] == 0x0D || !log_problem("bit 24 exception", 0x0D, data[24])) &&
         (data[30] == (sum & 0xFF) || !log_problem("checksum (bit 30) exception", sum, data[30])) &&
         (data[31] == 0x16 || !log_problem("bit 31 exception", 0x16, data[31]));
}

static float read_volume(uint8_t data[32]) {
  return (data[8] == 0x1A ? L_PER_M3 : 1.0) *
         (to_float(data[14]) * 10000000.0 + to_float(data[13]) * 100000.0 + to_float(data[12]) * 1000.0 +
          to_float(data[11]) * 10.0 + to_float(data[10]) * 0.1 + to_float(data[9]) * 0.001);
}

static float read_flow(uint8_t data[32]) {
  return (data[20] == 0x80 ? -1.0 : 1.0) *
         (to_float(data[19]) * 10000.0 + to_float(data[18]) * 100.0 + to_float(data[17]) + to_float(data[16]) * 0.01) *
         M3_PER_L;
}

static void log_hex(uint8_t data[32]) {
  std::string res;
  char buf[8];
  for (size_t i = 0; i < 32; i++) {
    if (i > 0) {
      res += " ";
    }
    snprintf(buf, sizeof(buf), "%02u:0x%02X", static_cast<unsigned int>(i), data[i]);
    res += buf;
  }
  ESP_LOGD(TAG, "%s", res.c_str());
}

static float read_temperature(uint8_t data[32]) {
  if (  // happens sometimes before getting real reading
      data[27] == 0X00 && (data[26] == 0x00 || data[26] == 0x70) && data[25] == 0X00) {
    return UNKNOWN;
  } else {
    return to_float(data[27]) * 100.0 + to_float(data[26]) + to_float(data[25]) * 0.01;
  }
}

static bool read_ufp_chip_error(const uint8_t data[32]) { return data[29] & 0x20; }

static bool read_flow_direction_wrong(const uint8_t data[32]) { return data[29] & 0x08; }

static bool read_empty_tube(const uint8_t data[32]) { return data[28] & 0x20; }

static bool read_flow_rate_out_of_range(const uint8_t data[32]) { return data[29] & 0x04; }

bool UFM01Component::send_command_(const std::vector<uint8_t> &command) {
  this->write_array(command);
  return 0xE5 == this->read();
}

bool UFM01Component::reset_device_() { return this->send_command_(RESET_DEVICE); }

bool UFM01Component::clear_accumulated_flow_() { return this->send_command_(CLEAR_ACCUMULATED_FLOW); }

bool UFM01Component::set_active_mode_() { return this->send_command_(ACTIVE_MODE); }

float UFM01Component::get_setup_priority() const { return setup_priority::IO; }

void UFM01Component::setup() {
  ESP_LOGI(TAG, "Setting up UFM-01...");
  this->set_active_mode_();
}

void UFM01Component::on_data_(uint8_t data[32]) {
  if (this->ufp_chip_error_binary_sensor_ != nullptr)
    this->ufp_chip_error_binary_sensor_->publish_state(read_ufp_chip_error(data));
  if (this->flow_direction_wrong_binary_sensor_ != nullptr)
    this->flow_direction_wrong_binary_sensor_->publish_state(read_flow_direction_wrong(data));
  bool empty_tube = read_empty_tube(data);
  if (this->empty_tube_binary_sensor_ != nullptr)
    this->empty_tube_binary_sensor_->publish_state(empty_tube);
  if (this->flow_rate_out_of_range_binary_sensor_ != nullptr)
    this->flow_rate_out_of_range_binary_sensor_->publish_state(read_flow_rate_out_of_range(data));

  if (this->volume_sensor_ != nullptr)
    this->volume_sensor_->publish_state(read_volume(data));

  if (empty_tube) {
    if (this->flow_sensor_ != nullptr)
      this->flow_sensor_->publish_state(UNKNOWN);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(UNKNOWN);
  } else {
    if (this->flow_sensor_ != nullptr)
      this->flow_sensor_->publish_state(read_flow(data));
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(read_temperature(data));
  }
}

void UFM01Component::loop() {
  if (this->read_index_ == 32 && validate_data(this->data_)) {
    this->on_data_(this->data_);
    this->read_index_ = 0;
  }
  if (this->read_index_ == 32) {
    log_hex(this->data_);
    ESP_LOGE(TAG, "unable to read data");
    for (int i = 2; i < 31 && this->read_index_ == 32; ++i) {
      if ((this->data_[i] == 0x3C) && (this->data_[i + 1] == 0x32)) {
        for (int j = i; j < 32; ++j)
          this->data_[j - i] = this->data_[j];
        this->read_index_ = 32 - i;
      }
    }
    if (this->read_index_ == 32)
      this->read_index_ = 0;
  }
  if (this->read_index_ > 32) {  // NOT EXPECTED
    ESP_LOGE(TAG, "Read more than 32 bytes into array");
    this->read_index_ = 0;
  }
  if (this->read_index_ < 32 && this->available()) {
    if (this->read_byte(&this->data_[this->read_index_])) {
      // ESP_LOGD(TAG, "%2d GOT %02X", this->read_index, this->data[this->read_index]);
      if ((this->read_index_ == 0 && this->data_[0] != 0x3C) || (this->read_index_ == 1 && this->data_[1] != 0x32)) {
        ESP_LOGW(TAG, "not start of data at %d (is 0x%02X)", this->read_index_, this->data_[this->read_index_]);
        this->read_index_ = 0;
      } else {
        this->read_index_++;
      }
    } else {
      ESP_LOGW(TAG, "unable to read byte");
      this->read_index_ = 0;
    }
  }
}

}  // namespace ufm01
}  // namespace esphome
