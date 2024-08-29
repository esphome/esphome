#include "bl0942.h"
#include "esphome/core/log.h"
#include <cinttypes>

// Datasheet: https://www.belling.com.cn/media/file_object/bel_product/BL0942/datasheet/BL0942_V1.06_en.pdf

namespace esphome {
namespace bl0942 {

static const char *const TAG = "bl0942";

static const uint8_t BL0942_READ_COMMAND = 0x58;
static const uint8_t BL0942_FULL_PACKET = 0xAA;
static const uint8_t BL0942_PACKET_HEADER = 0x55;

static const uint8_t BL0942_WRITE_COMMAND = 0xA8;

static const uint8_t BL0942_REG_I_RMSOS = 0x12;
static const uint8_t BL0942_REG_WA_CREEP = 0x14;
static const uint8_t BL0942_REG_I_FAST_RMS_TH = 0x15;
static const uint8_t BL0942_REG_I_FAST_RMS_CYC = 0x16;
static const uint8_t BL0942_REG_FREQ_CYC = 0x17;
static const uint8_t BL0942_REG_OT_FUNX = 0x18;
static const uint8_t BL0942_REG_MODE = 0x19;
static const uint8_t BL0942_REG_SOFT_RESET = 0x1C;
static const uint8_t BL0942_REG_USR_WRPROT = 0x1D;
static const uint8_t BL0942_REG_TPS_CTRL = 0x1B;

static const uint32_t BL0942_REG_MODE_RESV = 0x03;
static const uint32_t BL0942_REG_MODE_CF_EN = 0x04;
static const uint32_t BL0942_REG_MODE_RMS_UPDATE_SEL = 0x08;
static const uint32_t BL0942_REG_MODE_FAST_RMS_SEL = 0x10;
static const uint32_t BL0942_REG_MODE_AC_FREQ_SEL = 0x20;
static const uint32_t BL0942_REG_MODE_CF_CNT_CLR_SEL = 0x40;
static const uint32_t BL0942_REG_MODE_CF_CNT_ADD_SEL = 0x80;
static const uint32_t BL0942_REG_MODE_UART_RATE_19200 = 0x200;
static const uint32_t BL0942_REG_MODE_UART_RATE_38400 = 0x300;
static const uint32_t BL0942_REG_MODE_DEFAULT =
    BL0942_REG_MODE_RESV | BL0942_REG_MODE_CF_EN | BL0942_REG_MODE_CF_CNT_ADD_SEL;

static const uint32_t BL0942_REG_SOFT_RESET_MAGIC = 0x5a5a5a;
static const uint32_t BL0942_REG_USR_WRPROT_MAGIC = 0x55;

void BL0942::loop() {
  DataPacket buffer;
  if (!this->available()) {
    return;
  }
  if (this->read_array((uint8_t *) &buffer, sizeof(buffer))) {
    if (this->validate_checksum_(&buffer)) {
      this->received_package_(&buffer);
    }
  } else {
    ESP_LOGW(TAG, "Junk on wire. Throwing away partial message");
    while (read() >= 0)
      ;
  }
}

bool BL0942::validate_checksum_(DataPacket *data) {
  uint8_t checksum = BL0942_READ_COMMAND | this->address_;
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

void BL0942::write_reg_(uint8_t reg, uint32_t val) {
  uint8_t pkt[6];

  this->flush();
  pkt[0] = BL0942_WRITE_COMMAND | this->address_;
  pkt[1] = reg;
  pkt[2] = (val & 0xff);
  pkt[3] = (val >> 8) & 0xff;
  pkt[4] = (val >> 16) & 0xff;
  pkt[5] = (pkt[0] + pkt[1] + pkt[2] + pkt[3] + pkt[4]) ^ 0xff;
  this->write_array(pkt, 6);
  delay(1);
}

int BL0942::read_reg_(uint8_t reg) {
  union {
    uint8_t b[4];
    uint32_le_t le32;
  } resp;

  this->write_byte(BL0942_READ_COMMAND | this->address_);
  this->write_byte(reg);
  this->flush();
  if (this->read_array(resp.b, 4) &&
      resp.b[3] ==
          (uint8_t) ((BL0942_READ_COMMAND + this->address_ + reg + resp.b[0] + resp.b[1] + resp.b[2]) ^ 0xff)) {
    resp.b[3] = 0;
    return resp.le32;
  }
  return -1;
}

void BL0942::update() {
  this->write_byte(BL0942_READ_COMMAND | this->address_);
  this->write_byte(BL0942_FULL_PACKET);
}

void BL0942::setup() {
  this->write_reg_(BL0942_REG_USR_WRPROT, BL0942_REG_USR_WRPROT_MAGIC);
  this->write_reg_(BL0942_REG_SOFT_RESET, BL0942_REG_SOFT_RESET_MAGIC);

  uint32_t mode = BL0942_REG_MODE_DEFAULT;
  mode |= BL0942_REG_MODE_RMS_UPDATE_SEL; /* 800ms refresh time */
  if (this->line_freq_ == LINE_FREQUENCY_60HZ)
    mode |= BL0942_REG_MODE_AC_FREQ_SEL;
  this->write_reg_(BL0942_REG_MODE, mode);

  this->write_reg_(BL0942_REG_USR_WRPROT, 0);

  if (this->read_reg_(BL0942_REG_MODE) != mode)
    this->status_set_warning("BL0942 setup failed!");

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
  this->status_clear_warning();
  ESP_LOGV(TAG, "BL0942: U %fV, I %fA, P %fW, Cnt %" PRId32 ", ∫P %fkWh, frequency %fHz, status 0x%08X", v_rms, i_rms,
           watt, cf_cnt, total_energy_consumption, frequency, data->status);
}

void BL0942::dump_config() {  // NOLINT(readability-function-cognitive-complexity)
  ESP_LOGCONFIG(TAG, "BL0942:");
  ESP_LOGCONFIG(TAG, "  Address: %d", this->address_);
  ESP_LOGCONFIG(TAG, "  Nominal line frequency: %d Hz", this->line_freq_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Energy", this->energy_sensor_);
  LOG_SENSOR("", "frequency", this->frequency_sensor_);
}

}  // namespace bl0942
}  // namespace esphome
