#include "genibus_frame.h"

#include <algorithm>

#include "esphome/core/helpers.h"

namespace esphome::genibus {

// Bits 7-6 of the second APDU byte
static constexpr uint8_t APDU_ACK_SHIFT = 6;

uint16_t frame_crc(const uint8_t *data, size_t len) {
  // Initial value and final XOR 0xFFFF, not reflected
  return crc16be(data, len, 0, 0x1021, true, true);
}

size_t build_request(std::span<uint8_t> out, uint8_t destination, uint8_t source, std::span<const uint8_t> apdus) {
  const size_t size = apdus.size() + 2 + FRAME_OVERHEAD;
  if (apdus.empty() || size > MAX_FRAME_SIZE || size > out.size())
    return 0;
  out[0] = START_REQUEST;
  out[1] = static_cast<uint8_t>(apdus.size() + 2);
  out[2] = destination;
  out[3] = source;
  std::copy(apdus.begin(), apdus.end(), out.begin() + 4);
  put_be16(&out[size - 2], frame_crc(&out[1], size - 3));
  return size;
}

uint8_t *put_get_object(uint8_t *dst, uint8_t object, uint16_t sub_id) {
  *dst++ = CLASS_DATA_OBJECT;
  *dst++ = OPERATION_GET | 3;
  *dst++ = object;
  return put_be16(dst, sub_id);
}

bool ApduReader::next(Apdu &apdu) {
  if (this->rest_.size() < 2)
    return false;
  const size_t length = this->rest_[1] & APDU_LENGTH_MASK;
  if (this->rest_.size() < length + 2)
    return false;
  apdu.data_class = this->rest_[0];
  apdu.ack = this->rest_[1] >> APDU_ACK_SHIFT;
  apdu.data = this->rest_.subspan(2, length);
  this->rest_ = this->rest_.subspan(length + 2);
  return true;
}

bool parse_data_object(const Apdu &apdu, DataObject &object) {
  const auto &data = apdu.data;
  if (apdu.data_class != CLASS_DATA_OBJECT || apdu.ack != 0 || data.size() < DATA_OBJECT_HEADER_SIZE)
    return false;
  const uint32_t size = encode_uint24(data[4], data[5], data[6]);
  if (size != data.size() - DATA_OBJECT_HEADER_SIZE)
    return false;
  object.status = data[0];
  object.type = encode_uint16(data[1], data[2]);
  object.version = data[3];
  object.body = data.subspan(DATA_OBJECT_HEADER_SIZE);
  return true;
}

bool is_object_missing(const Apdu &apdu) {
  if (apdu.data_class != CLASS_DATA_OBJECT)
    return false;
  if (apdu.ack == ACK_UNKNOWN_ID)
    return true;
  return apdu.ack == 0 && apdu.data.size() == 1 && apdu.data[0] == DATA_OBJECT_STATUS_MISSING;
}

bool FrameAssembler::feed(uint8_t byte) {
  // A complete frame that was not reset is dropped, so the buffer never overflows
  if (this->size_ >= 2 && this->size_ == this->buffer_[1] + FRAME_OVERHEAD)
    this->size_ = 0;
  if (this->size_ == 0 && byte != START_REPLY && byte != START_REQUEST && byte != START_MESSAGE)
    return false;
  // A length below DA and SA cannot start a frame
  if (this->size_ == 1 && byte < 2) {
    this->size_ = 0;
    return false;
  }
  this->buffer_[this->size_++] = byte;
  return this->size_ >= 2 && this->size_ == this->buffer_[1] + FRAME_OVERHEAD;
}

bool FrameAssembler::crc_ok() const {
  if (this->size_ < MIN_FRAME_SIZE)
    return false;
  return frame_crc(&this->buffer_[1], this->size_ - 3) ==
         encode_uint16(this->buffer_[this->size_ - 2], this->buffer_[this->size_ - 1]);
}

}  // namespace esphome::genibus
