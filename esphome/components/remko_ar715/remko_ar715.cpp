#include "remko_ar715.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::remko_ar715 {

ESPHOME_LOG_TAG(TAG, "remko_ar715.climate");

// Timings from ir_ClimaButler.cpp (IRremoteESP8266), in microseconds
static constexpr uint16_t BIT_MARK = 511;
static constexpr uint16_t HDR_SPACE = 3492;
static constexpr uint16_t ONE_SPACE = 1540;
static constexpr uint16_t ZERO_SPACE = 548;
static constexpr uint32_t CARRIER_FREQUENCY = 38000;
static constexpr uint8_t BITS = 52;

static constexpr uint8_t POWER_ON = 0x3;
static constexpr uint8_t POWER_OFF = 0x0;
static constexpr uint8_t SWING_ON = 0x5;
static constexpr uint8_t SWING_OFF = 0x7;

// Mode/fan byte: high nibble N3, low nibble N2
static constexpr uint8_t MODE_FAN_AUTO = 0x80;
static constexpr uint8_t MODE_FAN_LOW = 0xA2;
static constexpr uint8_t MODE_FAN_MEDIUM = 0x91;
static constexpr uint8_t MODE_FAN_HIGH = 0x88;
static constexpr uint8_t MODE_DRY = 0xA4;
static constexpr uint8_t MODE_FAN_ONLY = 0xA3;

static uint8_t fan_mode_byte(climate::ClimateFanMode fan_mode) {
  switch (fan_mode) {
    case climate::CLIMATE_FAN_HIGH:
      return MODE_FAN_HIGH;
    case climate::CLIMATE_FAN_MEDIUM:
      return MODE_FAN_MEDIUM;
    case climate::CLIMATE_FAN_LOW:
      return MODE_FAN_LOW;
    default:
      return MODE_FAN_AUTO;
  }
}

// 52-bit word, 13 nibbles MSB first:
//   N12=0x8 (fixed), N11=power, N10..N5=0 (timer, unused), N4=swing,
//   N3,N2=mode/fan, N1=temp-16, N0=checksum (0xF - sum(N12..N1)) & 0xF
static uint64_t build_frame(uint8_t power, uint8_t swing, uint8_t mode_fan, uint8_t temp) {
  const uint8_t temp_nibble = (temp - 16) & 0xF;
  const uint8_t sum = 0x8 + power + swing + (mode_fan >> 4) + (mode_fan & 0xF) + temp_nibble;
  const uint8_t checksum = (0xF - sum) & 0xF;
  return (uint64_t{0x8} << 48) | (uint64_t{power} << 44) | (uint32_t{swing} << 16) | (uint32_t{mode_fan} << 8) |
         (temp_nibble << 4) | checksum;
}

static void send_frame(remote_base::RemoteTransmitData *data, uint64_t code) {
  data->set_carrier_frequency(CARRIER_FREQUENCY);
  data->reserve(2 + BITS * 2 + 2);
  data->item(BIT_MARK, HDR_SPACE);
  for (int i = BITS - 1; i >= 0; i--)
    data->item(BIT_MARK, (code >> i) & 1 ? ONE_SPACE : ZERO_SPACE);
  // Stop bit and inter-frame gap
  data->item(BIT_MARK, HDR_SPACE);
}

void RemkoAr715Climate::transmit_state() {
  const uint8_t temp = lroundf(clamp(this->target_temperature, TEMP_MIN, TEMP_MAX));
  const bool power = this->mode != climate::CLIMATE_MODE_OFF;
  const bool swing = this->swing_mode == climate::CLIMATE_SWING_VERTICAL;

  // DRY and FAN_ONLY carry no fan speed: mode and fan share the same nibbles
  uint8_t mode_fan;
  switch (this->mode) {
    case climate::CLIMATE_MODE_DRY:
      mode_fan = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_fan = MODE_FAN_ONLY;
      break;
    default:
      mode_fan = fan_mode_byte(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
      break;
  }

  const uint64_t code = build_frame(power ? POWER_ON : POWER_OFF, swing ? SWING_ON : SWING_OFF, mode_fan, temp);
  ESP_LOGV(TAG, "Sending power=%s temp=%u swing=%s mode_fan=0x%02X code=0x%05" PRIX32 "%08" PRIX32, ONOFF(power), temp,
           ONOFF(swing), mode_fan, static_cast<uint32_t>(code >> 32), static_cast<uint32_t>(code));

  auto transmit = this->transmitter_->transmit();
  send_frame(transmit.get_data(), code);
  transmit.perform();
}

}  // namespace esphome::remko_ar715
