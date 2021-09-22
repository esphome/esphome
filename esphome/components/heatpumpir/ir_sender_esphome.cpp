#include "ir_sender_esphome.h"

#ifdef USE_ARDUINO

namespace esphome {
namespace heatpumpir {

void IRSenderESPHome::setFrequency(int frequency) {  // NOLINT(readability-identifier-naming)
  auto data = transmit_.get_data();
  data->set_carrier_frequency(1000 * frequency);
}

// Send an IR 'mark' symbol, i.e. transmitter ON
void IRSenderESPHome::mark(int mark_length) {
  auto data = transmit_.get_data();
  data->mark(mark_length);
}

// Send an IR 'space' symbol, i.e. transmitter OFF
void IRSenderESPHome::space(int space_length) {
  if (space_length) {
    auto data = transmit_.get_data();
    data->space(space_length);
  } else {
    transmit_.perform();
  }
}

}  // namespace heatpumpir
}  // namespace esphome

#endif
