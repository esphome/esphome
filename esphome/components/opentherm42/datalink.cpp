#include "datalink.h"
#include <cstring>
#include "esphome/core/helpers.h"

namespace esphome::opentherm42 {

// name is always an enum-qualified expression (e.g. MessageType::DATA_INVALID) -- stringifying it
// directly would return the whole "EnumType::VALUE" text; strrchr + 1 trims everything up to and
// including the last ':', leaving just "VALUE" for log output.
#define TO_STRING_CASE(name) \
  case name: \
    return strrchr(#name, ':') + 1;

const char *data_link_error_to_string(DataLinkError error) {
  switch (error) {
    TO_STRING_CASE(DataLinkError::NONE)
    TO_STRING_CASE(DataLinkError::MANCHESTER_NO_TRANSITION)
    TO_STRING_CASE(DataLinkError::MANCHESTER_TIMEOUT)
    TO_STRING_CASE(DataLinkError::INVALID_STOP_BIT)
    TO_STRING_CASE(DataLinkError::PARITY_ERROR)
    TO_STRING_CASE(DataLinkError::RESPONSE_TIMEOUT)
    TO_STRING_CASE(DataLinkError::RMT_ERROR)
    default:
      return "<INVALID>";
  }
}

const char *message_type_to_string(MessageType type) {
  switch (type) {
    TO_STRING_CASE(MessageType::READ_DATA)
    TO_STRING_CASE(MessageType::WRITE_DATA)
    TO_STRING_CASE(MessageType::INVALID_DATA)
    TO_STRING_CASE(MessageType::READ_ACK)
    TO_STRING_CASE(MessageType::WRITE_ACK)
    TO_STRING_CASE(MessageType::DATA_INVALID)
    TO_STRING_CASE(MessageType::UNKNOWN_DATA_ID)
    default:
      return "<INVALID>";
  }
}

const char *rmt_error_to_string(RmtError error) {
  switch (error) {
    TO_STRING_CASE(RmtError::RMT_ERROR_NONE)
    TO_STRING_CASE(RmtError::RMT_ERROR_NEW_TX_CHANNEL)
    TO_STRING_CASE(RmtError::RMT_ERROR_NEW_RX_CHANNEL)
    TO_STRING_CASE(RmtError::RMT_ERROR_NEW_ENCODER)
    TO_STRING_CASE(RmtError::RMT_ERROR_ENABLE)
    TO_STRING_CASE(RmtError::RMT_ERROR_TRANSMIT)
    TO_STRING_CASE(RmtError::RMT_ERROR_RECEIVE)
    TO_STRING_CASE(RmtError::RMT_ERROR_TIMER)
    default:
      return "<INVALID>";
  }
}

void IRAM_ATTR OpenThermDataLink::set_error_(DataLinkError error) {
  this->state_ = DataLinkState::ERROR;
  this->error_ = error;
}

// §4.2.1: the parity bit is set/cleared such that the total number of '1' bits across the whole 32-bit
// frame is even. https://graphics.stanford.edu/~seander/bithacks.html#ParityParallel
bool OpenThermDataLink::check_parity(uint32_t frame_bits) {
  frame_bits ^= frame_bits >> 16;
  frame_bits ^= frame_bits >> 8;
  frame_bits ^= frame_bits >> 4;
  frame_bits &= 0xF;
  return ((0x6996 >> frame_bits) & 1) == 0;
}

}  // namespace esphome::opentherm42
