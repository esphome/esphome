#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/api/proto.h"

namespace esphome {
namespace api {

class ProtoClient {
 public:
 protected:
  virtual bool is_authenticated() = 0;
  virtual bool is_connection_setup() = 0;
  virtual void on_fatal_error() = 0;
  virtual ProtoWriteBuffer create_buffer() = 0;
  virtual bool send_buffer(ProtoWriteBuffer buffer, uint32_t message_type) = 0;
  virtual bool read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) = 0;

  template<class C> bool send_message_(const C &msg, uint32_t message_type) {
    auto buffer = this->create_buffer();
    msg.encode(buffer);
    return this->send_buffer(buffer, message_type);
  }
};

}  // namespace api
}  // namespace esphome
