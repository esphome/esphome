#include "text_sensor.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::duco {

#ifdef USE_DUCO_ERROR_TEXT_SENSOR
// Only built when an error text sensor is configured; the tables are about 3 kB of flash
ESPHOME_LOG_TAG(TAG, "duco.text_sensor");
static constexpr size_t MAX_ERROR_TEXT_LENGTH = 255;

PROGMEM_STRING_TABLE(ErrorCodes, "E01.04.01", "E03.01.01", "E03.01.02", "E03.01.03", "E03.01.04", "E03.01.05",
                     "E03.01.06", "E04.01.01", "E04.01.02", "E04.01.03", "E04.01.04", "E04.01.05", "E04.01.06",
                     "E06.01.01", "E06.01.02", "E06.01.03", "E06.01.04", "E06.01.05", "E06.01.06", "E16.01.01",
                     "E16.01.02", "E16.01.03", "E16.01.04", "E16.01.05", "E16.01.06", "E28.04.01", "E29.01.01",
                     "E29.01.02", "E29.01.03", "E29.01.04", "E29.01.05", "E29.01.06", "E30.01.01", "E30.01.02",
                     "E30.01.03", "E30.01.04", "E30.01.05", "E30.01.06", "E31.19.01", "E32.10.01", "E33.10.01",
                     "E35.17.01", "E35.17.02", "E35.17.03", "E36.24.01", "E36.24.02", "E37.17.01", "E37.17.02",
                     "E37.17.03", "E40.17.01", "E40.17.02", "E40.17.03", "E42.22.01", "E42.22.02", "E43.23.01",
                     "E43.23.02", "E44.06.01", "E44.06.02");
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
    "Filters need replacement.", "Outdoor temperature is too low; ventilation is off for 8 hours.",
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

// One line per error, with its description or only its code. Returns the text length, or 0 if it does not fit
// in MAX_ERROR_TEXT_LENGTH; text holds one byte more than that so a cut off text is told apart from a full one.
static size_t format_errors(const uint8_t *records, uint8_t count, char *text, size_t size, bool descriptions) {
  size_t pos = 0;
  for (uint8_t index = 0; index < count; index++) {
    const uint8_t *record = records + index * 5;
    const size_t code_start = pos;
    pos = buf_append_printf(text, size, pos, "E%02u.%02u.%02u", record[1], record[2], record[3]);
    if (descriptions && pos < size) {
      const int16_t description_index = progmem_string_table_find<ErrorCodes>(text + code_start, pos - code_start);
      pos = buf_append_str(text, size, pos, "  ");
      pos = buf_append_str_p(text, size, pos,
                             reinterpret_cast<ESPHOME_PGM_P>(
                                 ErrorDescriptions::get_progmem_str(description_index, ErrorDescriptions::LAST_INDEX)));
    }
    pos = buf_append_str(text, size, pos, "\n");
  }
  return pos <= MAX_ERROR_TEXT_LENGTH ? pos : 0;
}
#endif

void DucoTextSensor::update() {
  switch (this->reading_) {
    case DucoTextReading::DUCO_TEXT_READING_SERIAL_NUMBER:
      // Address 0 selects the separate box serial; nodes use a different data block (main board is node 1)
      if (this->address_ == 0) {
        this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, 0x01, 0x00, 0x1a, 0x10});
      } else {
        this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->address_, 0x00, 0x08, 0x06});
      }
      break;
    case DucoTextReading::DUCO_TEXT_READING_VERSION:
      this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->address_, 0x00, 0x00, 0x04});
      break;
    case DucoTextReading::DUCO_TEXT_READING_ERROR:
#ifdef USE_DUCO_ERROR_TEXT_SENSOR
      this->parent_->send(this, FUNCTION_ERRORS, {this->address_, 0x00, 0x00, 0x04});
#endif
      break;
    case DucoTextReading::DUCO_TEXT_READING_UPTIME:
      this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->address_, 0x00, 0x49, 0x04});
      break;
    case DucoTextReading::DUCO_TEXT_READING_NODE_TYPE:
      this->parent_->send(this, FUNCTION_NODE, {0x01, this->address_});
      break;
  }
}

void DucoTextSensor::receive_response(const DucoMessage &message) {
  switch (this->reading_) {
    case DucoTextReading::DUCO_TEXT_READING_SERIAL_NUMBER:
      this->publish_serial_number_(message);
      break;
    case DucoTextReading::DUCO_TEXT_READING_VERSION:
      this->publish_version_(message);
      break;
    case DucoTextReading::DUCO_TEXT_READING_ERROR:
#ifdef USE_DUCO_ERROR_TEXT_SENSOR
      this->publish_errors_(message);
#endif
      break;
    case DucoTextReading::DUCO_TEXT_READING_UPTIME:
      this->publish_uptime_(message);
      break;
    case DucoTextReading::DUCO_TEXT_READING_NODE_TYPE:
      if (message.size >= 1)
        this->publish_state(reinterpret_cast<ProgmemStr>(node_type_name(node_type_byte(message))));
      break;
  }
}

void DucoTextSensor::publish_serial_number_(const DucoMessage &message) {
  if (message.size <= 2)
    return;
  // The serial number starts at byte 2 and is padded with zeros
  const char *serial = reinterpret_cast<const char *>(message.data + 2);
  const auto serial_length = strnlen(serial, message.size - 2);
  if (serial_length == 0 || !std::all_of(serial, serial + serial_length, [](char character) {
        return std::isprint(static_cast<unsigned char>(character));
      })) {
    return;
  }
  this->publish_state(serial, serial_length);
}

void DucoTextSensor::publish_version_(const DucoMessage &message) {
  if (message.size < 7)
    return;
  const unsigned major = encode_uint16(message.data[3], message.data[2]);
  const unsigned minor = message.data[4], patch = message.data[6], build = message.data[5];
  char version[20];
  snprintf(version, sizeof(version), "%u.%u.%u.%u", major, minor, patch, build);
  this->publish_state(version);
}

#ifdef USE_DUCO_ERROR_TEXT_SENSOR
void DucoTextSensor::publish_errors_(const DucoMessage &message) {
  if (message.size == 0 || message.data[0] == 0) {
    this->publish_state("No errors");
    return;
  }
  const uint8_t error_count = message.data[0];
  if (message.size < 1 + static_cast<size_t>(error_count) * 5) {
    ESP_LOGW(TAG, "Error answer for %u errors has only %u bytes", error_count, message.size);
    return;
  }

  char text[MAX_ERROR_TEXT_LENGTH + 2];
  size_t length = format_errors(message.data + 1, error_count, text, sizeof(text), true);
  if (length == 0)  // the descriptions do not fit, the codes always do
    length = format_errors(message.data + 1, error_count, text, sizeof(text), false);
  this->publish_state(text, length);
}
#endif

void DucoTextSensor::publish_uptime_(const DucoMessage &message) {
  if (message.size < 10)
    return;
  const unsigned uptime_minutes = encode_uint16(message.data[9], message.data[8]);
  const unsigned days = uptime_minutes / 1440;
  const unsigned hours = uptime_minutes % 1440 / 60;
  const unsigned minutes = uptime_minutes % 60;
  char formatted[24];
  snprintf(formatted, sizeof(formatted), "%ud %uh %um", days, hours, minutes);
  this->publish_state(formatted);
}

}  // namespace esphome::duco
