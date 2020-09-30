#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

static const uint8_t TNF_EMPTY = 0x00;
static const uint8_t TNF_WELL_KNOWN = 0x01;
static const uint8_t TNF_MIME_MEDIA = 0x02;
static const uint8_t TNF_ABSOLUTE_URI = 0x03;
static const uint8_t TNF_EXTERNAL_TYPE = 0x04;
static const uint8_t TNF_UNKNOWN = 0x05;
static const uint8_t TNF_UNCHANGED = 0x06;
static const uint8_t TNF_RESERVED = 0x07;

namespace esphome {
namespace nfc {

class NdefRecord {
 public:
  NdefRecord(){};
  NdefRecord(uint8_t tnf, std::string type, std::string payload) {
    this->tnf_ = tnf;
    this->type_ = type;
    this->payload_ = payload;
  };
  NdefRecord(uint8_t tnf, std::string type, std::string payload, std::string id) {
    this->tnf_ = tnf;
    this->type_ = type;
    this->payload_ = payload;
    this->id_ = id;
  };
  void set_tnf(uint8_t tnf) { this->tnf_ = tnf; };
  void set_type(std::string type) { this->type_ = type; };
  void set_payload(std::string payload) { this->payload_ = payload; };
  void set_id(std::string id) { this->id_ = id; };

  uint32_t get_encoded_size();

  void encode(uint8_t* data, bool first, bool last);
  uint8_t get_tnf_byte(bool first, bool last);

  std::string get_type() { return this->type_; };
  std::string get_payload() { return this->payload_; };
  std::string get_id() { return this->id_; };

 protected:
  uint8_t tnf_;
  std::string type_;
  std::string payload_;
  std::string id_;
};

}  // namespace nfc
}  // namespace esphome
