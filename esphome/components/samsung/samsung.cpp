#include "samsung.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::samsung {

ESPHOME_LOG_TAG(TAG, "samsung.climate");

static constexpr uint32_t CARRIER_FREQUENCY_HZ = 38000;
static constexpr uint32_t HEADER_MARK_US = 3000;
static constexpr uint32_t HEADER_SPACE_US = 9000;
static constexpr uint32_t BIT_MARK_US = 500;
static constexpr uint32_t ONE_SPACE_US = 1500;
static constexpr uint32_t ZERO_SPACE_US = 500;
static constexpr uint32_t SECTION_SPACE_US = 2000;

static constexpr uint8_t EXTENDED_LENGTH = 3 * SAMSUNG_AC_SECTION_LENGTH;

// A powered-on unit at 24 °C, cool mode, fan auto, swing off (from IRremoteESP8266); checksums are refilled on send
static const uint8_t RESET_STATE[SAMSUNG_AC_STATE_LENGTH] PROGMEM = {0x02, 0x92, 0x0F, 0x00, 0x00, 0x00, 0xF0,
                                                                     0x01, 0xE2, 0xFE, 0x71, 0x80, 0x11, 0xF0};
// Middle section of a power on/off frame: an all-zero timer block
static const uint8_t TIMER_SECTION[SAMSUNG_AC_SECTION_LENGTH] PROGMEM = {0x01, 0xD2, 0x0F, 0x00, 0x00, 0x00, 0x00};

// Where each setting lives in the 14 byte state
struct Field {
  uint8_t byte;
  uint8_t shift;
  uint8_t mask;
};
static constexpr Field POWER_1{6, 4, 0b11};
static constexpr Field SWING{9, 4, 0b111};
static constexpr Field TEMP{11, 4, 0b1111};
static constexpr Field FAN{12, 1, 0b111};
static constexpr Field MODE{12, 4, 0b111};
static constexpr Field POWER_2{13, 4, 0b11};
static constexpr uint8_t POWER_ON = 0b11;

static uint8_t get_field(const uint8_t *state, Field field) { return (state[field.byte] >> field.shift) & field.mask; }
static void set_field(uint8_t *state, Field field, uint8_t value) {
  state[field.byte] = (state[field.byte] & ~(field.mask << field.shift)) | ((value & field.mask) << field.shift);
}
static bool is_powered_on(const uint8_t *state) {
  return get_field(state, POWER_1) == POWER_ON && get_field(state, POWER_2) == POWER_ON;
}

template<typename T> struct Code {
  T value;
  uint8_t code;
};
// The first entry of each table is the fallback in both directions
static constexpr Code<climate::ClimateMode> MODES[] = {
    {climate::CLIMATE_MODE_HEAT_COOL, 0}, {climate::CLIMATE_MODE_COOL, 1}, {climate::CLIMATE_MODE_DRY, 2},
    {climate::CLIMATE_MODE_FAN_ONLY, 3},  {climate::CLIMATE_MODE_HEAT, 4},
};
static constexpr Code<climate::ClimateFanMode> FANS[] = {
    {climate::CLIMATE_FAN_AUTO, 0},
    {climate::CLIMATE_FAN_LOW, 2},
    {climate::CLIMATE_FAN_MEDIUM, 4},
    {climate::CLIMATE_FAN_HIGH, 5},
};
static constexpr Code<climate::ClimateSwingMode> SWINGS[] = {
    {climate::CLIMATE_SWING_OFF, 0b111},
    {climate::CLIMATE_SWING_VERTICAL, 0b010},
    {climate::CLIMATE_SWING_HORIZONTAL, 0b011},
    {climate::CLIMATE_SWING_BOTH, 0b100},
};

template<typename T, size_t N> static uint8_t to_code(const Code<T> (&table)[N], T value) {
  for (const auto &entry : table) {
    if (entry.value == value)
      return entry.code;
  }
  return table[0].code;
}
template<typename T, size_t N> static T from_code(const Code<T> (&table)[N], uint8_t code) {
  for (const auto &entry : table) {
    if (entry.code == code)
      return entry.value;
  }
  return table[0].value;
}

// Inverted count of the one bits in a section, skipping the two nibbles that hold the checksum itself.
// See https://github.com/crankyoldgit/IRremoteESP8266/issues/1538#issuecomment-894645947
static uint8_t section_checksum(const uint8_t *section) {
  uint32_t ones =
      __builtin_popcount(section[0]) + __builtin_popcount(section[1] & 0x0F) + __builtin_popcount(section[2] & 0xF0);
  for (uint8_t i = 3; i < SAMSUNG_AC_SECTION_LENGTH; i++) {
    ones += __builtin_popcount(section[i]);
  }
  return ~static_cast<uint8_t>(ones);
}
static uint8_t stored_checksum(const uint8_t *section) { return (section[1] >> 4) | (section[2] << 4); }
static void store_checksum(uint8_t *section) {
  const uint8_t sum = section_checksum(section);
  section[1] = (section[1] & 0x0F) | (sum << 4);
  section[2] = (section[2] & 0xF0) | (sum >> 4);
}

