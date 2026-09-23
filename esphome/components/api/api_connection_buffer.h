#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API

// Inline APIConnection members that need APIServer complete. Include this
// instead of api_connection.h when calling them.

#include "api_connection.h"
#include "api_server.h"

namespace esphome::api {

inline uint16_t ESPHOME_ALWAYS_INLINE APIConnection::encode_to_buffer(uint32_t calculated_size,
                                                                      MessageEncodeFn encode_fn, const void *msg,
                                                                      APIConnection *conn, uint32_t remaining_size) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  if (conn->flags_.log_only_mode) {
    auto *proto_msg = static_cast<const ProtoMessage *>(msg);
    DumpBuffer dump_buf;
    conn->log_send_message_(proto_msg->message_name(), proto_msg->dump_to(dump_buf));
    return 1;
  }
#endif
  const uint8_t footer_size = conn->helper_->frame_footer_size();

  // First message uses max padding (already in buffer), subsequent use exact header size
  size_t to_add;
  if (conn->flags_.batch_first_message) {
    conn->flags_.batch_first_message = false;
    conn->batch_header_size_ = conn->helper_->frame_header_padding();
    to_add = calculated_size;
  } else {
    conn->batch_header_size_ = conn->helper_->frame_header_size(calculated_size, conn->batch_message_type_);
    to_add = calculated_size + conn->batch_header_size_ + footer_size;
  }

  // Check if it fits (using actual header size, not max padding)
  uint16_t total_calculated_size = calculated_size + conn->batch_header_size_ + footer_size;
  if (total_calculated_size > remaining_size)
    return 0;

  auto &shared_buf = conn->parent_->get_shared_buffer_ref();
  if (!shared_buf.resize(shared_buf.size() + to_add)) [[unlikely]] {
    conn->fatal_out_of_memory_();
    return 0;
  }
  ProtoWriteBuffer buffer{&shared_buf, shared_buf.size() - calculated_size};
  encode_fn(msg, buffer PROTO_ENCODE_DEBUG_INIT(&shared_buf));

  return total_calculated_size;
}

inline uint32_t APIConnection::get_batch_delay_ms_() const { return this->parent_->get_batch_delay(); }

inline bool APIConnection::prepare_first_message_buffer(size_t header_padding, size_t total_size) {
  auto &shared_buf = this->parent_->get_shared_buffer_ref();
  shared_buf.clear();
  // Reserve space for header padding + message + footer
  // - Header padding: space for protocol headers (7 bytes for Noise, 6 for Plaintext)
  // - Footer: space for MAC (16 bytes for Noise, 0 for Plaintext)
  // Reserve full size but only set initial size to header padding
  // so message encoding starts at the correct position
  return shared_buf.reserve_and_resize(total_size, header_padding);
}

inline bool APIConnection::prepare_first_message_buffer(size_t payload_size) {
  const uint8_t header_padding = this->helper_->frame_header_padding();
  const uint8_t footer_size = this->helper_->frame_footer_size();
  return this->prepare_first_message_buffer(header_padding, payload_size + header_padding + footer_size);
}

}  // namespace esphome::api
#endif
