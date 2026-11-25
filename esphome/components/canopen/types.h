#pragma once

// Public types; no CANopenNode headers here.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "esphome/core/helpers.h"

namespace esphome {
struct LogString;
}  // namespace esphome

namespace esphome::canopen {

enum class SDOResult : uint8_t {
  SDO_RESULT_SUCCESS,
  SDO_RESULT_ABORT,            // see abort_code
  SDO_RESULT_TIMEOUT,          // no response within response_timeout
  SDO_RESULT_SIZE_MISMATCH,    // value of another width than expected, or over 4 bytes
  SDO_RESULT_TRANSPORT_ERROR,  // couldn't be started or failed locally
  SDO_RESULT_REJECTED,         // not queued: queue full, invalid arguments or failed hub
};
// A write that ended with TIMEOUT or TRANSPORT_ERROR may still have been done. SDO has no transaction id, so a late
// reply to a timed-out request can be taken as the reply to the next request for the same object of that node.

const LogString *sdo_result_to_string(SDOResult result);

// Values use value/size (zero-extended), buffer reads data/len (valid only during the callback). abort_code is set
// for ABORT and TIMEOUT.
struct SDOReply {
  SDOResult result;
  uint32_t abort_code;
  uint32_t value;
  uint8_t size;
  const uint8_t *data;
  size_t len;

  bool ok() const { return this->result == SDOResult::SDO_RESULT_SUCCESS; }
  // sign-extended from size bytes for signed T, bit cast for float
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

// Called exactly once per request; with REJECTED before the request call returns false, or from a failed setup().
using SDOCallback = void (*)(void *context, const SDOReply &reply);

// NMT state as sent in a heartbeat (CiA 301)
enum class NMTState : uint8_t {
  NMT_STATE_BOOTUP = 0,
  NMT_STATE_STOPPED = 4,
  NMT_STATE_OPERATIONAL = 5,
  NMT_STATE_PRE_OPERATIONAL = 127,
  NMT_STATE_UNKNOWN = 255,
};

// the states a heartbeat can carry; CiA 301 values are sparse
inline constexpr std::array<NMTState, 4> NMT_STATES{NMTState::NMT_STATE_BOOTUP, NMTState::NMT_STATE_STOPPED,
                                                    NMTState::NMT_STATE_OPERATIONAL,
                                                    NMTState::NMT_STATE_PRE_OPERATIONAL};

const LogString *nmt_state_to_string(NMTState state);

// raw CAN frame listener (see CANopenComponent::add_frame_listener()); data is valid only during the call
using FrameCallback = void (*)(void *context, uint32_t can_id, const uint8_t *data, uint8_t len);

// A raw frame listener, owned by the caller and kept for the device's lifetime (e.g. a static)
struct FrameListener {
  uint16_t can_id;
  uint16_t mask;
  FrameCallback callback;
  void *context;
  FrameListener *next{nullptr};  // set by add_frame_listener()
};

// EMCY frame (CiA 301): error code, error register (0x1001), manufacturer-specific bytes
struct Emergency {
  uint16_t error_code;
  uint8_t error_register;
  std::array<uint8_t, 5> manufacturer_data;
};

// Heartbeat frame: COB-ID 0x700 + node (1..127), 1 byte. Returns false for any other frame.
inline bool decode_heartbeat(uint32_t can_id, const uint8_t *data, size_t len, uint8_t &node, NMTState &state) {
  if (can_id < 0x701 || can_id > 0x77F || len != 1)
    return false;
  node = can_id - 0x700;
  // bit 7 is the node guarding toggle bit, always 0 in a heartbeat
  const auto raw = static_cast<NMTState>(data[0] & 0x7F);
  state = NMTState::NMT_STATE_UNKNOWN;
  for (NMTState known : NMT_STATES) {
    if (known == raw)
      state = raw;
  }
  return true;
}

// EMCY frame: COB-ID 0x80 + node (1..127; 0x80 itself is SYNC), 8 bytes. Returns false for any other frame.
inline bool decode_emcy(uint32_t can_id, const uint8_t *data, size_t len, uint8_t &node, Emergency &emcy) {
  if (can_id < 0x81 || can_id > 0xFF || len != 8)
    return false;
  node = can_id - 0x80;
  emcy.error_code = encode_uint16(data[1], data[0]);
  emcy.error_register = data[2];
  std::memcpy(emcy.manufacturer_data.data(), data + 3, emcy.manufacturer_data.size());
  return true;
}

}  // namespace esphome::canopen
