#include "bl0940.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace bl0940 {

static const char *const TAG = "bl0940";

static const uint8_t BL0940_READ_COMMAND = 0x50;  // 0x58 according to documentation
static const uint8_t BL0940_FULL_PACKET = 0xAA;
static const uint8_t BL0940_PACKET_HEADER = 0x55;  // 0x58 according to documentation

static const uint8_t BL0940_WRITE_COMMAND = 0xA0;  // 0xA8 according to documentation
static const uint8_t BL0940_REG_I_FAST_RMS_CTRL = 0x10;
static const uint8_t BL0940_REG_MODE = 0x18;
static const uint8_t BL0940_REG_SOFT_RESET = 0x19;
static const uint8_t BL0940_REG_USR_WRPROT = 0x1A;
static const uint8_t BL0940_REG_TPS_CTRL = 0x1B;

const uint8_t BL0940_INIT[5][6] = {
    // Reset to default
    {BL0940_WRITE_COMMAND, BL0940_REG_SOFT_RESET, 0x5A, 0x5A, 0x5A, 0x38},
    // Enable User Operation Write
    {BL0940_WRITE_COMMAND, BL0940_REG_USR_WRPROT, 0x55, 0x00, 0x00, 0xF0},
    // 0x0100 = CF_UNABLE energy pulse, AC_FREQ_SEL 50Hz, RMS_UPDATE_SEL 800mS
    {BL0940_WRITE_COMMAND, BL0940_REG_MODE, 0x00, 0x10, 0x00, 0x37},
    // 0x47FF = Over-current and leakage alarm on, Automatic temperature measurement, Interval 100mS
    {BL0940_WRITE_COMMAND, BL0940_REG_TPS_CTRL, 0xFF, 0x47, 0x00, 0xFE},
    // 0x181C = Half cycle, Fast RMS threshold 6172
    {BL0940_WRITE_COMMAND, BL0940_REG_I_FAST_RMS_CTRL, 0x1C, 0x18, 0x00, 0x1B}};

void BL0940::loop() {
  DataPacket buffer;
  if (!this->available()) {
    return;
  }
  if (read_array((uint8_t *) &buffer, sizeof(buffer))) {
    if (validate_checksum(&buffer)) {
      received_package_(&buffer);
    }
  } else {
    ESP_LOGW(TAG, "Junk on wire. Throwing away partial message");
    while (read() >= 0)
      ;
  }
}

bool BL0940::validate_checksum(const DataPacket *data) {
  uint8_t checksum = BL0940_READ_COMMAND;
  // Whole package but checksum
  for (uint32_t i = 0; i < sizeof(data->raw) - 1; i++) {
    checksum += data->raw[i];
  }
  checksum ^= 0xFF;
  if (checksum != data->checksum) {
    ESP_LOGW(TAG, "BL0940 invalid checksum! 0x%02X != 0x%02X", checksum, data->checksum);
  }
  return checksum == data->checksum;
}

void BL0940::update() {
  this->flush();
  this->write_byte(BL0940_READ_COMMAND);
  this->write_byte(BL0940_FULL_PACKET);
}

void BL0940::setup() {
  for (auto *i : BL0940_INIT) {
    this->write_array(i, 6);
    delay(1);
  }
  this->flush();
}

float BL0940::update_temp_(sensor::Sensor *sensor, ube16_t temperature) const {
  auto tb = (float) (temperature.h << 8 | temperature.l);
  float converted_temp = ((float) 170 / 448) * (tb / 2 - 32) - 45;
  if (sensor != nullptr) {
    if (sensor->has_state() && std::abs(converted_temp - sensor->get_state()) > max_temperature_diff_) {
      ESP_LOGD(TAG, "Invalid temperature change. Sensor: '%s', Old temperature: %f, New temperature: %f",
               sensor->get_name().c_str(), sensor->get_state(), converted_temp);
      return 0.0f;
    }
    sensor->publish_state(converted_temp);
  }
  return converted_temp;
}

void BL0940::received_package_(const DataPacket *data) const {
  // Bad header
  if (data->frame_header != BL0940_PACKET_HEADER) {
    ESP_LOGI(TAG, "Invalid data. Header mismatch: %d", data->frame_header);
    return;
  }

  float v_rms = (float) to_uint32_t(data->v_rms) / voltage_reference_;
  float i_rms = (float) to_uint32_t(data->i_rms) / current_reference_;
  float watt = (float) to_int32_t(data->watt) / power_reference_;
  uint32_t cf_cnt = to_uint32_t(data->cf_cnt);
  float total_energy_consumption = (float) cf_cnt / energy_reference_;

  float tps1 = update_temp_(internal_temperature_sensor_, data->tps1);
  float tps2 = update_temp_(external_temperature_sensor_, data->tps2);

  if (voltage_sensor_ != nullptr) {
    voltage_sensor_->publish_state(v_rms);
  }
  if (current_sensor_ != nullptr) {
    current_sensor_->publish_state(i_rms);
  }
  if (power_sensor_ != nullptr) {
    power_sensor_->publish_state(watt);
  }
  if (energy_sensor_ != nullptr) {
    energy_sensor_->publish_state(total_energy_consumption);
  }

  ESP_LOGV(TAG, "BL0940: U %fV, I %fA, P %fW, Cnt %" PRId32 ", ∫P %fkWh, T1 %f°C, T2 %f°C", v_rms, i_rms, watt, cf_cnt,
           total_energy_consumption, tps1, tps2);
}

void BL0940::dump_config() {  // NOLINT(readability-function-cognitive-complexity)
  ESP_LOGCONFIG(TAG, "BL0940:");
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Energy", this->energy_sensor_);
  LOG_SENSOR("", "Internal temperature", this->internal_temperature_sensor_);
  LOG_SENSOR("", "External temperature", this->external_temperature_sensor_);
}

uint32_t BL0940::to_uint32_t(ube24_t input) { return input.h << 16 | input.m << 8 | input.l; }

int32_t BL0940::to_int32_t(sbe24_t input) { return input.h << 16 | input.m << 8 | input.l; }

}  // namespace bl0940
}  // namespace esphome
