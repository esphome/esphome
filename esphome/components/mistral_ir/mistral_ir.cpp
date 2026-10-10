#include "mistral_ir.h"
#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mistral_ir {

ESPHOME_LOG_TAG(TAG, "mistral_ir.climate");

// Fixed bytes of every frame; bytes 1, 3, 4, 5, 6 and 11 are filled in per state
static const uint8_t FRAME_TEMPLATE[MISTRAL_FRAME_SIZE] PROGMEM = {0x56, 0x00, 0x08, 0x00, 0x00, 0x00,
                                                                   0x00, 0x00, 0xE8, 0x00, 0x60, 0x00};
static const uint8_t FIXED_BYTES[] PROGMEM = {0, 2, 7, 8, 9, 10};
static const uint8_t CHECKSUM_BYTES[] PROGMEM = {1, 3, 4, 5, 6, 8};

constexpr uint8_t MISTRAL_POWER_ON = 0x20;
constexpr uint8_t MISTRAL_TEMP_OFFSET = 0x9F;
constexpr uint8_t MISTRAL_MODE_AUTO = 0x10;

// The tables live in flash, so entries are copied out before use
struct ModeCode {
  climate::ClimateMode mode;
  uint8_t code;
};
static const ModeCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_COOL, 0xC0},
    {climate::CLIMATE_MODE_DRY, 0x40},
    {climate::CLIMATE_MODE_FAN_ONLY, 0x80},
    {climate::CLIMATE_MODE_HEAT, 0x60},
    {climate::CLIMATE_MODE_HEAT_COOL, MISTRAL_MODE_AUTO},
};

struct FanCode {
  climate::ClimateFanMode fan;
  uint8_t byte1;
  uint8_t byte6;
};
static const FanCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, 0xA4, 0x00},
    {climate::CLIMATE_FAN_LOW, 0x44, 0x40},
    {climate::CLIMATE_FAN_MEDIUM, 0x24, 0xC0},
    {climate::CLIMATE_FAN_HIGH, 0xA4, 0xA0},
};

template<typename T> static T load_entry(const T &slot) {
  T entry;
  progmem_memcpy(&entry, &slot, sizeof(T));
  return entry;
}

template<typename T, size_t N, typename Pred> static optional<T> find_entry(const T (&table)[N], Pred pred) {
  for (const T &slot : table) {
    T entry = load_entry(slot);
    if (pred(entry))
      return entry;
  }
  return {};
}

static uint8_t compute_checksum(const uint8_t *frame) {
  uint8_t sum = 0;
  for (const uint8_t &idx : CHECKSUM_BYTES)
    sum += reverse_bits(frame[progmem_read_byte(&idx)]);
  return reverse_bits(sum);
}

static bool is_valid_frame(const uint8_t *frame) {
  for (const uint8_t &slot : FIXED_BYTES) {
    const uint8_t idx = progmem_read_byte(&slot);
    if (frame[idx] != progmem_read_byte(&FRAME_TEMPLATE[idx]))
      return false;
  }
  return frame[11] == compute_checksum(frame);
}

void MistralIR::transmit_state() {
  const auto mode = find_entry(MODES, [this](const ModeCode &m) { return m.mode == this->mode; });
  const climate::ClimateFanMode fan_mode = this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO);
  const FanCode fan =
      find_entry(FANS, [fan_mode](const FanCode &f) { return f.fan == fan_mode; }).value_or(load_entry(FANS[0]));
  const uint8_t temp = static_cast<uint8_t>(clamp<float>(this->target_temperature, MISTRAL_TEMP_MIN, MISTRAL_TEMP_MAX));

  uint8_t frame[MISTRAL_FRAME_SIZE];
  progmem_memcpy(frame, FRAME_TEMPLATE, sizeof(frame));
  frame[1] = fan.byte1;
  frame[3] = this->mode == climate::CLIMATE_MODE_OFF ? 0x00 : MISTRAL_POWER_ON;
  frame[4] = mode.has_value() ? mode->code : MISTRAL_MODE_AUTO;
  frame[5] = reverse_bits(static_cast<uint8_t>(MISTRAL_TEMP_OFFSET - temp));
  frame[6] = fan.byte6;
  frame[11] = compute_checksum(frame);

  auto transmit = this->transmitter_->transmit();
  remote_base::AEHAProtocol().encode(transmit.get_data(), MISTRAL_ADDRESS, frame, sizeof(frame));
  transmit.perform();
}

bool MistralIR::on_receive(remote_base::RemoteReceiveData data) {
  auto aeha = remote_base::AEHAProtocol().decode(data);
  if (!aeha.has_value() || aeha->address != MISTRAL_ADDRESS || aeha->data.size() != MISTRAL_FRAME_SIZE)
    return false;

  const uint8_t *frame = aeha->data.data();
  if (!is_valid_frame(frame))
    return false;

  if (frame[3] != MISTRAL_POWER_ON) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
    return true;
  }

  // Decode everything before touching the state so an unknown code leaves it untouched
  const auto mode = find_entry(MODES, [frame](const ModeCode &m) { return m.code == frame[4]; });
  const auto fan = find_entry(FANS, [frame](const FanCode &f) { return f.byte6 == frame[6]; });
  if (!mode.has_value() || !fan.has_value())
    return false;

  this->mode = mode->mode;
  this->target_temperature =
      clamp<float>(MISTRAL_TEMP_OFFSET - reverse_bits(frame[5]), MISTRAL_TEMP_MIN, MISTRAL_TEMP_MAX);
  this->fan_mode = fan->fan;

  ESP_LOGV(TAG, "Received Mistral frame: mode=%d temp=%.0f fan=%d", static_cast<int>(this->mode),
           this->target_temperature, static_cast<int>(fan->fan));
  this->publish_state();
  return true;
}

}  // namespace esphome::mistral_ir
