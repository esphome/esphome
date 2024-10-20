#include "mitp_packet.h"

namespace esphome {
namespace mitsubishi_itp {

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
  if (pkt_.get_length() < PACKET_HEADER_SIZE)
    return "";
  std::stringstream stream;

  stream << CONSOLE_COLOR_CYAN;  // Cyan
  stream << '[';

  for (size_t i = 0; i < PACKET_HEADER_SIZE; i++) {
    if (i == 1) {
      stream << CONSOLE_COLOR_CYAN_BOLD;
    }
    stream << format_hex_pretty_char((pkt_.get_bytes()[i] & 0xF0) >> 4);
    stream << format_hex_pretty_char(pkt_.get_bytes()[i] & 0x0F);
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
  for (size_t i = PACKET_HEADER_SIZE; i < pkt_.get_length() - 1; i++) {
    stream << format_hex_pretty_char((pkt_.get_bytes()[i] & 0xF0) >> 4);
    stream << format_hex_pretty_char(pkt_.get_bytes()[i] & 0x0F);
    if (i < pkt_.get_length() - 2) {
      stream << '.';
    }
  }

  // Space
  stream << ' ';
  stream << CONSOLE_COLOR_GREEN;  // Green

  // Checksum
  stream << format_hex_pretty_char((pkt_.get_bytes()[pkt_.get_length() - 1] & 0xF0) >> 4);
  stream << format_hex_pretty_char(pkt_.get_bytes()[pkt_.get_length() - 1] & 0x0F);

  stream << CONSOLE_COLOR_NONE;  // Reset

  return stream.str();
}

void Packet::set_flags(const uint8_t flag_value) { pkt_.set_payload_byte(PLINDEX_FLAGS, flag_value); }

// Adds a flag (ONLY APPLICABLE FOR SOME COMMANDS)
void Packet::add_flag(const uint8_t flag_to_add) {
  pkt_.set_payload_byte(PLINDEX_FLAGS, pkt_.get_payload_byte(PLINDEX_FLAGS) | flag_to_add);
}
// Adds a flag2 (ONLY APPLICABLE FOR SOME COMMANDS)
void Packet::add_flag2(const uint8_t flag2_to_add) {
  pkt_.set_payload_byte(PLINDEX_FLAGS2, pkt_.get_payload_byte(PLINDEX_FLAGS2) | flag2_to_add);
}

}  // namespace mitsubishi_itp
}  // namespace esphome
