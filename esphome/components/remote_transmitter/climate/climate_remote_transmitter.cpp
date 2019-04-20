#include "climate_remote_transmitter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate {

static const char *TAG = "climate.remote_transmitter";


void ClimateRemoteTransmitter::setup() {
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    // restore from defaults, change_away handles those for us
    this->mode = climate::CLIMATE_MODE_AUTO;
  }
}

void ClimateRemoteTransmitter::set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter)
{
  this->transmitter_ = transmitter;
}
  
void ClimateRemoteTransmitter::set_model(ClimateRemoteTransmitterModel model)
{
  this->model_ = model;
  switch (model)
  {
    case CLIMATE_REMOTE_TRANSMITTER_COOLIX:
      this->set_visual_max_temperature_override(30);
      this->set_visual_min_temperature_override(17);
      this->set_visual_temperature_step_override(1);
      break;
    case CLIMATE_REMOTE_TRANSMITTER_TCL:
      this->set_visual_max_temperature_override(30);
      this->set_visual_min_temperature_override(16);
      this->set_visual_temperature_step_override(.5f);
      break;
  }
}

void ClimateRemoteTransmitter::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->current_temperature = NAN;

  switch (this->model_)
  {
    case CLIMATE_REMOTE_TRANSMITTER_COOLIX:
      this->transmit_state_coolix_();
      break;
    case CLIMATE_REMOTE_TRANSMITTER_TCL:
      this->transmit_state_tcl_();
      break;
  }
  this->publish_state();
}

climate::ClimateTraits ClimateRemoteTransmitter::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  return traits;
}

// Generic method for sending data that is common to most protocols.
// Will send leading or trailing 0's if the nbits is larger than the number
// of bits in data.
//
// Args:
//   onemark:    Nr. of usecs for the led to be pulsed for a '1' bit.
//   onespace:   Nr. of usecs for the led to be fully off for a '1' bit.
//   zeromark:   Nr. of usecs for the led to be pulsed for a '0' bit.
//   zerospace:  Nr. of usecs for the led to be fully off for a '0' bit.
//   data:       The data to be transmitted.
//   nbits:      Nr. of bits of data to be sent.
//   MSBfirst:   Flag for bit transmission order. Defaults to MSB->LSB order.
void ClimateRemoteTransmitter::sendData(remote_base::RemoteTransmitData *transmitData, 
                                        uint16_t onemark, uint32_t onespace, uint16_t zeromark,
                                        uint32_t zerospace, uint64_t data, uint16_t nbits,
                                        bool MSBfirst) {
  if (nbits == 0)  // If we are asked to send nothing, just return.
    return;
  if (MSBfirst) {  // Send the MSB first.
    // Send 0's until we get down to a bit size we can actually manage.
    while (nbits > sizeof(data) * 8) {
      transmitData->mark(zeromark);
      transmitData->space(zerospace);
      nbits--;
    }
    // Send the supplied data.
    for (uint64_t mask = 1ULL << (nbits - 1); mask; mask >>= 1)
      if (data & mask) {  // Send a 1
        transmitData->mark(onemark);
        transmitData->space(onespace);
      } else {  // Send a 0
        transmitData->mark(zeromark);
        transmitData->space(zerospace);
      }
  } else {  // Send the Least Significant Bit (LSB) first / MSB last.
    for (uint16_t bit = 0; bit < nbits; bit++, data >>= 1)
      if (data & 1) {  // Send a 1
        transmitData->mark(onemark);
        transmitData->space(onespace);
      } else {  // Send a 0
        transmitData->mark(zeromark);
        transmitData->space(zerospace);
      }
  }
}

ClimateRemoteTransmitter::ClimateRemoteTransmitter(const std::string &name) : climate::Climate(name) {}
void ClimateRemoteTransmitter::set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
void ClimateRemoteTransmitter::set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }

}  // namespace climate
}  // namespace esphome
