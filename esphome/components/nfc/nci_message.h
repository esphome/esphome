#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace nfc {

class NciMessage {
 public:
  NciMessage() {}
  NciMessage(uint8_t message_type, const std::vector<uint8_t> &payload);
  NciMessage(uint8_t message_type, uint8_t gid, uint8_t oid);
  NciMessage(uint8_t message_type, uint8_t gid, uint8_t oid, const std::vector<uint8_t> &payload);
  NciMessage(const std::vector<uint8_t> &raw_packet);

  std::vector<uint8_t> encode();
  void reset();

  uint8_t get_message_type() const;
  uint8_t get_gid() const;
  uint8_t get_oid() const;
  uint8_t get_payload_size(bool recompute = false);
  uint8_t get_simple_status_response() const;
  uint8_t get_message_byte(uint8_t offset) const;
  std::vector<uint8_t> &get_message();

  bool has_payload() const;
  bool message_type_is(uint8_t message_type) const;
  bool message_length_is(uint8_t message_length, bool recompute = false);
  bool gid_is(uint8_t gid) const;
  bool oid_is(uint8_t oid) const;
  bool simple_status_response_is(uint8_t response) const;

  void set_header(uint8_t message_type, uint8_t gid, uint8_t oid);
  void set_message(uint8_t message_type, const std::vector<uint8_t> &payload);
  void set_message(uint8_t message_type, uint8_t gid, uint8_t oid, const std::vector<uint8_t> &payload);
  void set_message_type(uint8_t message_type);
  void set_gid(uint8_t gid);
  void set_oid(uint8_t oid);
  void set_payload(const std::vector<uint8_t> &payload);

 protected:
  std::vector<uint8_t> nci_message_{0, 0, 0};  // three bytes, MT/PBF/GID, OID, payload length/size
};

}  // namespace nfc
}  // namespace esphome
