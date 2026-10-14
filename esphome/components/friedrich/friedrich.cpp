#include "friedrich.h"

#include <cmath>

#include "esphome/components/remote_base/aeha_protocol.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::friedrich {
// clang-format off
/*
* MW12Y3H - Currently the only supported model. Model field is retained to allow future per-model code differentiation.
* AEHA
* Byte0  Fixed = 0x00
* Byte1  Fixed = 0x08
* Byte2  Fixed = 0x08
* Byte3  Command: Move Louver = 0x36 Off = 0x40, else Fixed = 0x7F
* Byte4  Checksum when command, else Fixed = 0x90
* Byte5  N/A when command, else Fixed = 0x0C
* Byte6  N/A when command, else Temp(F), add 0x80 for On
* Byte7  N/A when command, else Mode (Min Heat = 0xD0 and Coil Dry = 0x10 are not exposed)
* Byte8  N/A when command, else Fan, add 0x08 for Vertical Swing
* Byte9 N/A when command, else Fixed = 0x00 // not interested
* Byte10 N/A when command, else Fixed = 0x00 // not interested
* Byte11 N/A when command, else Fixed = 0x00 // not interested
* Byte12 N/A when command, else Economy (On = 0x00, Off = 0x04)
* Byte13 N/A when command, else CheckSum
*/
// clang-format on
ESPHOME_LOG_TAG(TAG, "friedrich");

static constexpr uint8_t TEMP_MIN_NOT_HEAT = 64;
static constexpr uint8_t TEMP_DEFAULT = 72;
static constexpr uint8_t STATE_MESSAGE_LENGTH = FriedrichClimate::STATE_MESSAGE_LENGTH;
static constexpr uint8_t STATE_MESSAGE_LENGTH_UTIL = 5;
static constexpr uint16_t CARRIER_ADDRESS = 0x28C6;
static constexpr uint32_t CARRIER_FREQUENCY = 38000;

static constexpr uint8_t BYTE3_POWER_OFF = 0x40;
static constexpr uint8_t BYTE6_POWER_ON = 0x80;
static constexpr uint8_t BYTE7_MODE_AUTO = 0x00;
static constexpr uint8_t BYTE8_FAN_AUTO = 0x00;
static constexpr uint8_t BYTE8_FAN_SWING = 0x08;

// Every state frame: power on, economy off; bytes 6, 7, 8 and 13 are patched per state
static constexpr uint8_t STATE_TEMPLATE[STATE_MESSAGE_LENGTH] PROGMEM = {
    0x00, 0x08, 0x08, 0x7F, 0x90, 0x0C, BYTE6_POWER_ON, BYTE7_MODE_AUTO, BYTE8_FAN_AUTO, 0x00, 0x00, 0x00, 0x04, 0x00};
// The off command, including its checksum (255 - byte 3)
static constexpr uint8_t OFF_FRAME[STATE_MESSAGE_LENGTH_UTIL] PROGMEM = {0x00, 0x08, 0x08, BYTE3_POWER_OFF,
                                                                         255 - BYTE3_POWER_OFF};

struct Code {
  uint8_t value;  // a climate enum, all of which are uint8_t
  uint8_t code;
};
static constexpr Code MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_HEAT_COOL, BYTE7_MODE_AUTO},
    {climate::CLIMATE_MODE_COOL, 0x80},
    {climate::CLIMATE_MODE_DRY, 0x40},
    {climate::CLIMATE_MODE_FAN_ONLY, 0xC0},
    {climate::CLIMATE_MODE_HEAT, 0x20},
};
static constexpr Code FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, BYTE8_FAN_AUTO}, {climate::CLIMATE_FAN_HIGH, 0x80},
    {climate::CLIMATE_FAN_MEDIUM, 0x40},         {climate::CLIMATE_FAN_LOW, 0xC0},
    {climate::CLIMATE_FAN_QUIET, 0x20},
};

// The entry whose value (or code) matches key. Entries are copied out of PROGMEM.
template<size_t N> static optional<Code> find_entry(const Code (&table)[N], uint8_t key, bool match_code) {
  for (const Code &slot : table) {
    Code entry;
    progmem_memcpy(&entry, &slot, sizeof(entry));
    if ((match_code ? entry.code : entry.value) == key)
      return entry;
  }
  return {};
}

// The temperature code is the 4-bit reversal of (F - TEMP_MIN) / TEMP_STEP; the mapping is its own inverse.
static uint8_t reverse_nibble(uint8_t value) { return reverse_bits(value) >> 4; }

static bool matches_template(const uint8_t *data, uint8_t from, uint8_t to) {
  for (uint8_t i = from; i <= to; i++) {
    if (data[i] != progmem_read_byte(&STATE_TEMPLATE[i]))
      return false;
  }
  return true;
}

/*
 * https://gist.github.com/GeorgeDewar/11171561
 * 1. Reverse (flip) bytes 6 - 13)
 * 2. Sum those bytes
 * 3. (208 - sum) % 256
 * 4. Reverse (flip) bytes of result
 */
static uint8_t checksum_state(const uint8_t *data) {
  uint8_t sum = 0;
  for (uint8_t i = 6; i < STATE_MESSAGE_LENGTH - 1; ++i) {
    sum += reverse_bits(data[i]);
  }
  return reverse_bits(static_cast<uint8_t>(208 - sum));
}

static const LogString *model_to_string(Model model) {
  switch (model) {
    case Model::MODEL_MW12Y3H:
    default:
      return LOG_STR("MW12Y3H");
  }
}

