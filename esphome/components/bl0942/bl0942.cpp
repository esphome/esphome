#include "bl0942.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace bl0942 {

static const char *const TAG = "bl0942";

static const uint8_t BL0942_READ_COMMAND = 0x58;
static const uint8_t BL0942_FULL_PACKET = 0xAA;
static const uint8_t BL0942_PACKET_HEADER = 0x55;

static const uint8_t BL0942_WRITE_COMMAND = 0xA8;
static const uint8_t BL0942_REG_I_FAST_RMS_CTRL = 0x10;
static const uint8_t BL0942_REG_MODE = 0x18;
static const uint8_t BL0942_REG_SOFT_RESET = 0x19;
static const uint8_t BL0942_REG_USR_WRPROT = 0x1A;
static const uint8_t BL0942_REG_TPS_CTRL = 0x1B;

// TODO: Confirm insialisation works as intended
const uint8_t BL0942_INIT[5][6] = {
    // Reset to default
    {BL0942_WRITE_COMMAND, BL0942_REG_SOFT_RESET, 0x5A, 0x5A, 0x5A, 0x38},
    // Enable User Operation Write
    {BL0942_WRITE_COMMAND, BL0942_REG_USR_WRPROT, 0x55, 0x00, 0x00, 0xF0},
    // 0x0100 = CF_UNABLE energy pulse, AC_FREQ_SEL 50Hz, RMS_UPDATE_SEL 800mS
    {BL0942_WRITE_COMMAND, BL0942_REG_MODE, 0x00, 0x10, 0x00, 0x37},
    // 0x47FF = Over-current and leakage alarm on, Automatic temperature measurement, Interval 100mS
    {BL0942_WRITE_COMMAND, BL0942_REG_TPS_CTRL, 0xFF, 0x47, 0x00, 0xFE},
    // 0x181C = Half cycle, Fast RMS threshold 6172
    {BL0942_WRITE_COMMAND, BL0942_REG_I_FAST_RMS_CTRL, 0x1C, 0x18, 0x00, 0x1B}};

void BL0942::loop() {
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

bool BL0942::validate_checksum(DataPacket *data) {
  uint8_t checksum = BL0942_READ_COMMAND;
  // Whole package but checksum
  uint8_t *raw = (uint8_t *) data;
  for (uint32_t i = 0; i < sizeof(*data) - 1; i++) {
    checksum += raw[i];
  }
  checksum ^= 0xFF;
  if (checksum != data->checksum) {
    ESP_LOGW(TAG, "BL0942 invalid checksum! 0x%02X != 0x%02X", checksum, data->checksum);
  }
  return checksum == data->checksum;
}

void BL0942::update() {
  this->flush();
  this->write_byte(BL0942_READ_COMMAND);
  this->write_byte(BL0942_FULL_PACKET);
}

void BL0942::setup() {
  for (auto *i : BL0942_INIT) {
    this->write_array(i, 6);
    delay(1);
  }
  this->flush();
}

void BL0942::received_package_(DataPacket *data) {
  // Bad header
  if (data->frame_header != BL0942_PACKET_HEADER) {
    ESP_LOGI(TAG, "Invalid data. Header mismatch: %d", data->frame_header);
    return;
  }

  float v_rms = (uint24_t) data->v_rms / voltage_reference_;
  float i_rms = (uint24_t) data->i_rms / current_reference_;
  float watt = (int24_t) data->watt / power_reference_;
  uint32_t cf_cnt = (uint24_t) data->cf_cnt;
  float total_energy_consumption = cf_cnt / energy_reference_;
  float frequency = 1000000.0f / data->frequency;

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
  if (frequency_sensor_ != nullptr) {
    frequency_sensor_->publish_state(frequency);
  }

  ESP_LOGV(TAG, "BL0942: U %fV, I %fA, P %fW, Cnt %" PRId32 ", ∫P %fkWh, frequency %fHz, status 0x%08X", v_rms, i_rms,
           watt, cf_cnt, total_energy_consumption, frequency, data->status);
}

void BL0942::dump_config() {  // NOLINT(readability-function-cognitive-complexity)
  ESP_LOGCONFIG(TAG, "BL0942:");
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Energy", this->energy_sensor_);
  LOG_SENSOR("", "frequency", this->frequency_sensor_);
}

}  // namespace bl0942
}  // namespace esphome
