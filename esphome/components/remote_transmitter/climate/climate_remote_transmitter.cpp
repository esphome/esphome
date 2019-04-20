#include "climate_remote_transmitter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate {

static const char *TAG = "climate.remote_transmitter";

const uint32_t kCoolixOff = 0xB27BE0;
// On, 25C, Mode: Auto, Fan: Auto, Zone Follow: Off, Sensor Temp: Ignore.
const uint32_t kCoolixDefaultState =  0xB2BFC8; 
const uint32_t kCoolixDefaultStateAutoFan = 0xB21FC8;
const uint8_t kCoolixCool = 0b00;
const uint8_t kCoolixDry = 0b01;
const uint8_t kCoolixAuto = 0b10;
const uint8_t kCoolixHeat = 0b11;
const uint8_t kCoolixFan = 4;                                 // Synthetic.
const uint32_t kCoolixModeMask = 0b000000000000000000001100;  // 0xC

// Temperature
const uint8_t kCoolixTempMin = 17;  // Celsius
const uint8_t kCoolixTempMax = 30;  // Celsius
const uint8_t kCoolixTempRange = kCoolixTempMax - kCoolixTempMin + 1;
const uint8_t kCoolixFanTempCode = 0b1110;  // Part of Fan Mode.
const uint32_t kCoolixTempMask = 0b11110000;
const uint8_t kCoolixTempMap[kCoolixTempRange] = {
    0b0000,  // 17C
    0b0001,  // 18c
    0b0011,  // 19C
    0b0010,  // 20C
    0b0110,  // 21C
    0b0111,  // 22C
    0b0101,  // 23C
    0b0100,  // 24C
    0b1100,  // 25C
    0b1101,  // 26C
    0b1001,  // 27C
    0b1000,  // 28C
    0b1010,  // 29C
    0b1011   // 30C
};

// Constants
// Pulse parms are *50-100 for the Mark and *50+100 for the space
// First MARK is the one after the long gap
// pulse parameters in usec
const uint16_t kCoolixTick = 560;  // Approximately 21 cycles at 38kHz
const uint16_t kCoolixBitMarkTicks = 1;
const uint16_t kCoolixBitMark = kCoolixBitMarkTicks * kCoolixTick;
const uint16_t kCoolixOneSpaceTicks = 3;
const uint16_t kCoolixOneSpace = kCoolixOneSpaceTicks * kCoolixTick;
const uint16_t kCoolixZeroSpaceTicks = 1;
const uint16_t kCoolixZeroSpace = kCoolixZeroSpaceTicks * kCoolixTick;
const uint16_t kCoolixHdrMarkTicks = 8;
const uint16_t kCoolixHdrMark = kCoolixHdrMarkTicks * kCoolixTick;
const uint16_t kCoolixHdrSpaceTicks = 8;
const uint16_t kCoolixHdrSpace = kCoolixHdrSpaceTicks * kCoolixTick;
const uint16_t kCoolixMinGapTicks = kCoolixHdrMarkTicks + kCoolixZeroSpaceTicks;
const uint16_t kCoolixMinGap = kCoolixMinGapTicks * kCoolixTick;

const uint16_t kCoolixBits = 24;

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
void ClimateRemoteTransmitter::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->current_temperature = NAN;
  this->transmit_state_();
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
void ClimateRemoteTransmitter::transmit_state_() {
  uint32_t remote_state;

  switch (this->mode) {
    case CLIMATE_MODE_OFF:
      remote_state = kCoolixOff;
      break;
    case CLIMATE_MODE_COOL:
      remote_state = kCoolixDefaultState;
      remote_state = (remote_state & ~kCoolixModeMask) | (kCoolixCool << 2);
      break;
    case CLIMATE_MODE_HEAT:
      remote_state = kCoolixDefaultState;
      remote_state = (remote_state & ~kCoolixModeMask) | (kCoolixHeat << 2);
      break;
    case CLIMATE_MODE_AUTO:
      remote_state = kCoolixDefaultStateAutoFan;
      break;
  }
  if (this->mode != CLIMATE_MODE_OFF)
  {
    uint8_t temp = std::min((uint8_t)this->target_temperature, kCoolixTempMax);
    temp = std::max((uint8_t)this->target_temperature, kCoolixTempMin);
    remote_state &= ~kCoolixTempMask;  // Clear the old temp.
    remote_state |= (kCoolixTempMap[temp - kCoolixTempMin] << 4);
  }

  ESP_LOGD(TAG, "Sending coolix code: %u", remote_state);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);
  uint16_t repeat = 1;
  for (uint16_t r = 0; r <= repeat; r++) {
    // Header
    data->mark(kCoolixHdrMark);
    data->space(kCoolixHdrSpace);

    // Data
    //   Break data into byte segments, starting at the Most Significant
    //   Byte. Each byte then being sent normal, then followed inverted.
    for (uint16_t i = 8; i <= kCoolixBits; i += 8) {
      // Grab a bytes worth of data.
      uint8_t segment = (remote_state >> (kCoolixBits - i)) & 0xFF;
      // Normal
      sendData(data, kCoolixBitMark, kCoolixOneSpace, kCoolixBitMark,
               kCoolixZeroSpace, segment, 8, true);
      // Inverted.
      sendData(data, kCoolixBitMark, kCoolixOneSpace, kCoolixBitMark,
               kCoolixZeroSpace, segment ^ 0xFF, 8, true);
    }

    // Footer
    data->mark(kCoolixBitMark);
    data->space(kCoolixMinGap);  // Pause before repeating
  }

  transmit.perform();
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