climate::ClimateTraits FriedrichClimate::traits() {
  auto traits = ClimateIR::traits();
  traits.set_temperature_unit(TemperatureUnit::FAHRENHEIT);
  return traits;
}

void FriedrichClimate::setup() {
  ClimateIR::setup();
  // The base class defaults assume Celsius. This also discards a value saved in Celsius by an earlier version.
  if (std::isnan(this->target_temperature) || this->target_temperature < TEMP_MIN) {
    this->target_temperature = TEMP_DEFAULT;
  }
  this->target_temperature = std::min<float>(this->target_temperature, TEMP_MAX);
}

void FriedrichClimate::dump_config() {
  // ClimateIR::dump_config() is not called: it prints the temperature range in Celsius
  LOG_CLIMATE("", "Friedrich Climate", this);
  ESP_LOGCONFIG(TAG,
                "  Temperature range: %u to %u °F\n"
                "  Model: %s",
                TEMP_MIN, TEMP_MAX, LOG_STR_ARG(model_to_string(this->model_)));
}

void FriedrichClimate::transmit_state() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_off_();
    return;
  }

  ESP_LOGV(TAG, "Transmit state");

  // Snap to what the unit accepts (even values, 64°F minimum unless heating) and store it,
  // so the state published after this call matches the frame sent.
  const uint8_t effective_min = this->mode == climate::CLIMATE_MODE_HEAT ? TEMP_MIN : TEMP_MIN_NOT_HEAT;
  uint8_t temperature = (uint8_t) roundf(clamp<float>(this->target_temperature, effective_min, TEMP_MAX));
  temperature += temperature % 2;
  this->target_temperature = temperature;

  const auto mode = find_entry(MODES, this->mode, false);
  const auto fan = find_entry(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), false);

  uint8_t frame[STATE_MESSAGE_LENGTH];
  progmem_memcpy(frame, STATE_TEMPLATE, sizeof(frame));
  frame[6] |= reverse_nibble((temperature - TEMP_MIN) / TEMP_STEP);
  frame[7] = mode.has_value() ? mode->code : BYTE7_MODE_AUTO;
  frame[8] = fan.has_value() ? fan->code : BYTE8_FAN_AUTO;
  if (this->swing_mode != climate::CLIMATE_SWING_OFF)
    frame[8] |= BYTE8_FAN_SWING;
  frame[STATE_MESSAGE_LENGTH - 1] = checksum_state(frame);

  this->transmit_(frame, sizeof(frame));
}

void FriedrichClimate::transmit_off_() {
  ESP_LOGV(TAG, "Transmit off");

  uint8_t frame[STATE_MESSAGE_LENGTH_UTIL];
  progmem_memcpy(frame, OFF_FRAME, sizeof(frame));
  this->transmit_(frame, sizeof(frame));
}

void FriedrichClimate::transmit_(const uint8_t *data, uint8_t len) {
  ESP_LOGV(TAG, "Transmit message");

  auto transmit = this->transmitter_->transmit();
  auto *dst = transmit.get_data();
  dst->set_carrier_frequency(CARRIER_FREQUENCY);
  remote_base::AEHAProtocol().encode(dst, CARRIER_ADDRESS, data, len);
  transmit.perform();
}

bool FriedrichClimate::on_receive(remote_base::RemoteReceiveData src) {
  ESP_LOGV(TAG, "Received message");
  const auto odata = remote_base::AEHAProtocol().decode(src);
  if (!odata.has_value())
    return false;
  const auto &d = odata->data;
  // Other vendors also use AEHA; only accept frames with our address and fixed header bytes.
  if (odata->address != CARRIER_ADDRESS || d.size() < STATE_MESSAGE_LENGTH_UTIL || !matches_template(d.data(), 0, 2))
    return false;

  if (d.size() == STATE_MESSAGE_LENGTH_UTIL) {
    // Not looking for other types of messages
    if (d[3] != BYTE3_POWER_OFF || d[4] != 255 - d[3])
      return false;
    ESP_LOGV(TAG, "Received off message");
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
    return true;
  }

  if (d.size() != STATE_MESSAGE_LENGTH || d[STATE_MESSAGE_LENGTH - 1] != checksum_state(d.data()) ||
      !matches_template(d.data(), 3, 5))
    return false;

  // Ignore frames with a mode this component does not expose (e.g. MIN_HEAT, COIL_DRY)
  const auto mode = find_entry(MODES, d[7], true);
  if (!mode.has_value()) {
    ESP_LOGV(TAG, "Ignoring unknown mode 0x%02X", d[7]);
    return false;
  }
  this->mode = static_cast<climate::ClimateMode>(mode->value);

  // State frames always carry the power-on flag in bit 7 (power off is the separate 5-byte frame), so it is
  // stripped rather than acted on
  const uint8_t temp_code = d[6] & ~BYTE6_POWER_ON;
  const uint8_t temp_index = reverse_nibble(temp_code);
  if (temp_code <= 0x0F && temp_index <= (TEMP_MAX - TEMP_MIN) / TEMP_STEP)
    this->target_temperature = TEMP_MIN + temp_index * TEMP_STEP;

  this->swing_mode = (d[8] & BYTE8_FAN_SWING) ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  const auto fan = find_entry(FANS, d[8] & ~BYTE8_FAN_SWING, true);
  this->fan_mode = fan.has_value() ? static_cast<climate::ClimateFanMode>(fan->value) : climate::CLIMATE_FAN_AUTO;

  this->publish_state();
  return true;
}

}  // namespace esphome::friedrich
