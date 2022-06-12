#include "es8388_component.h"

namespace esphome {
namespace es8388 {

void ES8388Component::setup() {
  this->power_pin_->setup();
  this->power_pin_->digital_write(false);

  // reset
  this->write_bytes(0x00, {0x80}, 1);
  this->write_bytes(0x00, {0x00}, 1);
  // mute
  this->write_bytes(0x19, {0x04}, 1);
  this->write_bytes(0x01, {0x50}, 1);
  // powerup
  this->write_bytes(0x02, {0x00}, 1);
  // slave mode
  this->write_bytes(0x08, {0x00}, 1);
  // DAC powerdown
  this->write_bytes(0x04, {0xC0}, 1);
  // vmidsel/500k ADC/DAC idem
  this->write_bytes(0x00, {0x12}, 1);

  this->write_bytes(0x01, {0x00}, 1);
  // i2s 16 bits
  this->write_bytes(0x17, {0x18}, 1);
  // sample freq 256
  this->write_bytes(0x18, {0x02}, 1);
  // LIN2/RIN2 for mixer
  this->write_bytes(0x26, {0x09}, 1);
  // left DAC to left mixer
  this->write_bytes(0x27, {0x90}, 1);
  // right DAC to right mixer
  this->write_bytes(0x2A, {0x90}, 1);
  // DACLRC ADCLRC idem
  this->write_bytes(0x2B, {0x80}, 1);
  this->write_bytes(0x2D, {0x00}, 1);
  // DAC volume max
  this->write_bytes(0x1B, {0x00}, 1);
  this->write_bytes(0x1A, {0x00}, 1);

  this->write_bytes(0x02, {0xF0}, 1);
  this->write_bytes(0x02, {0x00}, 1);
  this->write_bytes(0x1D, {0x1C}, 1);
  // DAC power-up LOUT1/ROUT1 enabled
  this->write_bytes(0x04, {0x30}, 1);
  // unmute
  this->write_bytes(0x19, {0x00}, 1);

  // Turn on amplifier
  this->power_pin_->digital_write(true);
}
}  // namespace es8388
}  // namespace esphome
