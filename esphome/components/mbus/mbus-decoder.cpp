#include "mbus-decoder.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mbus {

int64_t MBusDecoder::decode_int(const std::vector<uint8_t> &data) {
  auto size = data.size();

  size_t i;
  uint8_t neg;
  int64_t value = 0;

  if (size == 0) {
    return 0;
  }

  neg = data[size - 1] & 0x80;

  for (i = size; i > 0; i--) {
    if (neg) {
      value = (value << 8) + (data[i - 1] ^ 0xFF);
    } else {
      value = (value << 8) + data[i - 1];
    }
  }

  if (neg) {
    value = (value * -1) - 1;
  }

  return value;
}

uint64_t MBusDecoder::decode_bcd_int(const std::vector<uint8_t> &data) {
  auto size = data.size();

  uint64_t value = 0;
  size_t i;

  for (i = size; i > 0; i--) {
    value = (value * 10);

    if (data[i - 1] >> 4 < 0xA) {
      value += ((data[i - 1] >> 4) & 0xF);
    }

    value = (value * 10) + (data[i - 1] & 0xF);
  }

  // hex code Fh in the MSD position signals a negative BCD number
  if (data[size - 1] >> 4 == 0xF) {
    value *= -1;
  }

  return value;
}

uint32_t MBusDecoder::decode_bcd_uint32(const uint8_t bcd_data[4]) {
  uint64_t val = 0;
  size_t i;

  for (i = 4; i > 0; i--) {
    val = (val << 8) | bcd_data[i - 1];
  }

  return val;
}

std::string MBusDecoder::decode_manufacturer(const uint8_t data[2]) {
  int16_t m_id = decode_int16(data);

  uint8_t m_str[4];
  m_str[0] = (char) (((m_id >> 10) & 0x001F) + 64);
  m_str[1] = (char) (((m_id >> 5) & 0x001F) + 64);
  m_str[2] = (char) (((m_id) &0x001F) + 64);
  m_str[3] = 0;

  std::string manufacturer((char *) m_str);
  return manufacturer;
}

int16_t MBusDecoder::decode_int16(const uint8_t data[2]) {
  int neg;
  uint16_t value = 0;

  neg = data[1] & 0x80;

  for (auto i = 2; i > 0; i--) {
    if (neg) {
      value = (value << 8) + (data[i - 1] ^ 0xFF);
    } else {
      value = (value << 8) + data[i - 1];
    }
  }

  if (neg) {
    value = value * -1 - 1;
  }

  return value;
}

std::string MBusDecoder::decode_medium(const uint8_t data) {
  switch (data) {
    case 0x00:
      return "Other";
    case 0x01:
      return "Oil";
    case 0x02:
      return "Electricity";
    case 0x03:
      return "Gas";
    case 0x04:
      return "Heat (Volume measured at return temperature: outlet)";
    case 0x05:
      return "Steam";
    case 0x06:
      return "Hot Water";
    case 0x07:
      return "Water";
    case 0x08:
      return "Heat Cost Allocator.";
    case 0x09:
      return "Compressed Air";
    case 0x0A:
      return "Cooling load meter (Volume measured at return temperature: outlet)";
    case 0x0B:
      return "Cooling load meter (Volume measured at flow temperature: inlet) ♣";
    case 0x0C:
      return "Heat (Volume measured at flow temperature: inlet)";
    case 0x0D:
      return "Heat / Cooling load meter ♣";
    case 0x0E:
      return "Bus / System";
    case 0x0F:
      return "Unknown Medium";
    case 0x10:
      return "Irrigation Water (Non Drinkable)";
    case 0x11:
      return "Water data logger";
    case 0x12:
      return "Gas data logger";
    case 0x13:
      return "Gas converter";
    case 0x14:
      return "Heat Value";
    case 0x15:
      return "Hot Water (>=90°C) (Non Drinkable)";
    case 0x16:
      return "Cold Water";
    case 0x17:
      return "Dual Water";
    case 0x18:
      return "Pressure";
    case 0x19:
      return "A/D Converter";
    case 0x1A:
      return "Smoke detector";
    case 0x1B:
      return "Room sensor (e.g. Temperature or Humidity)";
    case 0x1C:
      return "Gas detector";
    case 0x1F:
      return "Reserved for Sensors";
    case 0x20:
      return "Breaker (Electricity)";
    case 0x21:
      return "Valve (Gas or Water)";
    case 0x24:
      return "Reserved for Switching Units";
    case 0x25:
      return "Customer Unit (Display)";
    case 0x27:
      return "Reserved for End User Units";
    case 0x28:
      return "Waste Water (Non Drinkable)";
    case 0x29:
      return "Waste";
    case 0x2A:
      return "Reserved for CO2";
    case 0x2F:
      return "Reserved for environmental meter";
    case 0x30:
      return "Service tool";
    case 0x31:
      return "Gateway";
    case 0x32:
      return "Unidirectional Repeater";
    case 0x33:
      return "Bidirectional Repeater";
    case 0x35:
      return "Reserved for System Units";
    case 0x36:
      return "Radio Control Unit (System Side)";
    case 0x37:
      return "Radio Control Unit (Meter Side)";
    case 0x38:
      return "Bus Control Unit (Meter Side)";
    case 0x3F:
      return "Reserved for System Units";
    case 0xFE:
      return "Reserved";
    case 0xFF:
      return "Placeholder";
  }

  return "unknown medium";
}

uint64_t MBusDecoder::decode_secondary_address(const uint8_t id[4], const uint8_t manufacturer[2],
                                               const uint8_t version, const uint8_t medium) {
  uint64_t secondary_address = id[3];
  secondary_address = (secondary_address << 8) + id[2];
  secondary_address = (secondary_address << 8) + id[1];
  secondary_address = (secondary_address << 8) + id[0];
  secondary_address = (secondary_address << 8) + manufacturer[0];
  secondary_address = (secondary_address << 8) + manufacturer[1];
  secondary_address = (secondary_address << 8) + version;
  secondary_address = (secondary_address << 8) + medium;

  return secondary_address;
}

void MBusDecoder::encode_secondary_address(const uint64_t secondary_address, std::vector<uint8_t> *data) {
  uint8_t buffer[8];
  // Secondary Address
  // ID[4]
  buffer[3] = (secondary_address >> 0x38) & 0xFF;
  buffer[2] = (secondary_address >> 0x30) & 0xFF;
  buffer[1] = (secondary_address >> 0x28) & 0xFF;
  buffer[0] = (secondary_address >> 0x20) & 0xFF;

  // manufacturer[2]
  buffer[4] = (secondary_address >> 0x18) & 0xFF;
  buffer[5] = (secondary_address >> 0x10) & 0xFF;
  // version[1]
  buffer[6] = (secondary_address >> 0x08) & 0xFF;
  // medium[1]
  buffer[7] = secondary_address & 0xFF;

  data->clear();
  auto buffer_size = sizeof(buffer);
  data->reserve(buffer_size);
  data->insert(data->begin(), buffer, buffer + buffer_size);
}
}  // namespace mbus
}  // namespace esphome
