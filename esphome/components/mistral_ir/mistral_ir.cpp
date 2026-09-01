#include "mistral_ir.h"
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/core/log.h"

namespace esphome::mistral_ir {

static const char *const TAG = "mistral_ir.climate";

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

static uint8_t compute_checksum(const std::vector<uint8_t> &frame) {
  uint8_t sum = 0;
  for (uint8_t idx : {1, 3, 4, 5, 6, 8})
    sum += bit_reverse(frame[idx]);
  return bit_reverse(sum);
}

// Validates the fixed bytes and checksum of a received frame, to reject corrupted or unrelated
// AEHA frames that happen to share the Mistral address and length before acting on their contents.
static bool is_valid_frame(const std::vector<uint8_t> &frame) {
  return frame[0] == MISTRAL_HEADER && frame[2] == MISTRAL_BYTE2 && frame[7] == MISTRAL_BYTE7 &&
         frame[8] == MISTRAL_BYTE8 && frame[9] == MISTRAL_BYTE9 && frame[10] == MISTRAL_BYTE10 &&
         frame[11] == compute_checksum(frame);
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

  std::vector<uint8_t> &frame = this->frame_.data;
  frame[0] = MISTRAL_HEADER;
  frame[1] = fan_byte1;
  frame[2] = MISTRAL_BYTE2;
  frame[3] = power_byte;
  frame[4] = mode_byte;
  frame[5] = temperature_to_byte(temp);
  frame[6] = fan_byte6;
  frame[7] = MISTRAL_BYTE7;
  frame[8] = MISTRAL_BYTE8;
  frame[9] = MISTRAL_BYTE9;
  frame[10] = MISTRAL_BYTE10;
  frame[11] = compute_checksum(frame);

  auto transmit = this->transmitter_->transmit();
  remote_base::AEHAProtocol().encode(transmit.get_data(), this->frame_);
  transmit.perform();
}

bool MistralIR::on_receive(remote_base::RemoteReceiveData data) {
  auto aeha = remote_base::AEHAProtocol().decode(data);
  if (!aeha.has_value() || aeha->address != MISTRAL_ADDRESS || aeha->data.size() != 12)
    return false;

  const std::vector<uint8_t> &frame = aeha->data;
  if (!is_valid_frame(frame))
    return false;

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
    case MISTRAL_MODE_AUTO:
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
      break;
    default:
      return false;
  }

  this->target_temperature = clamp<float>(byte_to_temperature(frame[5]), MISTRAL_TEMP_MIN, MISTRAL_TEMP_MAX);

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
    case MISTRAL_FAN_AUTO_B6:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    default:
      return false;
  }

  ESP_LOGV(TAG, "Received Mistral frame: mode=%d temp=%.0f fan=%d", static_cast<int>(this->mode),
           this->target_temperature, static_cast<int>(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)));
  this->publish_state();
  return true;
}

}  // namespace esphome::mistral_ir
