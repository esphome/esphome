#include "climate_remote_transmitter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate {

const uint16_t kTcl112AcStateLength = 14;
const uint16_t kTcl112AcBits = kTcl112AcStateLength * 8;

const uint8_t kTcl112AcHeat = 1;
const uint8_t kTcl112AcDry =  2;
const uint8_t kTcl112AcCool = 3;
const uint8_t kTcl112AcFan =  7;
const uint8_t kTcl112AcAuto = 8;

const uint8_t kTcl112AcPowerMask = 0x04;

const uint8_t kTcl112AcHalfDegree = 0b00100000;
const float   kTcl112AcTempMax = 31.0;
const float   kTcl112AcTempMin = 16.0;

const uint16_t kTcl112AcHdrMark = 3000;
const uint16_t kTcl112AcHdrSpace = 1650;
const uint16_t kTcl112AcBitMark = 500;
const uint16_t kTcl112AcOneSpace = 1050;
const uint16_t kTcl112AcZeroSpace = 325;
const uint32_t kTcl112AcGap = kTcl112AcHdrSpace;  // Just a guess.

static const char *TAG = "climate.remote_transmitter.tcl";

void ClimateRemoteTransmitter::transmit_state_tcl_() {
  uint8_t remote_state[kTcl112AcStateLength];

  for (uint8_t i = 0; i < kTcl112AcStateLength; i++)
    remote_state[i] = 0x0;
  // A known good state. (On, Cool, 24C)
  remote_state[0] =  0x23;
  remote_state[1] =  0xCB;
  remote_state[2] =  0x26;
  remote_state[3] =  0x01;
  remote_state[5] =  0x24;
  remote_state[6] =  0x03;
  remote_state[7] =  0x07;
  remote_state[8] =  0x40;

  // Set mode
  switch (this->mode) {
    case CLIMATE_MODE_AUTO:
      remote_state[6] &= 0xF0;
      remote_state[6] |= kTcl112AcAuto;
      break;
    case CLIMATE_MODE_COOL:
      remote_state[6] &= 0xF0;
      remote_state[6] |= kTcl112AcCool;
      break;
    case CLIMATE_MODE_HEAT:
      remote_state[6] &= 0xF0;
      remote_state[6] |= kTcl112AcHeat;
      break;
    case CLIMATE_MODE_OFF:
    default:
      remote_state[5] &= ~kTcl112AcPowerMask;
      break;
  }

  // Set temperature
  // Make sure we have desired temp in the correct range.
  float safecelsius = std::max(this->target_temperature, kTcl112AcTempMin);
  safecelsius = std::min(safecelsius, kTcl112AcTempMax);
  // Convert to integer nr. of half degrees.
  uint8_t nrHalfDegrees = safecelsius * 2;
  if (nrHalfDegrees & 1)  // Do we have a half degree celsius?
    remote_state[12] |= kTcl112AcHalfDegree;  // Add 0.5 degrees
  else
    remote_state[12] &= ~kTcl112AcHalfDegree;  // Clear the half degree.
  remote_state[7] &= 0xF0;  // Clear temp bits.
  remote_state[7] |= ((uint8_t)kTcl112AcTempMax - nrHalfDegrees / 2);

  // Calculate & set the checksum for the current internal state of the remote.
  // Stored the checksum value in the last byte.
  for (uint8_t checksumByte = 0; checksumByte < kTcl112AcStateLength - 1; checksumByte++)
    remote_state[kTcl112AcStateLength - 1] += remote_state[checksumByte];

  ESP_LOGD(TAG, "Sending tcl code: %u", remote_state[7]);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(kTcl112AcHdrMark);
  data->space(kTcl112AcHdrSpace);
  // Data
  for (uint16_t i = 0; i < kTcl112AcStateLength; i++)
    this->sendData(data, 
                   kTcl112AcBitMark, kTcl112AcOneSpace, kTcl112AcBitMark, kTcl112AcZeroSpace,
                   remote_state[i], 8, false);
  // Footer
  data->mark(kTcl112AcBitMark);
  data->space(kTcl112AcGap);
  transmit.perform();
}

}  // namespace climate
}  // namespace esphome
