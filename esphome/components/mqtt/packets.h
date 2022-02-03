#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT

#include <string>
#include <vector>
#include <cstdint>
#include "esphome/core/optional.h"

namespace esphome {
namespace mqtt {

enum class ErrorCode {
  OK = 0,
  VALUE_TOO_LONG = 1,
  BAD_FLAGS = 2,
  MALFORMED_PACKET = 3,
  BAD_STATE = 4,
  RESOLVE_ERROR = 5,
  SOCKET_ERROR = 6,
  TIMEOUT = 7,
  IN_PROGRESS = 8,
  WOULD_BLOCK = 9,
  CONNECTION_CLOSED = 10,
  UNEXPECTED = 11,
  PROTOCOL_ERROR = 12,
};

enum class PacketType : uint8_t {
  CONNECT = 1,
  CONNACK = 2,
  PUBLISH = 3,
  PUBACK = 4,
  PUBREC = 5,
  PUBREL = 6,
  PUBCOMP = 7,
  SUBSCRIBE = 8,
  SUBACK = 9,
  UNSUBSCRIBE = 10,
  UNSUBACK = 11,
  PINGREQ = 12,
  PINGRESP = 13,
  DISCONNECT = 14,
};

namespace util {
class Parser {
  public:
  Parser(const uint8_t *data, size_t len) : data_(data), len_(len) {}

  size_t size_left() const {
    return len_ - at_;
  }

  uint8_t consume() {
    return data_[at_++];
  }

  void consume(size_t amount) {
    at_ += amount;
  }

  private:
  const uint8_t *data_;
  size_t len_;
  size_t at_ = 0;
};

ErrorCode encode_uint16(std::vector<uint8_t> &target, uint16_t value);
ErrorCode decode_uint16(Parser *parser, uint16_t *value);
ErrorCode encode_bytes(std::vector<uint8_t> &target, const std::vector<uint8_t> &value);
ErrorCode decode_bytes(Parser *parser, std::vector<uint8_t> *value);
ErrorCode encode_utf8(std::vector<uint8_t> &target, const std::string &value);
ErrorCode decode_utf8(Parser *parser, std::string *value);
ErrorCode encode_varint(std::vector<uint8_t> &target, size_t value);
ErrorCode encode_fixed_header(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags,
  size_t remaining_length
);
ErrorCode encode_packet(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags
);
ErrorCode encode_packet(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags,
  const std::vector<uint8_t> &variable_header
);
ErrorCode encode_packet(
  std::vector<uint8_t> &target,
  PacketType packet_type,
  uint8_t flags,
  const std::vector<uint8_t> &variable_header,
  const std::vector<uint8_t> &payload
);

}  // namespace util

class MQTTPacket {
 public:
  virtual ErrorCode encode(std::vector<uint8_t> &target) const = 0;
  virtual ErrorCode decode(uint8_t flags, util::Parser parser) = 0;
};

enum class QOSLevel : uint8_t {
  QOS0 = 0,
  QOS1 = 1,
  QOS2 = 2,
};

class ConnectPacket : public MQTTPacket {
 public:
  // 3.1
  std::string client_id;
  optional<std::string> username;
  optional<std::vector<uint8_t>> password;
  std::string will_topic;
  std::vector<uint8_t> will_message;
  QOSLevel will_qos = QOSLevel::QOS0;
  bool will_retain = false;
  bool clean_session = true;
  uint8_t protocol_level = 4;
  uint16_t keep_alive;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    uint8_t connect_flags = 0;
    if (username.has_value())
      connect_flags |= 0x80;
    if (password.has_value())
      connect_flags |= 0x40;
    if (will_retain)
      connect_flags |= 0x20;
    connect_flags |= static_cast<uint8_t>(will_qos) << 3;
    if (!will_topic.empty())
      connect_flags |= 0x04;
    if (clean_session)
      connect_flags |= 0x02;
    std::vector<uint8_t> variable_header;
    variable_header.push_back(0x00);
    variable_header.push_back(0x04);
    variable_header.push_back('M');
    variable_header.push_back('Q');
    variable_header.push_back('T');
    variable_header.push_back('T');
    variable_header.push_back(protocol_level);
    variable_header.push_back(connect_flags);
    ErrorCode ec = util::encode_uint16(variable_header, keep_alive);
    if (ec != ErrorCode::OK)
      return ec;

