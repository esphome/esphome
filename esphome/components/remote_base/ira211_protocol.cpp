#include "ira211_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.ira211");

static constexpr uint32_t CARRIER_FREQUENCY = 38000;
static constexpr uint32_t T_US = 800;
static constexpr uint32_t HEADER_US = 19 * T_US / 2;  // 7600 us, also the end-of-frame gap

static constexpr uint8_t DEVICE_ID = 0x66;
static constexpr uint8_t BITS_PER_PACKET = 10;
static constexpr uint8_t MIN_PACKETS = 4;
static constexpr uint8_t MAX_PACKETS = 7;
static constexpr uint8_t MAX_BITS = MAX_PACKETS * BITS_PER_PACKET;

// Fields each command carries, in wire order after the command byte: temperature (whole degrees
// then tenths), mode, fan.
static constexpr uint8_t FIELD_TEMPERATURE = 1 << 0;
static constexpr uint8_t FIELD_MODE = 1 << 1;
static constexpr uint8_t FIELD_FAN = 1 << 2;

struct CommandLayout {
  IRA211Command command;
  uint8_t fields;
};

static constexpr CommandLayout LAYOUTS[] = {
    {IRA211Command::IRA211_COMMAND_TEMP_UP, FIELD_TEMPERATURE},
    {IRA211Command::IRA211_COMMAND_TEMP_DOWN, FIELD_TEMPERATURE},
    {IRA211Command::IRA211_COMMAND_MODE, FIELD_MODE},
    {IRA211Command::IRA211_COMMAND_FAN, FIELD_FAN},
    {IRA211Command::IRA211_COMMAND_POWER, FIELD_TEMPERATURE | FIELD_MODE | FIELD_FAN},
    {IRA211Command::IRA211_COMMAND_SYNC, FIELD_TEMPERATURE | FIELD_MODE | FIELD_FAN},
};

static const CommandLayout *find_layout(IRA211Command command) {
  for (const auto &layout : LAYOUTS) {
    if (layout.command == command)
      return &layout;
  }
  return nullptr;
}

// Device ID, command, fields, checksum
static uint8_t packet_count(const CommandLayout &layout) {
  return 3 + ((layout.fields & FIELD_TEMPERATURE) ? 2 : 0) + ((layout.fields & FIELD_MODE) ? 1 : 0) +
         ((layout.fields & FIELD_FAN) ? 1 : 0);
}

// Value to wire byte and back; the transform is its own inverse
static uint8_t wire_xform(uint8_t value) { return reverse_bits(static_cast<uint8_t>(~value)); }

static uint8_t checksum(const uint8_t *bytes, uint8_t count) {
  uint8_t sum = count;
  for (uint8_t i = 0; i < count; i++)
    sum += reverse_bits(bytes[i]);
  return reverse_bits(sum);
}

bool IRA211Data::operator==(const IRA211Data &rhs) const {
  if (this->command != rhs.command)
    return false;
  const CommandLayout *layout = find_layout(this->command);
  if (layout == nullptr)
    return true;
  if ((layout->fields & FIELD_TEMPERATURE) && this->half_degrees != rhs.half_degrees)
    return false;
  if ((layout->fields & FIELD_MODE) && this->mode != rhs.mode)
    return false;
  return !(layout->fields & FIELD_FAN) || this->fan == rhs.fan;
}

void IRA211Protocol::encode(RemoteTransmitData *dst, const IRA211Data &data) {
  const CommandLayout *layout = find_layout(data.command);
  if (layout == nullptr)
    return;

  uint8_t bytes[MAX_PACKETS];
  uint8_t count = 0;
  bytes[count++] = DEVICE_ID;
  bytes[count++] = wire_xform(static_cast<uint8_t>(data.command));
  if (layout->fields & FIELD_TEMPERATURE) {
    bytes[count++] = wire_xform(data.half_degrees / 2);
    bytes[count++] = wire_xform((data.half_degrees & 1) ? 5 : 0);
  }
  if (layout->fields & FIELD_MODE)
    bytes[count++] = wire_xform(static_cast<uint8_t>(data.mode));
  if (layout->fields & FIELD_FAN)
    bytes[count++] = wire_xform(static_cast<uint8_t>(data.fan));
  bytes[count] = checksum(bytes, count);
  count++;

  dst->set_carrier_frequency(CARRIER_FREQUENCY);
  // Preamble, at most one item per bit, trailing mark
  dst->reserve(4 + count * BITS_PER_PACKET + 1);
  dst->item(HEADER_US, T_US);
  dst->item(T_US, HEADER_US);

  // Each packet is a 1 bit, the byte MSB first, then a 0 bit; runs of equal bits become one mark or space
  bool level = true;
  uint32_t run = 0;
  for (uint8_t p = 0; p < count; p++) {
    const uint16_t packet = (1 << 9) | (bytes[p] << 1);
    for (int8_t i = BITS_PER_PACKET - 1; i >= 0; i--) {
      const bool bit = (packet >> i) & 1;
      if (bit != level && run != 0) {
        if (level) {
          dst->mark(run * T_US);
        } else {
          dst->space(run * T_US);
        }
        run = 0;
      }
      level = bit;
      run++;
    }
  }
  // Every packet ends with a 0 bit, so the frame ends with a space, closed by a trailing mark
  dst->space(run * T_US);
  dst->mark(T_US);
}

