#include "climate_remote_transmitter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate {

static const char *TAG = "climate.remote_transmitter.coolix";

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

void ClimateRemoteTransmitter::transmit_state_coolix_() {
  uint32_t remote_state;

  switch (this->mode) {
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
    case CLIMATE_MODE_OFF:
    default:
      remote_state = kCoolixOff;
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

}  // namespace climate
}  // namespace esphome
