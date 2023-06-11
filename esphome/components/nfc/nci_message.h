#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace nfc {

class NciMessage {
 public:
  NciMessage() {}
  NciMessage(const uint8_t message_type, const std::vector<uint8_t> &payload);
  NciMessage(const uint8_t message_type, const uint8_t gid, const uint8_t oid);
  NciMessage(const uint8_t message_type, const uint8_t gid, const uint8_t oid, const std::vector<uint8_t> &payload);
  NciMessage(const std::vector<uint8_t> &raw_packet);

  std::vector<uint8_t> encode();
  void reset();

  const uint8_t get_message_type() const;
  const uint8_t get_gid() const;
  const uint8_t get_oid() const;
  const uint8_t get_payload_size(const bool recompute = false);
  const uint8_t get_simple_status_response() const;
  const uint8_t get_message_byte(const uint8_t offset) const;
  std::vector<uint8_t> &get_message();

  const bool has_payload() const;
  const bool message_type_is(const uint8_t message_type) const;
  const bool message_length_is(const uint8_t message_length, const bool recompute = false);
  const bool gid_is(const uint8_t gid) const;
  const bool oid_is(const uint8_t oid) const;
  const bool simple_status_response_is(const uint8_t response) const;

  void set_header(const uint8_t message_type, const uint8_t gid, const uint8_t oid);
  void set_message(const uint8_t message_type, const std::vector<uint8_t> &payload);
  void set_message(const uint8_t message_type, const uint8_t gid, const uint8_t oid,
                   const std::vector<uint8_t> &payload);
  void set_message_type(const uint8_t message_type);
  void set_gid(const uint8_t gid);
  void set_oid(const uint8_t oid);
  void set_payload(const std::vector<uint8_t> &payload);

 protected:
  std::vector<uint8_t> nci_message_{0, 0, 0};  // three bytes, MT/PBF/GID, OID, payload length/size
};

}  // namespace nfc
}  // namespace esphome
