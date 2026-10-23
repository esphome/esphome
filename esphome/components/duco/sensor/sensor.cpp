#include "sensor.h"

#include "esphome/core/hal.h"

namespace esphome::duco {

enum class DucoValueFormat : uint8_t {
  DUCO_VALUE_FORMAT_UINT8,
  DUCO_VALUE_FORMAT_UINT16,
  DUCO_VALUE_FORMAT_INT16,
};

/// A little endian field at offset in the answer. Values outside [minimum, maximum] are dropped;
/// the box reports those for sensors that are missing or still starting.
struct ReadingLayout {
  uint8_t function;
  uint8_t offset;
  DucoValueFormat format;
  uint8_t divisor;
  int16_t minimum;
  uint16_t maximum;
};

// Indexed by DucoReading
static constexpr ReadingLayout READINGS[] PROGMEM = {
    {FUNCTION_NODE_DATA, 4, DucoValueFormat::DUCO_VALUE_FORMAT_UINT16, 1, 300, 10000},  // CO2
    {FUNCTION_NODE_DATA, 2, DucoValueFormat::DUCO_VALUE_FORMAT_UINT16, 10, 0, 1000},    // node temperature
    {FUNCTION_NODE_DATA, 6, DucoValueFormat::DUCO_VALUE_FORMAT_UINT16, 100, 0, 10000},  // humidity
    {FUNCTION_PARAMETER, 3, DucoValueFormat::DUCO_VALUE_FORMAT_UINT8, 1, 0, 255},       // filter remaining
    {FUNCTION_NODE, 2, DucoValueFormat::DUCO_VALUE_FORMAT_UINT8, 1, 0, 255},            // flow level
    {FUNCTION_NODE, 12, DucoValueFormat::DUCO_VALUE_FORMAT_UINT16, 1, 0, 65535},        // time remaining
    {FUNCTION_PARAMETER, 3, DucoValueFormat::DUCO_VALUE_FORMAT_INT16, 10, -999, 1000},  // box temperature
    {FUNCTION_PARAMETER, 3, DucoValueFormat::DUCO_VALUE_FORMAT_UINT8, 1, 0, 100},       // bypass
};

static ReadingLayout layout_of(DucoReading reading) {
  ReadingLayout layout;
  progmem_memcpy(&layout, &READINGS[static_cast<uint8_t>(reading)], sizeof(layout));
  return layout;
}

void DucoSensor::update() {
  switch (layout_of(this->reading_).function) {
    case FUNCTION_NODE_DATA:
      this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->argument_, 0x00, 0x49, 0x04});
      break;
    case FUNCTION_PARAMETER:
      this->parent_->send(this, FUNCTION_PARAMETER, {0x00, this->argument_, 0x09});
      break;
    case FUNCTION_NODE:
      this->parent_->send(this, FUNCTION_NODE, {0x02, this->argument_});
      break;
  }
}

void DucoSensor::receive_response(const DucoMessage &message) {
  const ReadingLayout layout = layout_of(this->reading_);
  const bool wide = layout.format != DucoValueFormat::DUCO_VALUE_FORMAT_UINT8;
  if (message.size < layout.offset + (wide ? 2 : 1))
    return;

  const uint8_t *field = message.data + layout.offset;
  int32_t value = field[0];
  if (layout.format == DucoValueFormat::DUCO_VALUE_FORMAT_UINT16) {
    value = encode_value_le<uint16_t>(field);
  } else if (layout.format == DucoValueFormat::DUCO_VALUE_FORMAT_INT16) {
    value = encode_value_le<int16_t>(field);
  }
  if (value >= layout.minimum && value <= layout.maximum)
    this->publish_state(static_cast<float>(value) / layout.divisor);
}

}  // namespace esphome::duco
