// ESPHome climate component for Remko AR-715 IR remote.
// https://github.com/crankyoldgit/IRremoteESP8266/issues/1812
//
// Supported devices:
//   - Remko RKL series (RKL 490, 491, 494, 495) with AR-715 remote
//   - Fischer ClimaButler RCS-SD43UAI / RCS-SD43UWI
//   - TROTEC PAC 4600
//   - Novamatic CL 990 / CL 1590
//   - Rexair C15000N
//   - freecom RCS-SD43UAI / RCS-SD43UWI

#include "remko_ar715.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome::remko_ar715 {

static const char *const TAG = "remko_ar715.climate";

void RemkoAr715Climate::transmit_state() {
  uint8_t temp = (uint8_t) lroundf(this->target_temperature);
  temp = std::max((uint8_t) REMKO_AR715_TEMP_MIN, std::min((uint8_t) REMKO_AR715_TEMP_MAX, temp));

  bool power = (this->mode != climate::CLIMATE_MODE_OFF);
  bool swing = (this->swing_mode == climate::CLIMATE_SWING_VERTICAL);

  uint8_t n3, n2;
  if (this->mode == climate::CLIMATE_MODE_DRY) {
    n3 = REMKO_AR715_MODE_DRY_N3;
    n2 = REMKO_AR715_MODE_DRY_N2;
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    n3 = REMKO_AR715_MODE_FAN_N3;
    n2 = REMKO_AR715_MODE_FAN_N2;
  } else {
    // COOL and OFF: apply fan speed
    switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
      case climate::CLIMATE_FAN_HIGH:
        n3 = REMKO_AR715_FAN_HIGH_N3;
        n2 = REMKO_AR715_FAN_HIGH_N2;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        n3 = REMKO_AR715_FAN_MEDIUM_N3;
        n2 = REMKO_AR715_FAN_MEDIUM_N2;
        break;
      case climate::CLIMATE_FAN_LOW:
        n3 = REMKO_AR715_FAN_LOW_N3;
        n2 = REMKO_AR715_FAN_LOW_N2;
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        n3 = REMKO_AR715_FAN_AUTO_N3;
        n2 = REMKO_AR715_FAN_AUTO_N2;
        break;
    }
  }

  uint64_t code = this->build_code_(power, swing, n3, n2, temp);
  ESP_LOGD(TAG, "Sending code 0x%013llX (power=%s temp=%u swing=%s N3=0x%X N2=0x%X)", code, ONOFF(power), temp,
           ONOFF(swing), n3, n2);
  this->send_(code);
}

uint64_t RemkoAr715Climate::build_code_(bool power, bool swing, uint8_t n3, uint8_t n2, uint8_t temp) {
  // 52-bit word, 13 nibbles MSB->LSB:
  //   N12=0x8 (fixed header)
  //   N11=power
  //   N10..N5=0x0 (timer, unused)
  //   N4=swing
  //   N3,N2=mode/fan
  //   N1=temp-16
  //   N0=checksum: (0xF - sum(N12..N1)) mod 16
  uint8_t nibs[12] = {
      0x8,
      static_cast<uint8_t>(power ? REMKO_AR715_POWER_ON : REMKO_AR715_POWER_OFF),
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      0x0,
      static_cast<uint8_t>(swing ? REMKO_AR715_SWING_ON : REMKO_AR715_SWING_OFF),
      n3,
      n2,
      static_cast<uint8_t>((temp - 16) & 0xF),
  };
  uint8_t sum = 0;
  for (uint8_t nib : nibs)
    sum += nib;
  uint8_t n0 = (0xF - (sum & 0xF)) & 0xF;

  uint64_t code = 0;
  for (uint8_t nib : nibs)
    code = (code << 4) | nib;
  return (code << 4) | n0;
}

void RemkoAr715Climate::send_(uint64_t code) {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(REMKO_AR715_FREQUENCY);

  // Header
  data->mark(REMKO_AR715_BIT_MARK);
  data->space(REMKO_AR715_HDR_SPACE);

  // 52 data bits, MSB first, pulse-distance encoding
  for (int i = REMKO_AR715_BITS - 1; i >= 0; i--) {
    data->mark(REMKO_AR715_BIT_MARK);
    data->space((code >> i) & 1 ? REMKO_AR715_ONE_SPACE : REMKO_AR715_ZERO_SPACE);
  }

  // Stop bit + inter-frame gap
  data->mark(REMKO_AR715_BIT_MARK);
  data->space(REMKO_AR715_HDR_SPACE);

  transmit.perform();
}

}  // namespace esphome::remko_ar715
