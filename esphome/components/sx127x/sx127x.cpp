#include "sx127x.h"

namespace esphome {
namespace sx127x {

static const char *const TAG = "sx127x";

uint8_t SX127x::read_register_(uint8_t reg) { return this->single_transfer_((uint8_t) reg & 0x7f, 0x00); }

void SX127x::write_register_(uint8_t reg, uint8_t value) { this->single_transfer_((uint8_t) reg | 0x80, value); }

uint8_t SX127x::single_transfer_(uint8_t reg, uint8_t value) {
  uint8_t response;
  this->delegate_->begin_transaction();
  this->nss_pin_->digital_write(false);
  this->delegate_->transfer(reg);
  response = this->delegate_->transfer(value);
  this->nss_pin_->digital_write(true);
  this->delegate_->end_transaction();
  return response;
}

void SX127x::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SX127x...");

  // setup nss and set high
  this->nss_pin_->setup();
  this->nss_pin_->digital_write(true);

  // setup reset
  this->rst_pin_->setup();

  // start spi
  this->spi_setup();

  // configure rf
  this->configure();
}

void SX127x::configure() {
  // toggle chip reset
  this->rst_pin_->digital_write(false);
  delay(1);
  this->rst_pin_->digital_write(true);
  delay(10);

  // check silicon version to make sure hw is ok
  if (this->read_register_(REG_VERSION) != 0x12) {
    this->mark_failed();
    return;
  }

  // set modulation and make sure transceiver is in sleep mode
  this->write_register_(REG_OP_MODE, this->modulation_ | MODE_LF_ON | MODE_SLEEP);
  delay(1);

  // set freq
  uint64_t frf = ((uint64_t) this->frequency_ << 19) / 32000000;
  this->write_register_(REG_FRF_MSB, (uint8_t) ((frf >> 16) & 0xFF));
  this->write_register_(REG_FRF_MID, (uint8_t) ((frf >> 8) & 0xFF));
  this->write_register_(REG_FRF_LSB, (uint8_t) ((frf >> 0) & 0xFF));

  // set fdev
  uint32_t fdev = std::min(this->fsk_fdev_ / 61, 0x3FFFu);
  this->write_register_(REG_FDEV_MSB, (uint8_t) ((fdev >> 8) & 0xFF));
  this->write_register_(REG_FDEV_LSB, (uint8_t) ((fdev >> 0) & 0xFF));

  // set the channel bw
  this->write_register_(REG_RX_BW, this->rx_bandwidth_);

  // config pa
  if (this->pa_pin_ == PA_PIN_BOOST) {
    this->pa_power_ = std::max(this->pa_power_, 2u);
    this->pa_power_ = std::min(this->pa_power_, 17u);
    this->write_register_(REG_PA_CONFIG, (this->pa_power_ - 2) | this->pa_pin_ | PA_MAX_POWER);
  } else {
    this->pa_power_ = std::min(this->pa_power_, 14u);
    this->write_register_(REG_PA_CONFIG, (this->pa_power_ - 0) | this->pa_pin_ | PA_MAX_POWER);
  }
  if (this->modulation_ == MOD_FSK) {
    this->write_register_(REG_PA_RAMP, this->fsk_ramp_ | this->fsk_shaping_);
  } else {
    this->write_register_(REG_PA_RAMP, this->fsk_ramp_);
  }

  // disable packet mode
  this->write_register_(REG_PACKET_CONFIG_1, 0x00);
  this->write_register_(REG_PACKET_CONFIG_2, 0x00);

  // disable bit synchronizer, disable sync generation and setup threshold
  this->write_register_(REG_SYNC_CONFIG, 0x00);
  this->write_register_(REG_OOK_PEAK, OOK_THRESH_STEP_0_5 | OOK_THRESH_PEAK);
  this->write_register_(REG_OOK_AVG, OOK_THRESH_DEC_1_8);

  // set ook floor
  this->write_register_(REG_OOK_FIX, 256 + int(this->rx_floor_ * 2.0));

  // enable standby mode
  this->set_mode_standby();
  delay(1);

  // enable rx mode
  if (this->rx_start_) {
    this->set_mode_rx();
    delay(1);
  }
}

void SX127x::set_mode_standby() { this->write_register_(REG_OP_MODE, this->modulation_ | MODE_LF_ON | MODE_STDBY); }

void SX127x::set_mode_rx() {
  this->write_register_(REG_OP_MODE, this->modulation_ | MODE_LF_ON | MODE_RX_FS);
  delay(1);
  this->write_register_(REG_OP_MODE, this->modulation_ | MODE_LF_ON | MODE_RX);
}

void SX127x::set_mode_tx() {
  this->write_register_(REG_OP_MODE, this->modulation_ | MODE_LF_ON | MODE_TX_FS);
  delay(1);
  this->write_register_(REG_OP_MODE, this->modulation_ | MODE_LF_ON | MODE_TX);
}

void SX127x::dump_config() {
  static const uint16_t RAMP_LUT[16] = {3400, 2000, 1000, 500, 250, 125, 100, 62, 50, 40, 31, 25, 20, 15, 12, 10};
  uint32_t rx_bw_mant = 16 + (this->rx_bandwidth_ >> 3) * 4;
  uint32_t rx_bw_exp = this->rx_bandwidth_ & 0x7;
  float rx_bw = (float) 32000000 / (rx_bw_mant * (1 << (rx_bw_exp + 2)));
  ESP_LOGCONFIG(TAG, "SX127x:");
  LOG_PIN("  NSS Pin: ", this->nss_pin_);
  LOG_PIN("  RST Pin: ", this->rst_pin_);
  ESP_LOGCONFIG(TAG, "  PA Pin: %s", this->pa_pin_ == PA_PIN_BOOST ? "BOOST" : "RFO");
  ESP_LOGCONFIG(TAG, "  PA Power: %d dBm", this->pa_power_);
  ESP_LOGCONFIG(TAG, "  Frequency: %f MHz", (float) this->frequency_ / 1000000);
  ESP_LOGCONFIG(TAG, "  Modulation: %s", this->modulation_ == MOD_FSK ? "FSK" : "OOK");
  ESP_LOGCONFIG(TAG, "  Rx Bandwidth: %.1f kHz", (float) rx_bw / 1000);
  ESP_LOGCONFIG(TAG, "  Rx Start: %s", this->rx_start_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Rx Floor: %.1f dBm", this->rx_floor_);
  ESP_LOGCONFIG(TAG, "  FSK Fdev: %d Hz", this->fsk_fdev_);
  ESP_LOGCONFIG(TAG, "  FSK Ramp: %d us", RAMP_LUT[this->fsk_ramp_]);
  if (this->fsk_shaping_ == SHAPING_BT_1_0) {
    ESP_LOGCONFIG(TAG, "  FSK Shaping: BT_1_0");
  } else if (this->fsk_shaping_ == SHAPING_BT_0_5) {
    ESP_LOGCONFIG(TAG, "  FSK Shaping: BT_0_5");
  } else if (this->fsk_shaping_ == SHAPING_BT_0_3) {
    ESP_LOGCONFIG(TAG, "  FSK Shaping: BT_0_3");
  } else {
    ESP_LOGCONFIG(TAG, "  FSK Shaping: NONE");
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Configuring SX127x failed");
  }
}

}  // namespace sx127x
}  // namespace esphome
