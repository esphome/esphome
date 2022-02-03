#include "packets.h"

#ifdef USE_MQTT

namespace esphome {
namespace mqtt {

namespace util {

ErrorCode encode_uint16(std::vector<uint8_t> &target, uint16_t value) {
  target.push_back((value >> 8) & 0xFF);
  target.push_back((value >> 0) & 0xFF);
  return ErrorCode::OK;
}
ErrorCode decode_uint16(Parser *parser, uint16_t *value) {
  if (parser->size_left() < 2)
    return ErrorCode::MALFORMED_PACKET;
  *value = 0;
  *value |= static_cast<uint16_t>(parser->consume()) << 8;
  *value |= static_cast<uint16_t>(parser->consume()) << 0;
  return ErrorCode::OK;
}
ErrorCode encode_bytes(std::vector<uint8_t> &target, const std::vector<uint8_t> &value) {
  if (value.size() > 65535)
    return ErrorCode::VALUE_TOO_LONG;
  encode_uint16(target, value.size());
  target.insert(target.end(), value.begin(), value.end());
  return ErrorCode::OK;
}
ErrorCode decode_bytes(Parser *parser, std::vector<uint8_t> *value) {
  uint16_t len;
  ErrorCode ec = decode_uint16(parser, &len);
  if (ec != ErrorCode::OK)
    return ec;
  if (len > parser->size_left())
    return ErrorCode::MALFORMED_PACKET;
  value->clear();
  value->reserve(len);
  for (size_t i = 0; i < len; i++)
    value->push_back(parser->consume());
  return ErrorCode::OK;
}
ErrorCode encode_utf8(std::vector<uint8_t> &target, const std::string &value) {
  if (value.size() > 65535)
    return ErrorCode::VALUE_TOO_LONG;
  encode_uint16(target, value.size());
  for (char c : value)
    target.push_back(static_cast<uint8_t>(c));
  return ErrorCode::OK;
}
ErrorCode decode_utf8(Parser *parser, std::string *value) {
  uint16_t len;
  ErrorCode ec = decode_uint16(parser, &len);
  if (ec != ErrorCode::OK)
    return ec;
  if (len > parser->size_left())
    return ErrorCode::MALFORMED_PACKET;
  value->clear();
  value->reserve(len);
  for (size_t i = 0; i < len; i++)
    value->push_back(static_cast<char>(parser->consume()));
  return ErrorCode::OK;
}
ErrorCode encode_varint(std::vector<uint8_t> &target, size_t value) {
  do {
    uint8_t encbyte = value % 0x80;
    value >>= 7;
    if (value > 0)
      encbyte |= 0x80;
    target.push_back(encbyte);
  } while (value > 0);
  return ErrorCode::OK;
}

ErrorCode encode_fixed_header(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags,
  size_t remaining_length
) {
  uint8_t head = 0;
  head |= static_cast<uint8_t>(packet_type) << 4;
  head |= flags;
  target.push_back(head);
  return encode_varint(target, remaining_length);
}

ErrorCode encode_packet(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags
) {
  return encode_fixed_header(target, packet_type, flags, 0);
}

ErrorCode encode_packet(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags,
  const std::vector<uint8_t> &variable_header
) {
  ErrorCode ec = encode_fixed_header(
    target, packet_type, flags,
    variable_header.size()
  );
  if (ec != ErrorCode::OK)
    return ec;
  target.insert(target.end(), variable_header.begin(), variable_header.end());
  return ErrorCode::OK;
}

ErrorCode encode_packet(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags,
  const std::vector<uint8_t> &variable_header,
  const std::vector<uint8_t> &payload
) {
  ErrorCode ec = encode_fixed_header(
    target, packet_type, flags,
    variable_header.size() + payload.size()
  );
  if (ec != ErrorCode::OK)
    return ec;
  target.insert(target.end(), variable_header.begin(), variable_header.end());
  target.insert(target.end(), payload.begin(), payload.end());
  return ErrorCode::OK;
}

}  // namespace util

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
