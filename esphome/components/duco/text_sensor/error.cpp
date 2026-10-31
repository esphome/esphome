#include "error.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "esphome/core/progmem.h"

namespace esphome::duco {

PROGMEM_STRING_TABLE(ErrorCodes, "E01.04.01", "E03.01.01", "E03.01.02", "E03.01.03", "E03.01.04",
                     "E03.01.05", "E03.01.06", "E04.01.01", "E04.01.02", "E04.01.03", "E04.01.04",
                     "E04.01.05", "E04.01.06", "E06.01.01", "E06.01.02", "E06.01.03", "E06.01.04",
                     "E06.01.05", "E06.01.06", "E16.01.01", "E16.01.02", "E16.01.03", "E16.01.04",
                     "E16.01.05", "E16.01.06", "E28.04.01", "E29.01.01", "E29.01.02", "E29.01.03",
                     "E29.01.04", "E29.01.05", "E29.01.06", "E30.01.01", "E30.01.02", "E30.01.03",
                     "E30.01.04", "E30.01.05", "E30.01.06", "E31.19.01", "E32.10.01", "E33.10.01",
                     "E35.17.01", "E35.17.02", "E35.17.03", "E36.24.01", "E36.24.02", "E37.17.01",
                     "E37.17.02", "E37.17.03", "E40.17.01", "E40.17.02", "E40.17.03", "E42.22.01",
                     "E42.22.02", "E43.23.01", "E43.23.02", "E44.06.01", "E44.06.02");
PROGMEM_STRING_TABLE(
    ErrorDescriptions, "No response from the extractor fan.", "ODA temperature sensor value is too low.",
    "ODA temperature sensor value is too high.", "ODA temperature sensor cannot be read.",
    "ODA temperature sensor value is unchanged for too long.", "ODA temperature sensor is short-circuited.",
    "ODA temperature sensor has a bad connection.", "ETA temperature sensor value is too low.",
    "ETA temperature sensor value is too high.", "ETA temperature sensor cannot be read.",
    "ETA temperature sensor value is unchanged for too long.", "ETA temperature sensor is short-circuited.",
    "ETA temperature sensor has a bad connection.", "EHA temperature sensor value is too low.",
    "EHA temperature sensor value is too high.", "EHA temperature sensor cannot be read.",
    "EHA temperature sensor value is unchanged for too long.", "EHA temperature sensor is short-circuited.",
    "EHA temperature sensor has a bad connection.", "Extract air pressure sensor value is too low.",
    "Extract air pressure sensor value is too high.", "Extract air pressure sensor cannot be read.",
    "Extract air pressure sensor value is unchanged for too long.", "Extract air pressure sensor is short-circuited.",
    "Extract air pressure sensor has a bad connection.", "No response from the supply fan.",
    "Supply air pressure sensor value is too low.", "Supply air pressure sensor value is too high.",
    "Supply air pressure sensor cannot be read.", "Supply air pressure sensor value is unchanged for too long.",
    "Supply air pressure sensor is short-circuited.", "Supply air pressure sensor has a bad connection.",
    "SUP temperature sensor value is too low.", "SUP temperature sensor value is too high.",
    "SUP temperature sensor cannot be read.", "SUP temperature sensor value is unchanged for too long.",
    "SUP temperature sensor is short-circuited.", "SUP temperature sensor has a bad connection.",
    "Filters need replacement.",
    "Outdoor temperature is too low; ventilation is off for 8 hours.",
    "Outdoor temperature is too low; ventilation is permanently off.", "Temperature sensor value is too low.",
    "Temperature sensor value is too high.", "Temperature sensor cannot be read.",
    "Connection to the weather station has been lost.", "Connection to the outdoor temperature sensor has been lost.",
    "CO2 sensor value is too low.", "CO2 sensor value is too high.", "CO2 sensor cannot be read.",
    "Humidity sensor value is too low.", "Humidity sensor value is too high.", "Humidity sensor cannot be read.",
    "Too many valves are connected.", "Too few valves are connected; at least two are required.",
    "Flow adjustment has not happened or failed.", "The requested flow rate cannot be achieved.",
    "No connection to the Duco Pre-Heater.", "A Duco Pre-Heater is connected but not configured.",
    "Unknown DUCO error");
static_assert(ErrorCodes::COUNT + 1 == ErrorDescriptions::COUNT);

void DucoError::setup() { this->state.reserve(MAX_ERROR_TEXT_LENGTH); }

void DucoError::update() { this->parent_->send(this, 0x30, {this->address_, 0x00, 0x00, 0x04}); }

void DucoError::receive_response(const DucoMessage &message) {
  if (message.size == 0) {
    this->publish_state("No errors");
    return;
  }

  const uint8_t error_count = message.data[0];
  const size_t expected_size = 1 + static_cast<size_t>(error_count) * 5;
  if (error_count == 0 || message.size < expected_size) {
    this->publish_state("No errors");
    return;
  }

  size_t length = 0;
  for (size_t index = 0; index < error_count; index++) {
    const size_t offset = 1 + index * 5;
    char code[13];
    snprintf(code, sizeof(code), "E%02u.%02u.%02u", static_cast<unsigned>(message.data[offset + 1]),
             static_cast<unsigned>(message.data[offset + 2]), static_cast<unsigned>(message.data[offset + 3]));
    const int16_t description_index = progmem_string_table_find<ErrorCodes>(code, strlen(code));
    char description[ErrorDescriptions::MAX_LENGTH + 1];
    ESPHOME_strncpy_P(description,
                      reinterpret_cast<ESPHOME_PGM_P>(
                          ErrorDescriptions::get_progmem_str(description_index, ErrorDescriptions::LAST_INDEX)),
                      sizeof(description));
    description[sizeof(description) - 1] = '\0';
    const int written = snprintf(this->error_text_.data() + length, this->error_text_.size() - length, "%s  %s\n", code,
                                 description);
    if (written < 0 || static_cast<size_t>(written) >= this->error_text_.size() - length)
      break;
    length += written;
  }
  this->publish_state(this->error_text_.data(), length);
}

}  // namespace esphome::duco
