#include "basic_messages.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

// Hello
bool HelloRequest::decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) {
  switch (field_id) {
    case 1:  // string client_info = 1;
      this->client_info_ = as_string(value, len);
      return true;
    default:
      return false;
  }
}
const std::string &HelloRequest::get_client_info() const { return this->client_info_; }
void HelloRequest::set_client_info(const std::string &client_info) { this->client_info_ = client_info; }
APIMessageType HelloRequest::message_type() const { return APIMessageType::HELLO_REQUEST; }

// Connect
bool ConnectRequest::decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) {
  switch (field_id) {
    case 1:  // string password = 1;
      this->password_ = as_string(value, len);
      return true;
    default:
      return false;
  }
}
const std::string &ConnectRequest::get_password() const { return this->password_; }
void ConnectRequest::set_password(const std::string &password) { this->password_ = password; }
APIMessageType ConnectRequest::message_type() const { return APIMessageType::CONNECT_REQUEST; }

APIMessageType DeviceInfoRequest::message_type() const { return APIMessageType::DEVICE_INFO_REQUEST; }
APIMessageType DisconnectRequest::message_type() const { return APIMessageType::DISCONNECT_REQUEST; }
bool DisconnectRequest::decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) {
  switch (field_id) {
    case 1:  // string reason = 1;
      this->reason_ = as_string(value, len);
      return true;
    default:
      return false;
  }
}
const std::string &DisconnectRequest::get_reason() const { return this->reason_; }
void DisconnectRequest::set_reason(const std::string &reason) { this->reason_ = reason; }
void DisconnectRequest::encode(APIBuffer &buffer) {
  // string reason = 1;
  buffer.encode_string(1, this->reason_);
}
APIMessageType DisconnectResponse::message_type() const { return APIMessageType::DISCONNECT_RESPONSE; }
APIMessageType PingRequest::message_type() const { return APIMessageType::PING_REQUEST; }
APIMessageType PingResponse::message_type() const { return APIMessageType::PING_RESPONSE; }

}  // namespace api
}  // namespace esphome