    std::vector<uint8_t> payload;
    ec = util::encode_utf8(payload, client_id);
    if (ec != ErrorCode::OK)
      return ec;
    if (!will_topic.empty()) {
      ec = util::encode_utf8(payload, will_topic);
      if (ec != ErrorCode::OK)
        return ec;
      ec = util::encode_bytes(payload, will_message);
      if (ec != ErrorCode::OK)
        return ec;
    }
    if (username.has_value()) {
      ec = util::encode_utf8(payload, *username);
      if (ec != ErrorCode::OK)
        return ec;
    }
    if (password.has_value()) {
      ec = util::encode_bytes(payload, *password);
      if (ec != ErrorCode::OK)
        return ec;
    }

    return util::encode_packet(
      target, PacketType::CONNECT, 0,
      variable_header, payload
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (
      parser.size_left() < 10
      || parser.consume() != '\x00'
      || parser.consume() != '\x04'
      || parser.consume() != 'M'
      || parser.consume() != 'Q'
      || parser.consume() != 'T'
      || parser.consume() != 'T'
    )
      return ErrorCode::MALFORMED_PACKET;
    protocol_level = parser.consume();
    uint8_t connect_flags = parser.consume();
    bool username_flag = connect_flags & 0x80;
    bool password_flag = connect_flags & 0x40;
    will_retain = connect_flags & 0x20;
    will_qos = static_cast<QOSLevel>((connect_flags >> 3) & 3);
    bool will_flag = connect_flags & 0x04;
    clean_session = connect_flags & 0x02;
    if ((flags & 1) != 0)
      return ErrorCode::MALFORMED_PACKET;
    ErrorCode ec = util::decode_uint16(&parser, &keep_alive);
    if (ec != ErrorCode::OK)
      return ec;

    ec = util::decode_utf8(&parser, &client_id);
    if (ec != ErrorCode::OK)
      return ec;

    will_topic.clear();
    will_message.clear();
    if (will_flag) {
      ec = util::decode_utf8(&parser, &will_topic);
      if (ec != ErrorCode::OK)
        return ec;
      ec = util::decode_bytes(&parser, &will_message);
      if (ec != ErrorCode::OK)
        return ec;
    }

    username.reset();
    if (username_flag) {
      username = {""};
      ec = util::decode_utf8(&parser, &(*username));
      if (ec != ErrorCode::OK)
        return ec;
    }
    password.reset();
    if (password_flag) {
      password = std::vector<uint8_t>{};
      ec = util::decode_bytes(&parser, &(*password));
      if (ec != ErrorCode::OK)
        return ec;
    }

    return ErrorCode::OK;
  }
};

enum class ConnectReturnCode : uint8_t {
  ACCEPTED = 0,
  UNACCEPTABLE_PROTOCOL_VERSION = 1,
  IDENTIFIER_REJECTED = 2,
  SERVER_UNAVAILABLE = 3,
  BAD_USER_NAME_OR_PASSWORD = 4,
  NOT_AUTHORIZED = 5,
};

class ConnackPacket : public MQTTPacket {
 public:
  // 3.2
  bool session_present;
  ConnectReturnCode connect_return_code;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    variable_header.push_back(static_cast<uint8_t>(session_present));
    variable_header.push_back(static_cast<uint8_t>(connect_return_code));
    return util::encode_packet(
      target, PacketType::CONNACK, 0,
      variable_header
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 2)
      return ErrorCode::MALFORMED_PACKET;
    session_present = parser.consume() & 1;
    connect_return_code = static_cast<ConnectReturnCode>(parser.consume());
    return ErrorCode::OK;
  }
};

class PublishPacket : public MQTTPacket {
 public:
  // 3.3
  std::string topic;
  std::vector<uint8_t> message;
  bool dup = false;
  QOSLevel qos = QOSLevel::QOS0;
  bool retain = false;
  optional<uint16_t> packet_identifier;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    uint8_t flags = 0;
    if (dup)
      flags |= 0x08;
    flags |= static_cast<uint8_t>(qos) << 1;
    if (retain)
      flags |= 0x01;
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_utf8(variable_header, topic);
    if (ec != ErrorCode::OK)
      return ec;
    if (packet_identifier.has_value()) {
      ec = util::encode_uint16(variable_header, *packet_identifier);
      if (ec != ErrorCode::OK)
        return ec;
    }

