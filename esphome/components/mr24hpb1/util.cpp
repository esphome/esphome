#include "mr24hpb1.h"
#include <sstream>

namespace esphome {
namespace mr24hpb1 {
FunctionCode get_packet_function_code(std::vector<uint8_t> &packet) { return packet.at(3); }

AddressCode1 get_packet_address_code_1(std::vector<uint8_t> &packet) { return packet.at(4); }

AddressCode2 get_packet_address_code_2(std::vector<uint8_t> &packet) { return packet.at(5); }

uint16_t get_packet_length(std::vector<uint8_t> &packet) {
  uint16_t length_low = packet.at(1);
  uint16_t length_high = packet.at(2);

  return (length_high << 8 | length_low) + 1;  // include PACKET_START in length
}

uint16_t get_packet_crc(std::vector<uint8_t> &packet) {
  uint16_t packet_size = packet.size();
  uint16_t crc16_high = packet.at(packet_size - 1);
  uint16_t crc16_low = packet.at(packet_size - 2);
  return crc16_low << 8 | crc16_high;
}

uint32_t packet_data_to_int(std::vector<uint8_t> &packet) {
  uint32_t converted_data = 0x00;
  std::vector<uint8_t> data = get_packet_data(packet);
  for (uint8_t &byte : data) {
    converted_data <<= 8;
    converted_data |= byte;
  }
  return converted_data;
}

std::vector<uint8_t> get_packet_data(std::vector<uint8_t> &packet) {
  return std::vector<uint8_t>(packet.begin() + 6, packet.end() - 2);
}

std::string packet_data_to_string(std::vector<uint8_t> &packet) {
  std::stringstream ss;
  std::vector<uint8_t> data = get_packet_data(packet);
  for (uint8_t &byte : data) {
    ss << byte;
  }
  return ss.str();
}

typedef union {
  uint32_t Byte;
  float Float;
} Float_Byte;

float packet_data_to_float(std::vector<uint8_t> &packet) {
  Float_Byte converted_data;
  converted_data.Byte = 0x00;
  std::vector<uint8_t> data = get_packet_data(packet);
  for (auto i = data.rbegin(); i != data.rend(); ++i)
  {
    converted_data.Byte <<= 8;
    converted_data.Byte |= *i;
  }
  return converted_data.Float;
}

const char *SceneSetting_to_string(SceneSetting setting) {
  switch (setting) {
    case SCENE_DEFAULT:
      return "DEFAULT";
    case AREA:
      return "AREA";
    case BATHROOM:
      return "BATHROOM";
    case BEDROOM:
      return "BEDROOM";
    case LIVING_ROOM:
      return "LIVING_ROOM";
    case OFFICE:
      return "OFFICE";
    case HOTEL:
      return "HOTEL";
    default:
      return "UNDEFINED";
  }
}

const char *EnvironmentStatus_to_string(EnvironmentStatus status) {
  switch (status) {
    case UNOCCUPIED:
      return "UNOCCUPIED";
    case STATIONARY:
      return "STATIONARY";
    case MOVING:
      return "MOVING";
    default:
      return "UNDEFINED";
  }
}
}  // namespace mr24hpb1
}  // namespace esphome