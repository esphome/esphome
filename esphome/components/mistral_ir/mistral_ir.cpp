#include "mistral_ir.h"
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/core/log.h"

namespace esphome::mistral_ir {

static const char *const TAG = "mistral_ir.climate";

constexpr uint16_t MISTRAL_ADDRESS = 0x322C;

constexpr uint8_t MISTRAL_MODE_COOL = 0xC0;
constexpr uint8_t MISTRAL_MODE_DRY = 0x40;
constexpr uint8_t MISTRAL_MODE_FAN_ONLY = 0x80;
constexpr uint8_t MISTRAL_MODE_HEAT = 0x60;
constexpr uint8_t MISTRAL_MODE_AUTO = 0x10;

constexpr uint8_t MISTRAL_POWER_ON = 0x20;
constexpr uint8_t MISTRAL_POWER_OFF = 0x00;

constexpr uint8_t MISTRAL_FAN_LOW_B1 = 0x44;
constexpr uint8_t MISTRAL_FAN_LOW_B6 = 0x40;
constexpr uint8_t MISTRAL_FAN_MEDIUM_B1 = 0x24;
constexpr uint8_t MISTRAL_FAN_MEDIUM_B6 = 0xC0;
constexpr uint8_t MISTRAL_FAN_HIGH_B1 = 0xA4;
constexpr uint8_t MISTRAL_FAN_HIGH_B6 = 0xA0;
constexpr uint8_t MISTRAL_FAN_AUTO_B1 = 0xA4;
constexpr uint8_t MISTRAL_FAN_AUTO_B6 = 0x00;

// Fixed bytes present in every frame regardless of state.
constexpr uint8_t MISTRAL_HEADER = 0x56;
constexpr uint8_t MISTRAL_BYTE2 = 0x08;
constexpr uint8_t MISTRAL_BYTE7 = 0x00;
constexpr uint8_t MISTRAL_BYTE8 = 0xE8;
constexpr uint8_t MISTRAL_BYTE9 = 0x00;
constexpr uint8_t MISTRAL_BYTE10 = 0x60;

// The checksum (and the temperature encoding) operate on bit-reversed byte values.
static uint8_t bit_reverse(uint8_t b) {
  uint8_t result = 0;
  for (uint8_t i = 0; i < 8; i++)
    result |= ((b >> i) & 1) << (7 - i);
  return result;
}

static uint8_t temperature_to_byte(uint8_t temp_celsius) {
  return bit_reverse(static_cast<uint8_t>(0x9F - temp_celsius));
}

static uint8_t byte_to_temperature(uint8_t byte5) { return static_cast<uint8_t>(0x9F - bit_reverse(byte5)); }

static uint8_t compute_checksum(const std::array<uint8_t, 12> &frame) {
  uint8_t sum = 0;
  for (uint8_t idx : {1, 3, 4, 5, 6, 8})
    sum += bit_reverse(frame[idx]);
  return bit_reverse(sum);
}

void MistralIR::transmit_state() {
  uint8_t mode_byte;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      mode_byte = MISTRAL_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      mode_byte = MISTRAL_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_byte = MISTRAL_MODE_FAN_ONLY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      mode_byte = MISTRAL_MODE_HEAT;
      break;
    default:
      mode_byte = MISTRAL_MODE_AUTO;
      break;
  }

  uint8_t fan_byte1;
  uint8_t fan_byte6;
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      fan_byte1 = MISTRAL_FAN_LOW_B1;
      fan_byte6 = MISTRAL_FAN_LOW_B6;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_byte1 = MISTRAL_FAN_MEDIUM_B1;
      fan_byte6 = MISTRAL_FAN_MEDIUM_B6;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_byte1 = MISTRAL_FAN_HIGH_B1;
      fan_byte6 = MISTRAL_FAN_HIGH_B6;
      break;
    default:
      fan_byte1 = MISTRAL_FAN_AUTO_B1;
      fan_byte6 = MISTRAL_FAN_AUTO_B6;
      break;
  }

  const uint8_t power_byte = this->mode == climate::CLIMATE_MODE_OFF ? MISTRAL_POWER_OFF : MISTRAL_POWER_ON;
  const uint8_t temp = static_cast<uint8_t>(clamp<float>(this->target_temperature, MISTRAL_TEMP_MIN, MISTRAL_TEMP_MAX));

  std::array<uint8_t, 12> frame = {
      MISTRAL_HEADER, fan_byte1,     MISTRAL_BYTE2, power_byte,    mode_byte,      temperature_to_byte(temp),
      fan_byte6,      MISTRAL_BYTE7, MISTRAL_BYTE8, MISTRAL_BYTE9, MISTRAL_BYTE10, 0x00};
  frame[11] = compute_checksum(frame);

  auto transmit = this->transmitter_->transmit();
  remote_base::AEHAProtocol().encode(transmit.get_data(),
                                     {MISTRAL_ADDRESS, std::vector<uint8_t>(frame.begin(), frame.end())});
  transmit.perform();
}

bool MistralIR::on_receive(remote_base::RemoteReceiveData data) {
  auto aeha = remote_base::AEHAProtocol().decode(data);
  if (!aeha.has_value() || aeha->address != MISTRAL_ADDRESS || aeha->data.size() != 12)
    return false;

  const std::vector<uint8_t> &frame = aeha->data;
  if (frame[3] == MISTRAL_POWER_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
    return true;
  }

  switch (frame[4]) {
    case MISTRAL_MODE_COOL:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case MISTRAL_MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    case MISTRAL_MODE_FAN_ONLY:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case MISTRAL_MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      break;
    default:
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      break;
  }

  this->target_temperature = byte_to_temperature(frame[5]);

  switch (frame[6]) {
    case MISTRAL_FAN_LOW_B6:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case MISTRAL_FAN_MEDIUM_B6:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case MISTRAL_FAN_HIGH_B6:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  ESP_LOGV(TAG, "Received Mistral frame: mode=%d temp=%.0f fan=%d", static_cast<int>(this->mode),
           this->target_temperature, static_cast<int>(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)));
  this->publish_state();
  return true;
}

}  // namespace esphome::mistral_ir