SamsungClimate::SamsungClimate()
    : climate_ir::ClimateIR(
          SAMSUNG_AC_TEMP_MIN, SAMSUNG_AC_TEMP_MAX, 1.0f, true, true,
          {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
          {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
           climate::CLIMATE_SWING_BOTH}) {
  progmem_memcpy(this->state_.data(), RESET_STATE, SAMSUNG_AC_STATE_LENGTH);
  // The unit's power state is unknown at boot, so the first "on" command sends a power frame
  set_field(this->state_.data(), POWER_1, 0);
  set_field(this->state_.data(), POWER_2, 0);
}

void SamsungClimate::transmit_state() {
  uint8_t *state = this->state_.data();
  const bool was_on = is_powered_on(state);
  const bool on = this->mode != climate::CLIMATE_MODE_OFF;

  if (on) {
    progmem_memcpy(state, RESET_STATE, SAMSUNG_AC_STATE_LENGTH);
    set_field(state, MODE, to_code(MODES, this->mode));
  }  // else keep the last mode so the power off frame still carries a valid state

  const uint8_t temp = clamp<uint8_t>(this->target_temperature, SAMSUNG_AC_TEMP_MIN, SAMSUNG_AC_TEMP_MAX);
  set_field(state, TEMP, temp - SAMSUNG_AC_TEMP_MIN);
  set_field(state, SWING, to_code(SWINGS, this->swing_mode));
  set_field(state, FAN, to_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)));
  set_field(state, POWER_1, on ? POWER_ON : 0);
  set_field(state, POWER_2, on ? POWER_ON : 0);

  if (on != was_on) {
    // A power change is sent as an extended frame with the timer block between the two state sections
    uint8_t timer[SAMSUNG_AC_SECTION_LENGTH];
    progmem_memcpy(timer, TIMER_SECTION, SAMSUNG_AC_SECTION_LENGTH);
    const uint8_t *sections[] = {state, timer, state + SAMSUNG_AC_SECTION_LENGTH};
    this->send_sections_(sections, 3);
  } else {
    const uint8_t *sections[] = {state, state + SAMSUNG_AC_SECTION_LENGTH};
    this->send_sections_(sections, 2);
  }
}

void SamsungClimate::send_sections_(const uint8_t *const *sections, uint8_t count) {
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  // Header(2) + (sections - 1) * separator(4) + bits(2 each) + trailing mark(1)
  data->reserve(2 + (count - 1) * 4 + count * SAMSUNG_AC_SECTION_LENGTH * 8 * 2 + 1);
  data->set_carrier_frequency(CARRIER_FREQUENCY_HZ);
  data->item(HEADER_MARK_US, HEADER_SPACE_US);

  for (uint8_t s = 0; s < count; s++) {
    if (s != 0) {
      data->item(BIT_MARK_US, SECTION_SPACE_US);
      data->item(HEADER_MARK_US, HEADER_SPACE_US);
    }
    uint8_t section[SAMSUNG_AC_SECTION_LENGTH];
    memcpy(section, sections[s], SAMSUNG_AC_SECTION_LENGTH);
    store_checksum(section);
    for (uint8_t byte : section) {
      for (uint8_t bit = 0; bit < 8; bit++, byte >>= 1) {
        data->item(BIT_MARK_US, (byte & 1) ? ONE_SPACE_US : ZERO_SPACE_US);
      }
    }
  }

  data->mark(BIT_MARK_US);
  transmit.perform();
}

bool SamsungClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(HEADER_MARK_US, HEADER_SPACE_US)) {
    return false;
  }

  // Decode into a local buffer so a bad frame leaves the stored state untouched
  uint8_t frame[EXTENDED_LENGTH] = {0};
  uint8_t length = EXTENDED_LENGTH;
  for (uint8_t i = 0; i < EXTENDED_LENGTH; i++) {
    if (i != 0 && i % SAMSUNG_AC_SECTION_LENGTH == 0) {
      const bool have_separator =
          data.expect_item(BIT_MARK_US, SECTION_SPACE_US) && data.expect_item(HEADER_MARK_US, HEADER_SPACE_US);
      if (!have_separator) {
        // A third section is optional; a missing separator after the second one ends the frame
        if (i >= SAMSUNG_AC_STATE_LENGTH) {
          length = i;
          break;
        }
        ESP_LOGW(TAG, "Missing section separator before byte %u", i);
        return false;
      }
    }
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(BIT_MARK_US, ONE_SPACE_US)) {
        frame[i] |= 1 << bit;
      } else if (!data.expect_item(BIT_MARK_US, ZERO_SPACE_US)) {
        ESP_LOGW(TAG, "Bad bit %u in byte %u", bit, i);
        return false;
      }
    }
  }
  if (!data.expect_mark(BIT_MARK_US)) {
    ESP_LOGVV(TAG, "Footer fail");
    return false;
  }
  for (uint8_t offset = 0; offset < length; offset += SAMSUNG_AC_SECTION_LENGTH) {
    if (stored_checksum(frame + offset) != section_checksum(frame + offset)) {
      ESP_LOGW(TAG, "Checksum mismatch in section %u", offset / SAMSUNG_AC_SECTION_LENGTH);
      return false;
    }
  }

  // In an extended frame the middle section is the timer block and the last one the second state section
  uint8_t *state = this->state_.data();
  memcpy(state, frame, SAMSUNG_AC_SECTION_LENGTH);
  memcpy(state + SAMSUNG_AC_SECTION_LENGTH, frame + length - SAMSUNG_AC_SECTION_LENGTH, SAMSUNG_AC_SECTION_LENGTH);

  this->swing_mode = from_code(SWINGS, get_field(state, SWING));
  this->target_temperature = get_field(state, TEMP) + SAMSUNG_AC_TEMP_MIN;
  this->fan_mode = from_code(FANS, get_field(state, FAN));
  this->mode = is_powered_on(state) ? from_code(MODES, get_field(state, MODE)) : climate::CLIMATE_MODE_OFF;

  ESP_LOGD(TAG, "Received %u byte frame, power %s", length, ONOFF(is_powered_on(state)));
  this->publish_state();
  return true;
}

}  // namespace esphome::samsung
