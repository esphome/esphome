#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include <type_traits>

namespace esphome {
namespace mitsubishi_itp {

static constexpr char PTAG[] = "mitsubishi_itp.packets";

const uint8_t BYTE_CONTROL = 0xfc;
const uint8_t PACKET_MAX_SIZE = 22;  // Used to intialize empty packet
const uint8_t PACKET_HEADER_SIZE = 5;
const uint8_t PACKET_HEADER_INDEX_PACKET_TYPE = 1;
const uint8_t PACKET_HEADER_INDEX_PAYLOAD_LENGTH = 4;

// TODO: Figure out something here so we don't have to static_cast<uint8_t> as much
enum class PacketType : uint8_t {
  CONNECT_REQUEST = 0x5a,
  CONNECT_RESPONSE = 0x7a,
  GET_REQUEST = 0x42,
  GET_RESPONSE = 0x62,
  SET_REQUEST = 0x41,
  SET_RESPONSE = 0x61,
  IDENTIFY_REQUEST = 0x5b,
  IDENTIFY_RESPONSE = 0x7b
};

// Used to specify certain packet subtypes
enum class GetCommand : uint8_t {
  SETTINGS = 0x02,
  CURRENT_TEMP = 0x03,
  ERROR_INFO = 0x04,
  STATUS = 0x06,
  RUN_STATE = 0x09,
  THERMOSTAT_STATE_DOWNLOAD = 0xa9,
  THERMOSTAT_GET_AB = 0xab,
};

// Used to specify certain packet subtypes
enum class SetCommand : uint8_t {
  SETTINGS = 0x01,
  REMOTE_TEMPERATURE = 0x07,
  RUN_STATE = 0x08,
  THERMOSTAT_SENSOR_STATUS = 0xa6,
  THERMOSTAT_HELLO = 0xa7,
  THERMOSTAT_STATE_UPLOAD = 0xa8,
  THERMOSTAT_SET_AA = 0xaa,
};

// Which MUARTBridge was the packet read from (used to determine flow direction of the packet)
enum class SourceBridge { NONE, HEATPUMP, THERMOSTAT };

// Specifies which controller the packet "belongs" to (i.e. which controler created it either directly or via a request
// packet)
enum class ControllerAssociation { MUART, THERMOSTAT };

static const uint8_t EMPTY_PACKET[PACKET_MAX_SIZE] = {BYTE_CONTROL,        // Sync
                                                      0x00,                // Packet type
                                                      0x01,         0x30,  // Unknown
                                                      0x00,                // Payload Size
                                                      0x00,         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                      0x00,         0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Payload
                                                      0x00};

/* A class representing the raw packet sent to or from the Mitsubishi equipment with definitions
for header indexes, checksum calculations, utility methods, etc.  These generally shouldn't be accessed
directly outside the MUARTBridge, and the Packet class (or its subclasses) should be used instead.
*/
class RawPacket {
 public:
  RawPacket(
      const uint8_t packet_bytes[], uint8_t packet_length, SourceBridge source_bridge = SourceBridge::NONE,
      ControllerAssociation controller_association = ControllerAssociation::MUART);  // For reading or copying packets
  // TODO: Can I hide this constructor except from optional?
  RawPacket();  // For optional<RawPacket> construction
  RawPacket(PacketType packet_type, uint8_t payload_size, SourceBridge source_bridge = SourceBridge::NONE,
            ControllerAssociation controller_association = ControllerAssociation::MUART);  // For building packets
  virtual ~RawPacket() {}

  virtual std::string to_string() const { return format_hex_pretty(&get_bytes()[0], get_length()); };

  uint8_t get_length() const { return length_; };
  const uint8_t *get_bytes() const { return packet_bytes_; };  // Primarily for sending packets

  bool is_checksum_valid() const;

  // Returns the packet type byte
  uint8_t get_packet_type() const { return packet_bytes_[PACKET_HEADER_INDEX_PACKET_TYPE]; };
  // Returns the first byte of the payload, often used as a command
  uint8_t get_command() const { return get_payload_byte(PLINDEX_COMMAND); };

  SourceBridge get_source_bridge() const { return source_bridge_; };
  ControllerAssociation get_controller_association() const { return controller_association_; };

  RawPacket &set_payload_byte(uint8_t payload_byte_index, uint8_t value);
  RawPacket &set_payload_bytes(uint8_t begin_index, const void *value, size_t size);
  uint8_t get_payload_byte(const uint8_t payload_byte_index) const {
    return packet_bytes_[PACKET_HEADER_SIZE + payload_byte_index];
  };
  const uint8_t *get_payload_bytes(size_t startIndex = 0) const {
    return &packet_bytes_[PACKET_HEADER_SIZE + startIndex];
  }

 private:
  static const int PLINDEX_COMMAND = 0;

  uint8_t packet_bytes_[PACKET_MAX_SIZE]{};
  uint8_t length_;
  uint8_t checksum_index_;

  SourceBridge source_bridge_;
  ControllerAssociation controller_association_;

  uint8_t calculate_checksum_() const;
  RawPacket &update_checksum_();
};

}  // namespace mitsubishi_itp
}  // namespace esphome
