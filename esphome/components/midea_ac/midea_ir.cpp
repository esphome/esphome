#include "midea_ir.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midea_ac {

static const char *const TAG = "climate.midea_ac";

void MideaCommand::set_on_timer(uint16_t minutes) {
  uint8_t halfhours = std::min<uint16_t>(24 * 60, minutes) / 30;
  this->data_[4] = halfhours ? ((halfhours - 1) * 2 + 1) : 0xFF;
}

void MideaCommand::set_off_timer(uint16_t minutes) {
  uint8_t halfhours = std::min<uint16_t>(24 * 60, minutes) / 30;
  this->set_value_(3, 0b111111, 1, halfhours ? (halfhours - 1) : 0b111111);
}

}  // namespace midea_ac
}  // namespace esphome
