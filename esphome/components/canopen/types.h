#pragma once

// Public CANopen types for lambdas and other components. No CANopenNode headers here.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace esphome {
struct LogString;
}  // namespace esphome

namespace esphome::canopen {

enum class SDOResult : uint8_t {
  SDO_RESULT_SUCCESS,
  SDO_RESULT_ABORT,            // the server aborted the transfer, see abort_code
  SDO_RESULT_TIMEOUT,          // no response within response_timeout
  SDO_RESULT_SIZE_MISMATCH,    // a value read returned another width than expected
  SDO_RESULT_TRANSPORT_ERROR,  // the transfer couldn't be started or failed locally
  SDO_RESULT_REJECTED,         // not queued (queue full, invalid arguments, stack not running)
};

const LogString *sdo_result_to_string(SDOResult result);

// Result of an SDO request. Value reads/writes use value/size (little endian, zero-extended); buffer reads use
// data/len, valid only during the callback. abort_code is set for ABORT and TIMEOUT.
struct SDOReply {
  SDOResult result;
  uint32_t abort_code;
  uint32_t value;
  uint8_t size;
  const uint8_t *data;
  size_t len;

  bool ok() const { return this->result == SDOResult::SDO_RESULT_SUCCESS; }
  // value as T: sign-extended from size bytes for signed T, bit cast for float
  template<typename T> T as() const {
    static_assert(sizeof(T) <= 4, "SDO values are at most 4 bytes");
    if constexpr (std::is_same_v<T, float>) {
      float f;
      std::memcpy(&f, &this->value, sizeof(f));
      return f;
    } else if constexpr (std::is_signed_v<T>) {
      if (this->size == 0 || this->size >= 4)
        return static_cast<T>(static_cast<int32_t>(this->value));
      uint32_t sign = 1u << (this->size * 8 - 1);
      return static_cast<T>(static_cast<int32_t>((this->value ^ sign) - sign));
    } else {
      return static_cast<T>(this->value);
    }
  }
};

// Called exactly once per request: from CANopenComponent::loop() / frame processing, or synchronously (REJECTED)
// before read_sdo() / write_sdo() return false.
using SDOCallback = void (*)(void *context, const SDOReply &reply);

}  // namespace esphome::canopen
