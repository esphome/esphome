#include "mpr121.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mpr121 {

static const char *TAG = "mpr121";

void MPR121Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MPR121...");
  // soft reset device
  this->write_byte(MPR121_SOFTRESET, 0x63);
  delay(100);  // NOLINT
  if (!this->write_byte(MPR121_ECR, 0x0)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  // set touch sensitivity for all 12 channels
  for (auto *channel : this->channels_) {
    this->write_byte(MPR121_TOUCHTH_0 + 2 * channel->channel_,
                     channel->touch_threshold_.value_or(this->touch_threshold_));
    this->write_byte(MPR121_RELEASETH_0 + 2 * channel->channel_,
                     channel->release_threshold_.value_or(this->release_threshold_));
  }
  this->write_byte(MPR121_MHDR, 0x01);
  this->write_byte(MPR121_NHDR, 0x01);
  this->write_byte(MPR121_NCLR, 0x0E);
  this->write_byte(MPR121_FDLR, 0x00);

  this->write_byte(MPR121_MHDF, 0x01);
  this->write_byte(MPR121_NHDF, 0x05);
  this->write_byte(MPR121_NCLF, 0x01);
  this->write_byte(MPR121_FDLF, 0x00);

  this->write_byte(MPR121_NHDT, 0x00);
  this->write_byte(MPR121_NCLT, 0x00);
  this->write_byte(MPR121_FDLT, 0x00);

  this->write_byte(MPR121_DEBOUNCE, 0);
  // default, 16uA charge current
  this->write_byte(MPR121_CONFIG1, 0x10);
  // 0.5uS encoding, 1ms period
  this->write_byte(MPR121_CONFIG2, 0x20);
  // start with first 5 bits of baseline tracking
  this->write_byte(MPR121_ECR, 0x8F);
}

void MPR121Component::set_touch_debounce(uint8_t debounce) {
  uint8_t mask = debounce << 4;
  this->debounce_ &= 0x0f;
  this->debounce_ |= mask;
}

void MPR121Component::set_release_debounce(uint8_t debounce) {
  uint8_t mask = debounce & 0x0f;
  this->debounce_ &= 0xf0;
  this->debounce_ |= mask;
};

void MPR121Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MPR121:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with MPR121 failed!");
      break;
    case WRONG_CHIP_STATE:
      ESP_LOGE(TAG, "MPR121 has wrong default value for CONFIG2?");
      break;
    case NONE:
    default:
      break;
  }
}
void MPR121Component::loop() {
  uint16_t val = 0;
  this->read_byte_16(MPR121_TOUCHSTATUS_L, &val);

  // Flip order
  uint8_t lsb = val >> 8;
  uint8_t msb = val;
  val = (uint16_t(msb) << 8) | lsb;

  for (auto *channel : this->channels_)
    channel->process(val);
}

}  // namespace mpr121
}  // namespace esphome
