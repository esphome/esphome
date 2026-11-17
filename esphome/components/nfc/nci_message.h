#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <initializer_list>
#include <span>

namespace esphome::nfc {

// An NCI packet is a three-byte header followed by up to 255 payload bytes
static constexpr size_t NCI_PKT_MAX_PAYLOAD_SIZE = 255;
static constexpr size_t NCI_PKT_MAX_SIZE = 3 + NCI_PKT_MAX_PAYLOAD_SIZE;

/// One NCI packet, held in a fixed buffer so building and reading messages never allocates
class NciMessage {
 public:
  using Buffer = StaticVector<uint8_t, NCI_PKT_MAX_SIZE>;

  NciMessage() { this->reset(); }
  NciMessage(uint8_t message_type, std::span<const uint8_t> payload);
  NciMessage(uint8_t message_type, std::initializer_list<uint8_t> payload);
  NciMessage(uint8_t message_type, uint8_t gid, uint8_t oid);
  NciMessage(uint8_t message_type, uint8_t gid, uint8_t oid, std::span<const uint8_t> payload);
  NciMessage(uint8_t message_type, uint8_t gid, uint8_t oid, std::initializer_list<uint8_t> payload);
  explicit NciMessage(std::span<const uint8_t> raw_packet);

  /// Stamps the payload length into the header and returns the packet ready to send
  std::span<const uint8_t> encode();
  void reset();

  uint8_t get_message_type() const;
  uint8_t get_gid() const;
  uint8_t get_oid() const;
  uint8_t get_payload_size(bool recompute = false);
  uint8_t get_simple_status_response() const;
  uint8_t get_message_byte(uint8_t offset) const;
  Buffer &get_message() { return this->nci_message_; }
  const Buffer &get_message() const { return this->nci_message_; }
  /// The payload bytes that follow the header
  std::span<const uint8_t> get_payload() const;

  bool has_payload() const;
  bool message_type_is(uint8_t message_type) const;
  bool message_length_is(uint8_t message_length, bool recompute = false);
  bool gid_is(uint8_t gid) const;
  bool oid_is(uint8_t oid) const;
  bool simple_status_response_is(uint8_t response) const;

  void set_header(uint8_t message_type, uint8_t gid, uint8_t oid);
  void set_message(uint8_t message_type, std::span<const uint8_t> payload);
  void set_message(uint8_t message_type, uint8_t gid, uint8_t oid, std::span<const uint8_t> payload);
  void set_message_type(uint8_t message_type);
  void set_gid(uint8_t gid);
  void set_oid(uint8_t oid);
  void set_payload(std::span<const uint8_t> payload);
  void set_payload(std::initializer_list<uint8_t> payload) {
    this->set_payload(std::span<const uint8_t>(payload.begin(), payload.size()));
  }
  /// Appends bytes to the payload; bytes that do not fit are dropped
  void append(std::span<const uint8_t> data);
  void append(std::initializer_list<uint8_t> data) {
    this->append(std::span<const uint8_t>(data.begin(), data.size()));
  }
  /// Sets the packet size to the header plus `size` payload bytes, for a bus driver filling the buffer directly
  void set_payload_size(uint8_t size);

 protected:
  Buffer nci_message_;  // MT/PBF/GID, OID, payload length, then the payload
};

}  // namespace esphome::nfc
