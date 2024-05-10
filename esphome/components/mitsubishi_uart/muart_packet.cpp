#include "muart_packet.h"

namespace esphome {
namespace mitsubishi_uart {

// Creates an empty packet
Packet::Packet() {
  // TODO: Is this okay?
}

// std::string Packet::to_string() const {
//   return format_hex_pretty(&pkt_.getBytes()[0], pkt_.getLength());
// }

static char format_hex_pretty_char(uint8_t v) { return v >= 10 ? 'A' + (v - 10) : '0' + v; }

std::string Packet::to_string() const {
  // Based on `format_hex_pretty` from ESPHome
  if (pkt_.getLength() < PACKET_HEADER_SIZE)
    return "";
  std::stringstream stream;

  stream << CONSOLE_COLOR_CYAN;  // Cyan
  stream << '[';

  for (size_t i = 0; i < PACKET_HEADER_SIZE; i++) {
    if (i == 1) {
      stream << CONSOLE_COLOR_CYAN_BOLD;
    }
    stream << format_hex_pretty_char((pkt_.getBytes()[i] & 0xF0) >> 4);
    stream << format_hex_pretty_char(pkt_.getBytes()[i] & 0x0F);
    if (i < PACKET_HEADER_SIZE - 1) {
      stream << '.';
    }
    if (i == 1) {
      stream << CONSOLE_COLOR_CYAN;
    }
  }
  // Header close-bracket
  stream << ']';
  stream << CONSOLE_COLOR_WHITE;  // White

  // Payload
  for (size_t i = PACKET_HEADER_SIZE; i < pkt_.getLength() - 1; i++) {
    stream << format_hex_pretty_char((pkt_.getBytes()[i] & 0xF0) >> 4);
    stream << format_hex_pretty_char(pkt_.getBytes()[i] & 0x0F);
    if (i < pkt_.getLength() - 2) {
      stream << '.';
    }
  }

  // Space
  stream << ' ';
  stream << CONSOLE_COLOR_GREEN;  // Green

  // Checksum
  stream << format_hex_pretty_char((pkt_.getBytes()[pkt_.getLength() - 1] & 0xF0) >> 4);
  stream << format_hex_pretty_char(pkt_.getBytes()[pkt_.getLength() - 1] & 0x0F);

  stream << CONSOLE_COLOR_NONE;  // Reset

  return stream.str();
}

void Packet::setFlags(const uint8_t flagValue) { pkt_.setPayloadByte(PLINDEX_FLAGS, flagValue); }

// Adds a flag (ONLY APPLICABLE FOR SOME COMMANDS)
void Packet::addFlag(const uint8_t flagToAdd) {
  pkt_.setPayloadByte(PLINDEX_FLAGS, pkt_.getPayloadByte(PLINDEX_FLAGS) | flagToAdd);
}
// Adds a flag2 (ONLY APPLICABLE FOR SOME COMMANDS)
void Packet::addFlag2(const uint8_t flag2ToAdd) {
  pkt_.setPayloadByte(PLINDEX_FLAGS2, pkt_.getPayloadByte(PLINDEX_FLAGS2) | flag2ToAdd);
}

}  // namespace mitsubishi_uart
}  // namespace esphome