    return util::encode_packet(
      target, PacketType::PUBLISH, flags,
      variable_header, message
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    dup = flags & 0x08;
    qos = static_cast<QOSLevel>((flags >> 1) & 3);
    retain = flags & 0x01;
    ErrorCode ec = util::decode_utf8(&parser, &topic);
    if (ec != ErrorCode::OK)
      return ec;
    if (qos == QOSLevel::QOS1 || qos == QOSLevel::QOS2) {
      packet_identifier = 0;
      ec = util::decode_uint16(&parser, &(*packet_identifier));
      if (ec != ErrorCode::OK)
        return ec;
    } else {
      packet_identifier.reset();
    }
    message.clear();
    message.reserve(parser.size_left());
    while (parser.size_left())
      message.push_back(parser.consume());
    return ErrorCode::OK;
  }
};

class PubackPacket : public MQTTPacket {
 public:
  // 3.4
  uint16_t packet_identifier;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    return util::encode_packet(
      target, PacketType::PUBACK, 0,
      variable_header
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 2)
      return ErrorCode::MALFORMED_PACKET;
    return util::decode_uint16(&parser, &packet_identifier);
  }
};

class PubrecPacket : public MQTTPacket {
 public:
  // 3.5
  uint16_t packet_identifier;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    return util::encode_packet(
      target, PacketType::PUBREC, 0,
      variable_header
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 2)
      return ErrorCode::MALFORMED_PACKET;
    return util::decode_uint16(&parser, &packet_identifier);
  }
};

class PubrelPacket : public MQTTPacket {
 public:
  // 3.6
  uint16_t packet_identifier;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    return util::encode_packet(
      target, PacketType::PUBREL, 2,
      variable_header
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 2)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 2)
      return ErrorCode::MALFORMED_PACKET;
    return util::decode_uint16(&parser, &packet_identifier);
  }
};

class PubcompPacket : public MQTTPacket {
 public:
  // 3.7
  uint16_t packet_identifier;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    return util::encode_packet(
      target, PacketType::PUBCOMP, 2,
      variable_header
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 2)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 2)
      return ErrorCode::MALFORMED_PACKET;
    return util::decode_uint16(&parser, &packet_identifier);
  }
};

struct Subscription {
  std::string topic_filter;
  QOSLevel requested_qos = QOSLevel::QOS0;
};

class SubscribePacket : public MQTTPacket {
 public:
  // 3.8
  uint16_t packet_identifier;
  std::vector<Subscription> subscriptions;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    std::vector<uint8_t> payload;
    for (const auto &sub : subscriptions) {
      ec = util::encode_utf8(payload, sub.topic_filter);
      if (ec != ErrorCode::OK)
        return ec;
      payload.push_back(static_cast<uint8_t>(sub.requested_qos));
    }
    return util::encode_packet(
      target, PacketType::SUBSCRIBE, 2,
      variable_header, payload
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 2)
      return ErrorCode::BAD_FLAGS;

