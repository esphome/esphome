#include "service_call_message.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

APIMessageType SubscribeServiceCallsRequest::message_type() const {
  return APIMessageType::SUBSCRIBE_SERVICE_CALLS_REQUEST;
}

APIMessageType ServiceCallResponse::message_type() const { return APIMessageType::SERVICE_CALL_RESPONSE; }
void ServiceCallResponse::encode(APIBuffer &buffer) {
  // string service = 1;
  buffer.encode_string(1, this->service_);
  // map<string, string> data = 2;
  for (auto &it : this->data_) {
    auto nested = buffer.begin_nested(2);
    buffer.encode_string(1, it.key);
    buffer.encode_string(2, it.value);
    buffer.end_nested(nested);
  }
  // map<string, string> data_template = 3;
  for (auto &it : this->data_template_) {
    auto nested = buffer.begin_nested(3);
    buffer.encode_string(1, it.key);
    buffer.encode_string(2, it.value);
    buffer.end_nested(nested);
  }
  // map<string, string> variables = 4;
  for (auto &it : this->variables_) {
    auto nested = buffer.begin_nested(4);
    buffer.encode_string(1, it.key);
    buffer.encode_string(2, it.value());
    buffer.end_nested(nested);
  }
}
void ServiceCallResponse::set_service(const std::string &service) { this->service_ = service; }
void ServiceCallResponse::set_data(const std::vector<KeyValuePair> &data) { this->data_ = data; }
void ServiceCallResponse::set_data_template(const std::vector<KeyValuePair> &data_template) {
  this->data_template_ = data_template;
}
void ServiceCallResponse::set_variables(const std::vector<TemplatableKeyValuePair> &variables) {
  this->variables_ = variables;
}

KeyValuePair::KeyValuePair(const std::string &key, const std::string &value) : key(key), value(value) {}

}  // namespace api
}  // namespace esphome
