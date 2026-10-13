#include "nci_core.h"
#include "nci_message.h"
#include "esphome/core/log.h"

namespace esphome::nfc {

NciMessage::NciMessage(const uint8_t message_type, const std::span<const uint8_t> payload) {
  this->set_message(message_type, payload);
}

NciMessage::NciMessage(const uint8_t message_type, const std::initializer_list<uint8_t> payload)
    : NciMessage(message_type, std::span<const uint8_t>(payload.begin(), payload.size())) {}

NciMessage::NciMessage(const uint8_t message_type, const uint8_t gid, const uint8_t oid) {
  this->reset();
  this->set_header(message_type, gid, oid);
}

NciMessage::NciMessage(const uint8_t message_type, const uint8_t gid, const uint8_t oid,
                       const std::span<const uint8_t> payload) {
  this->set_message(message_type, gid, oid, payload);
}

NciMessage::NciMessage(const uint8_t message_type, const uint8_t gid, const uint8_t oid,
                       const std::initializer_list<uint8_t> payload)
    : NciMessage(message_type, gid, oid, std::span<const uint8_t>(payload.begin(), payload.size())) {}

NciMessage::NciMessage(const std::span<const uint8_t> raw_packet) {
  this->nci_message_.assign(raw_packet.begin(), raw_packet.end());
}

std::span<const uint8_t> NciMessage::encode() {
  this->nci_message_[NCI_PKT_LENGTH_OFFSET] = this->nci_message_.size() - NCI_PKT_HEADER_SIZE;
  return this->nci_message_;
}

void NciMessage::reset() {
  this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  this->nci_message_[NCI_PKT_MT_GID_OFFSET] = 0;
  this->nci_message_[NCI_PKT_OID_OFFSET] = 0;
  this->nci_message_[NCI_PKT_LENGTH_OFFSET] = 0;
}

uint8_t NciMessage::get_message_type() const { return this->nci_message_[NCI_PKT_MT_GID_OFFSET] & NCI_PKT_MT_MASK; }

uint8_t NciMessage::get_gid() const { return this->nci_message_[NCI_PKT_MT_GID_OFFSET] & NCI_PKT_GID_MASK; }

uint8_t NciMessage::get_oid() const { return this->nci_message_[NCI_PKT_OID_OFFSET] & NCI_PKT_OID_MASK; }

uint8_t NciMessage::get_payload_size(const bool recompute) {
  if (!this->nci_message_.empty()) {
    if (recompute) {
      this->nci_message_[NCI_PKT_LENGTH_OFFSET] = this->nci_message_.size() - NCI_PKT_HEADER_SIZE;
    }
    return this->nci_message_[NCI_PKT_LENGTH_OFFSET];
  }
  return 0;
}

uint8_t NciMessage::get_simple_status_response() const {
  if (this->nci_message_.size() > NCI_PKT_PAYLOAD_OFFSET) {
    return this->nci_message_[NCI_PKT_PAYLOAD_OFFSET];
  }
  return STATUS_FAILED;
}

uint8_t NciMessage::get_message_byte(const uint8_t offset) const {
  if (this->nci_message_.size() > offset) {
    return this->nci_message_[offset];
  }
  return 0;
}

std::span<const uint8_t> NciMessage::get_payload() const {
  if (this->nci_message_.size() <= NCI_PKT_HEADER_SIZE) {
    return {};
  }
  return {this->nci_message_.data() + NCI_PKT_HEADER_SIZE, this->nci_message_.size() - NCI_PKT_HEADER_SIZE};
}

bool NciMessage::has_payload() const { return this->nci_message_.size() > NCI_PKT_HEADER_SIZE; }

bool NciMessage::message_type_is(const uint8_t message_type) const {
  if (!this->nci_message_.empty()) {
    return message_type == (this->nci_message_[NCI_PKT_MT_GID_OFFSET] & NCI_PKT_MT_MASK);
  }
  return false;
}

bool NciMessage::message_length_is(const uint8_t message_length, const bool recompute) {
  if (this->nci_message_.size() > NCI_PKT_LENGTH_OFFSET) {
    if (recompute) {
      this->nci_message_[NCI_PKT_LENGTH_OFFSET] = this->nci_message_.size() - NCI_PKT_HEADER_SIZE;
    }
    return message_length == this->nci_message_[NCI_PKT_LENGTH_OFFSET];
  }
  return false;
}

bool NciMessage::gid_is(const uint8_t gid) const {
  if (this->nci_message_.size() > NCI_PKT_MT_GID_OFFSET) {
    return gid == (this->nci_message_[NCI_PKT_MT_GID_OFFSET] & NCI_PKT_GID_MASK);
  }
  return false;
}

bool NciMessage::oid_is(const uint8_t oid) const {
  if (this->nci_message_.size() > NCI_PKT_OID_OFFSET) {
    return oid == (this->nci_message_[NCI_PKT_OID_OFFSET] & NCI_PKT_OID_MASK);
  }
  return false;
}

bool NciMessage::simple_status_response_is(const uint8_t response) const {
  if (this->nci_message_.size() > NCI_PKT_PAYLOAD_OFFSET) {
    return response == this->nci_message_[NCI_PKT_PAYLOAD_OFFSET];
  }
  return false;
}

void NciMessage::set_header(const uint8_t message_type, const uint8_t gid, const uint8_t oid) {
  if (this->nci_message_.size() < NCI_PKT_HEADER_SIZE) {
    this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  }
  this->nci_message_[NCI_PKT_MT_GID_OFFSET] = (message_type & NCI_PKT_MT_MASK) | (gid & NCI_PKT_GID_MASK);
  this->nci_message_[NCI_PKT_OID_OFFSET] = oid & NCI_PKT_OID_MASK;
}

void NciMessage::set_message(const uint8_t message_type, const std::span<const uint8_t> payload) {
  this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  this->nci_message_[NCI_PKT_MT_GID_OFFSET] = message_type & NCI_PKT_MT_MASK;
  this->nci_message_[NCI_PKT_OID_OFFSET] = 0;
  this->append(payload);
  this->nci_message_[NCI_PKT_LENGTH_OFFSET] = this->nci_message_.size() - NCI_PKT_HEADER_SIZE;
}

void NciMessage::set_message(const uint8_t message_type, const uint8_t gid, const uint8_t oid,
                             const std::span<const uint8_t> payload) {
  this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  this->set_header(message_type, gid, oid);
  this->append(payload);
  this->nci_message_[NCI_PKT_LENGTH_OFFSET] = this->nci_message_.size() - NCI_PKT_HEADER_SIZE;
}

void NciMessage::set_message_type(const uint8_t message_type) {
  if (this->nci_message_.size() < NCI_PKT_HEADER_SIZE) {
    this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  }
  auto mt_masked = this->nci_message_[NCI_PKT_MT_GID_OFFSET] & ~NCI_PKT_MT_MASK;
  this->nci_message_[NCI_PKT_MT_GID_OFFSET] = mt_masked | (message_type & NCI_PKT_MT_MASK);
}

void NciMessage::set_gid(const uint8_t gid) {
  if (this->nci_message_.size() < NCI_PKT_HEADER_SIZE) {
    this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  }
  auto gid_masked = this->nci_message_[NCI_PKT_MT_GID_OFFSET] & ~NCI_PKT_GID_MASK;
  this->nci_message_[NCI_PKT_MT_GID_OFFSET] = gid_masked | (gid & NCI_PKT_GID_MASK);
}

void NciMessage::set_oid(const uint8_t oid) {
  if (this->nci_message_.size() < NCI_PKT_HEADER_SIZE) {
    this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  }
  this->nci_message_[NCI_PKT_OID_OFFSET] = oid & NCI_PKT_OID_MASK;
}

void NciMessage::set_payload(const std::span<const uint8_t> payload) {
  this->nci_message_.resize(NCI_PKT_HEADER_SIZE);
  this->append(payload);
  this->nci_message_[NCI_PKT_LENGTH_OFFSET] = this->nci_message_.size() - NCI_PKT_HEADER_SIZE;
}

void NciMessage::append(const std::span<const uint8_t> data) {
  for (const uint8_t byte : data) {
    this->nci_message_.push_back(byte);
  }
}

void NciMessage::set_payload_size(const uint8_t size) { this->nci_message_.resize(NCI_PKT_HEADER_SIZE + size); }

}  // namespace esphome::nfc
