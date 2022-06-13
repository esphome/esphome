#include "es8388_component.h"

namespace esphome {
namespace es8388 {

void ES8388Component::setup() {
  this->power_pin_->setup();
  this->power_pin_->digital_write(false);

  // reset
  this->write_bytes(0x00, {0x80});
  this->write_bytes(0x00, {0x00});
  // mute
  this->write_bytes(0x19, {0x04});
  this->write_bytes(0x01, {0x50});
  // powerup
  this->write_bytes(0x02, {0x00});
  // worker mode
  this->write_bytes(0x08, {0x00});
  // DAC powerdown
  this->write_bytes(0x04, {0xC0});
  // vmidsel/500k ADC/DAC idem
  this->write_bytes(0x00, {0x12});

  this->write_bytes(0x01, {0x00});
  // i2s 16 bits
  this->write_bytes(0x17, {0x18});
  // sample freq 256
  this->write_bytes(0x18, {0x02});
  // LIN2/RIN2 for mixer
  this->write_bytes(0x26, {0x09});
  // left DAC to left mixer
  this->write_bytes(0x27, {0x90});
  // right DAC to right mixer
  this->write_bytes(0x2A, {0x90});
  // DACLRC ADCLRC idem
  this->write_bytes(0x2B, {0x80});
  this->write_bytes(0x2D, {0x00});
  // DAC volume max
  this->write_bytes(0x1B, {0x00});
  this->write_bytes(0x1A, {0x00});

  this->write_bytes(0x02, {0xF0});
  this->write_bytes(0x02, {0x00});
  this->write_bytes(0x1D, {0x1C});
  // DAC power-up LOUT1/ROUT1 enabled
  this->write_bytes(0x04, {0x30});
  // unmute
  this->write_bytes(0x19, {0x00});

  // Turn on amplifier
  this->power_pin_->digital_write(true);
}
}  // namespace es8388
}  // namespace esphome
