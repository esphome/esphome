#include "mbus-decoder.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mbus {

uint32_t MBusDecoder::decode_bcd_hex(uint8_t bcd_data[4]) {
  uint64_t val = 0;
  size_t i;

  for (i = 4; i > 0; i--) {
    val = (val << 8) | bcd_data[i - 1];
  }

  return val;
}

std::string MBusDecoder::decode_manufacturer(uint8_t data[2]) {
  int16_t m_id = decode_int(data);

  uint8_t m_str[4];
  m_str[0] = (char) (((m_id >> 10) & 0x001F) + 64);
  m_str[1] = (char) (((m_id >> 5) & 0x001F) + 64);
  m_str[2] = (char) (((m_id) &0x001F) + 64);
  m_str[3] = 0;

  std::string manufacturer((char *) m_str);
  return manufacturer;
}

int16_t MBusDecoder::decode_int(uint8_t data[2]) {
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

std::string MBusDecoder::decode_medium(uint8_t data) {
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
  // Medium Code bin Code hex
  // Other 0000 0000 00
  // Oil 0000 0001 01
  // Electricity 0000 0010 02
  // Gas 0000 0011 03
  // Heat (Volume measured at return temperature: outlet) 0000 0100 04
  // Steam 0000 0101 05
  // Hot Water 0000 0110 06
  // Water 0000 0111 07
  // Heat Cost Allocator. 0000 1000 08
  // Compressed Air 0000 1001 09
  // Cooling load meter (Volume measured at return temperature: outlet) 0000 1010 0A
  // Cooling load meter (Volume measured at flow temperature: inlet) ♣ 0000 1011 0B
  // Heat (Volume measured at flow temperature: inlet) 0000 1100 0C
  // Heat / Cooling load meter ♣ 0000 1101 OD
  // Bus / System 0000 1110 0E
  // Unknown Medium 0000 1111 0F
  // Irrigation Water (Non Drinkable) 0001 0000 10
  // Water data logger 0001 0001 11
  // Gas data logger 0001 0010 12
  // Gas converter 0001 0011 13
  // Heat Value 0001 0100 14
  // Hot Water (>=90°C) (Non Drinkable) 0001 0101 15
  // Cold Water 0001 0110 16
  // Dual Water 0001 0111 17
  // Pressure 0001 1000 18
  // A/D Converter 0001 1001 19
  // Smoke detector 0001 1010 1A
  // Room sensor (e.g. Temperature or Humidity) 0001 1011 1B
  // Gas detector 0001 1100 1C
  // Reserved for Sensors .......... 1D to 1F
  // Breaker (Electricity) 0010 0000 20
  // Valve (Gas or Water) 0010 0001 21
  // Reserved for Switching Units .......... 22 to 24
  // Customer Unit (Display) 0010 0101 25
  // Reserved for End User Units .......... 26 to 27
  // Waste Water (Non Drinkable) 0010 1000 28
  // Waste 0010 1001 29
  // Reserved for CO2 0010 1010 2A
  // Reserved for environmental meter .......... 2B to 2F
  // Service tool 0011 0000 30
  // Gateway 0011 0001 31
  // Unidirectional Repeater 0011 0010 32
  // Bidirectional Repeater 0011 0011 33
  // Reserved for System Units .......... 34 to 35
  // Radio Control Unit (System Side) 0011 0110 36
  // Radio Control Unit (Meter Side) 0011 0111 37
  // Bus Control Unit (Meter Side) 0011 1000 38
  // Reserved for System Units .......... 38 to 3F
  // Reserved .......... 40 to FE
  // Placeholder 1111 1111 FF
}

}  // namespace mbus
}  // namespace esphome