optional<IRA211Data> IRA211Protocol::decode(RemoteReceiveData src) {
  if (!src.expect_item(HEADER_US, T_US) || !src.expect_item(T_US, HEADER_US))
    return {};

  // Read bits until the end-of-frame gap, checking packet framing and collecting bytes as they arrive
  uint8_t bytes[MAX_PACKETS]{};
  uint8_t bits = 0;
  while (src.is_valid()) {
    const int32_t raw = src.peek();
    const bool is_mark = raw > 0;
    const uint32_t duration = is_mark ? raw : -raw;
    // A mark followed by the gap or the end of the data is the trailing mark, not a data bit
    if (is_mark && (!src.is_valid(1) || src.peek(1) <= -static_cast<int32_t>(HEADER_US)))
      break;
    if (!is_mark && duration >= HEADER_US)
      break;
    const uint32_t run = (duration + T_US / 2) / T_US;
    if (run == 0 || bits + run > MAX_BITS)
      return {};
    for (uint32_t i = 0; i < run; i++, bits++) {
      const uint8_t pos = bits % BITS_PER_PACKET;
      if ((pos == 0 && !is_mark) || (pos == BITS_PER_PACKET - 1 && is_mark))
        return {};
      if (pos != 0 && pos != BITS_PER_PACKET - 1) {
        uint8_t &byte = bytes[bits / BITS_PER_PACKET];
        byte = (byte << 1) | (is_mark ? 1 : 0);
      }
    }
    src.advance();
  }

  const uint8_t count = bits / BITS_PER_PACKET;
  if (bits % BITS_PER_PACKET != 0 || count < MIN_PACKETS || bytes[0] != DEVICE_ID)
    return {};
  IRA211Data out{};
  out.command = static_cast<IRA211Command>(wire_xform(bytes[1]));
  const CommandLayout *layout = find_layout(out.command);
  if (layout == nullptr || packet_count(*layout) != count || checksum(bytes, count - 1) != bytes[count - 1]) {
    ESP_LOGV(TAG, "Invalid frame (%u bits)", bits);
    return {};
  }

  uint8_t index = 2;
  if (layout->fields & FIELD_TEMPERATURE) {
    const uint8_t whole = wire_xform(bytes[index++]);
    const uint8_t tenths = wire_xform(bytes[index++]);
    out.half_degrees = whole * 2 + (tenths >= 5 ? 1 : 0);
  }
  if (layout->fields & FIELD_MODE)
    out.mode = static_cast<IRA211Mode>(wire_xform(bytes[index++]));
  if (layout->fields & FIELD_FAN)
    out.fan = static_cast<IRA211Fan>(wire_xform(bytes[index++]));
  return out;
}

static const LogString *command_to_string(IRA211Command command) {
  switch (command) {
    case IRA211Command::IRA211_COMMAND_TEMP_UP:
      return LOG_STR("TEMP_UP");
    case IRA211Command::IRA211_COMMAND_TEMP_DOWN:
      return LOG_STR("TEMP_DOWN");
    case IRA211Command::IRA211_COMMAND_MODE:
      return LOG_STR("MODE");
    case IRA211Command::IRA211_COMMAND_FAN:
      return LOG_STR("FAN");
    case IRA211Command::IRA211_COMMAND_POWER:
      return LOG_STR("POWER");
    case IRA211Command::IRA211_COMMAND_SYNC:
      return LOG_STR("SYNC");
    default:
      return LOG_STR("UNKNOWN");
  }
}

static const LogString *mode_to_string(IRA211Mode mode) {
  switch (mode) {
    case IRA211Mode::IRA211_MODE_PROTECTION:
      return LOG_STR("Protection");
    case IRA211Mode::IRA211_MODE_TIMER:
      return LOG_STR("Timer");
    case IRA211Mode::IRA211_MODE_COMFORT:
      return LOG_STR("Comfort");
    default:
      return LOG_STR("Unknown");
  }
}

static const LogString *fan_to_string(IRA211Fan fan) {
  switch (fan) {
    case IRA211Fan::IRA211_FAN_AUTO:
      return LOG_STR("Auto");
    case IRA211Fan::IRA211_FAN_LOW:
      return LOG_STR("1/3");
    case IRA211Fan::IRA211_FAN_MEDIUM:
      return LOG_STR("2/3");
    case IRA211Fan::IRA211_FAN_HIGH:
      return LOG_STR("3/3");
    default:
      return LOG_STR("Unknown");
  }
}

void IRA211Protocol::dump(const IRA211Data &data) {
  const CommandLayout *layout = find_layout(data.command);
  const uint8_t fields = layout == nullptr ? 0 : layout->fields;
  const unsigned whole = data.half_degrees / 2;
  const unsigned tenths = (data.half_degrees & 1) ? 5 : 0;
  const auto *command = LOG_STR_ARG(command_to_string(data.command));
  if (fields == (FIELD_TEMPERATURE | FIELD_MODE | FIELD_FAN)) {
    ESP_LOGI(TAG, "Received IRA211: command=%s, temperature=%u.%uC, mode=%s, fan=%s", command, whole, tenths,
             LOG_STR_ARG(mode_to_string(data.mode)), LOG_STR_ARG(fan_to_string(data.fan)));
  } else if (fields == FIELD_TEMPERATURE) {
    ESP_LOGI(TAG, "Received IRA211: command=%s, temperature=%u.%uC", command, whole, tenths);
  } else if (fields == FIELD_MODE) {
    ESP_LOGI(TAG, "Received IRA211: command=%s, mode=%s", command, LOG_STR_ARG(mode_to_string(data.mode)));
  } else if (fields == FIELD_FAN) {
    ESP_LOGI(TAG, "Received IRA211: command=%s, fan=%s", command, LOG_STR_ARG(fan_to_string(data.fan)));
  } else {
    ESP_LOGI(TAG, "Received IRA211: command=%s", command);
  }
}

}  // namespace esphome::remote_base
