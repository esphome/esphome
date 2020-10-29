#include "ir_sender_esphome.h"

namespace esphome {
namespace heatpumpir {

void IRSenderESPHome::setFrequency(int frequency)
{
  auto data = _transmit.get_data();
  data->set_carrier_frequency(1000*frequency);
}

// Send an IR 'mark' symbol, i.e. transmitter ON
void IRSenderESPHome::mark(int markLength)
{
  auto data = _transmit.get_data();
  data->mark(markLength);
}

// Send an IR 'space' symbol, i.e. transmitter OFF
void IRSenderESPHome::space(int spaceLength)
{
  if (spaceLength) {
    auto data = _transmit.get_data();
    data->space(spaceLength);
  } else {
    _transmit.perform();
  }
}

}  // namespace heatpumpir
}  // namespace esphome