    ErrorCode ec = util::decode_uint16(&parser, &packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    subscriptions.clear();
    while (parser.size_left()) {
      Subscription sub{};
      ec = util::decode_utf8(&parser, &sub.topic_filter);
      if (ec != ErrorCode::OK)
        return ec;
      if (parser.size_left() < 1)
        return ErrorCode::MALFORMED_PACKET;
      sub.requested_qos = static_cast<QOSLevel>(parser.consume());
      subscriptions.push_back(sub);
    }
    return ErrorCode::OK;
  }
};

enum class SubackReturnCode : uint8_t {
  SUCCESS_MAX_QOS0 = 0x00,
  SUCCESS_MAX_QOS1 = 0x01,
  SUCCESS_MAX_QOS2 = 0x02,
  FAILURE = 0x80,
};

class SubackPacket : public MQTTPacket {
 public:
  // 3.9
  uint16_t packet_identifier;
  std::vector<SubackReturnCode> return_codes;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    std::vector<uint8_t> payload;
    payload.reserve(return_codes.size());
    for (SubackReturnCode rc : return_codes) {
      payload.push_back(static_cast<uint8_t>(rc));
    }
    return util::encode_packet(
      target, PacketType::SUBACK, 0,
      variable_header, payload
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 2)
      return ErrorCode::BAD_FLAGS;
    ErrorCode ec = util::decode_uint16(&parser, &packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    return_codes.clear();
    return_codes.reserve(parser.size_left());
    while (parser.size_left()) {
      return_codes.push_back(static_cast<SubackReturnCode>(parser.consume()));
    }
    return ErrorCode::OK;
  }
};

class UnsubscribePacket : public MQTTPacket {
 public:
  // 3.10
  uint16_t packet_identifier;
  std::vector<std::string> topic_filters;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    std::vector<uint8_t> payload;
    for (const auto &topic : topic_filters) {
      ec = util::encode_utf8(payload, topic);
      if (ec != ErrorCode::OK)
        return ec;
    }
    return util::encode_packet(
      target, PacketType::UNSUBACK, 2,
      variable_header, payload
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 2)
      return ErrorCode::BAD_FLAGS;

    ErrorCode ec = util::decode_uint16(&parser, &packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    topic_filters.clear();
    while (parser.size_left()) {
      std::string topic;
      ec = util::decode_utf8(&parser, &topic);
      if (ec != ErrorCode::OK)
        return ec;
      topic_filters.push_back(topic);
    }
    return ErrorCode::OK;
  }
};

class UnsubackPacket : public MQTTPacket {
 public:
  // 3.11
  uint16_t packet_identifier;

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    std::vector<uint8_t> variable_header;
    ErrorCode ec = util::encode_uint16(variable_header, packet_identifier);
    if (ec != ErrorCode::OK)
      return ec;
    return util::encode_packet(
      target, PacketType::UNSUBACK, 0,
      variable_header
    );
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 2)
      return ErrorCode::MALFORMED_PACKET;
    return util::decode_uint16(&parser, &packet_identifier);
  }
};

class PingreqPacket : public MQTTPacket {
 public:
  // 3.12

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    return util::encode_packet(target, PacketType::PINGREQ, 0);
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 0)
      return ErrorCode::MALFORMED_PACKET;
    return ErrorCode::OK;
  }
};

class PingrespPacket : public MQTTPacket {
 public:
  // 3.13

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    return util::encode_packet(target, PacketType::PINGRESP, 0);
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 0)
      return ErrorCode::MALFORMED_PACKET;
    return ErrorCode::OK;
  }
};

class DisconnectPacket : public MQTTPacket {
 public:
  // 3.14

  ErrorCode encode(std::vector<uint8_t> &target) const final {
    return util::encode_packet(target, PacketType::DISCONNECT, 0);
  }

  ErrorCode decode(uint8_t flags, util::Parser parser) override final {
    if (flags != 0)
      return ErrorCode::BAD_FLAGS;
    if (parser.size_left() != 0)
      return ErrorCode::MALFORMED_PACKET;
    return ErrorCode::OK;
  }
};

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
