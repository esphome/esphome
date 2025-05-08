#pragma once

#include "proto.h"
#include <cstdint>
#include <string>

namespace esphome {
namespace api {

class ProtoSize {
 public:
  /**
   * @brief ProtoSize class for Protocol Buffer serialization size calculation
   *
   * This class provides static methods to calculate the exact byte counts needed
   * for encoding various Protocol Buffer field types. All methods are designed to be
   * efficient for the common case where many fields have default values.
   *
   * Implements Protocol Buffer encoding size calculation according to:
   * https://protobuf.dev/programming-guides/encoding/
   *
   * Key features:
   * - Early-return optimization for zero/default values
   * - Direct total_size updates to avoid unnecessary additions
   * - Specialized handling for different field types according to protobuf spec
   * - Templated helpers for repeated fields and messages
   */

  /**
   * @brief Calculates the size in bytes needed to encode a uint32_t value as a varint
   *
   * @param value The uint32_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(uint32_t value) {
    // Optimized varint size calculation using leading zeros
    // Each 7 bits requires one byte in the varint encoding
    if (value < 128)
      return 1;  // 7 bits, common case for small values

    // For larger values, count bytes needed based on the position of the highest bit set
    if (value < 16384) {
      return 2;  // 14 bits
    } else if (value < 2097152) {
      return 3;  // 21 bits
    } else if (value < 268435456) {
      return 4;  // 28 bits
    } else {
      return 5;  // 32 bits (maximum for uint32_t)
    }
  }

  /**
   * @brief Calculates the size in bytes needed to encode a uint64_t value as a varint
   *
   * @param value The uint64_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(uint64_t value) {
    // Handle common case of values fitting in uint32_t (vast majority of use cases)
    if (value <= UINT32_MAX) {
      return varint(static_cast<uint32_t>(value));
    }

    // For larger values, determine size based on highest bit position
    if (value < (1ULL << 35)) {
      return 5;  // 35 bits
    } else if (value < (1ULL << 42)) {
      return 6;  // 42 bits
    } else if (value < (1ULL << 49)) {
      return 7;  // 49 bits
    } else if (value < (1ULL << 56)) {
      return 8;  // 56 bits
    } else if (value < (1ULL << 63)) {
      return 9;  // 63 bits
    } else {
      return 10;  // 64 bits (maximum for uint64_t)
    }
  }

  /**
   * @brief Calculates the size in bytes needed to encode an int32_t value as a varint
   *
   * Special handling is needed for negative values, which are sign-extended to 64 bits
   * in Protocol Buffers, resulting in a 10-byte varint.
   *
   * @param value The int32_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(int32_t value) {
    // Negative values are sign-extended to 64 bits in protocol buffers,
    // which always results in a 10-byte varint for negative int32
    if (value < 0) {
      return 10;  // Negative int32 is always 10 bytes long
    }
    // For non-negative values, use the uint32_t implementation
    return varint(static_cast<uint32_t>(value));
  }

  /**
   * @brief Calculates the size in bytes needed to encode an int64_t value as a varint
   *
   * @param value The int64_t value to calculate size for
   * @return The number of bytes needed to encode the value
   */
  static inline uint32_t varint(int64_t value) {
    // For int64_t, we convert to uint64_t and calculate the size
    // This works because the bit pattern determines the encoding size,
    // and we've handled negative int32 values as a special case above
    return varint(static_cast<uint64_t>(value));
  }

  /**
   * @brief Calculates the size in bytes needed to encode a field ID and wire type
   *
   * @param field_id The field identifier
   * @param type The wire type value (from the WireType enum in the protobuf spec)
   * @return The number of bytes needed to encode the field ID and wire type
   */
  static inline uint32_t field(uint32_t field_id, uint32_t type) {
    uint32_t tag = (field_id << 3) | (type & 0b111);
    return varint(tag);
  }

  /**
   * @brief Common parameters for all add_*_field methods
   *
   * All add_*_field methods follow these common patterns:
   *
   * @param total_size Reference to the total message size to update
   * @param field_id_size Pre-calculated size of the field ID in bytes
   * @param value The value to calculate size for (type varies)
   * @param force Whether to calculate size even if the value is default/zero/empty
   *
   * Each method follows this implementation pattern:
   * 1. Skip calculation if value is default (0, false, empty) and not forced
   * 2. Calculate the size based on the field's encoding rules
   * 3. Add the field_id_size + calculated value size to total_size
   */

  /**
   * @brief Calculates and adds the size of an int32 field to the total message size
   */
  static inline void add_int32_field(uint32_t &total_size, uint32_t field_id_size, int32_t value, bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    if (value < 0) {
      // Negative values are encoded as 10-byte varints in protobuf
      total_size += field_id_size + 10;
    } else {
      // For non-negative values, use the standard varint size
      total_size += field_id_size + varint(static_cast<uint32_t>(value));
    }
  }

  /**
   * @brief Calculates and adds the size of a uint32 field to the total message size
   */
  static inline void add_uint32_field(uint32_t &total_size, uint32_t field_id_size, uint32_t value,
                                      bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a boolean field to the total message size
   */
  static inline void add_bool_field(uint32_t &total_size, uint32_t field_id_size, bool value, bool force = false) {
    // Skip calculation if value is false and not forced
    if (!value && !force) {
      return;  // No need to update total_size
    }

    // Boolean fields always use 1 byte when true
    total_size += field_id_size + 1;
  }

  /**
   * @brief Calculates and adds the size of a fixed field to the total message size
   *
   * Fixed fields always take exactly N bytes (4 for fixed32/float, 8 for fixed64/double).
   *
   * @tparam NumBytes The number of bytes for this fixed field (4 or 8)
   * @param is_nonzero Whether the value is non-zero
   */
  template<uint32_t NumBytes>
  static inline void add_fixed_field(uint32_t &total_size, uint32_t field_id_size, bool is_nonzero,
                                     bool force = false) {
    // Skip calculation if value is zero and not forced
    if (!is_nonzero && !force) {
      return;  // No need to update total_size
    }

    // Fixed fields always take exactly NumBytes
    total_size += field_id_size + NumBytes;
  }

  /**
   * @brief Calculates and adds the size of an enum field to the total message size
   *
   * Enum fields are encoded as uint32 varints.
   */
  static inline void add_enum_field(uint32_t &total_size, uint32_t field_id_size, uint32_t value, bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Enums are encoded as uint32
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a sint32 field to the total message size
   *
   * Sint32 fields use ZigZag encoding, which is more efficient for negative values.
   */
  static inline void add_sint32_field(uint32_t &total_size, uint32_t field_id_size, int32_t value, bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // ZigZag encoding for sint32: (n << 1) ^ (n >> 31)
    uint32_t zigzag = (static_cast<uint32_t>(value) << 1) ^ (static_cast<uint32_t>(value >> 31));
    total_size += field_id_size + varint(zigzag);
  }

  /**
   * @brief Calculates and adds the size of an int64 field to the total message size
   */
  static inline void add_int64_field(uint32_t &total_size, uint32_t field_id_size, int64_t value, bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a uint64 field to the total message size
   */
  static inline void add_uint64_field(uint32_t &total_size, uint32_t field_id_size, uint64_t value,
                                      bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    total_size += field_id_size + varint(value);
  }

  /**
   * @brief Calculates and adds the size of a sint64 field to the total message size
   *
   * Sint64 fields use ZigZag encoding, which is more efficient for negative values.
   */
  static inline void add_sint64_field(uint32_t &total_size, uint32_t field_id_size, int64_t value, bool force = false) {
    // Skip calculation if value is zero and not forced
    if (value == 0 && !force) {
      return;  // No need to update total_size
    }

    // ZigZag encoding for sint64: (n << 1) ^ (n >> 63)
    uint64_t zigzag = (static_cast<uint64_t>(value) << 1) ^ (static_cast<uint64_t>(value >> 63));
    total_size += field_id_size + varint(zigzag);
  }

  /**
   * @brief Calculates and adds the size of a string/bytes field to the total message size
   */
  static inline void add_string_field(uint32_t &total_size, uint32_t field_id_size, const std::string &str,
                                      bool force = false) {
    // Skip calculation if string is empty and not forced
    if (str.empty() && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    const uint32_t str_size = static_cast<uint32_t>(str.size());
    total_size += field_id_size + varint(str_size) + str_size;
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size
   *
   * This helper function directly updates the total_size reference if the nested size
   * is greater than zero or force is true.
   *
   * @param nested_size The pre-calculated size of the nested message
   */
  static inline void add_message_field(uint32_t &total_size, uint32_t field_id_size, uint32_t nested_size,
                                       bool force = false) {
    // Skip calculation if nested message is empty and not forced
    if (nested_size == 0 && !force) {
      return;  // No need to update total_size
    }

    // Calculate and directly add to total_size
    // Field ID + length varint + nested message content
    total_size += field_id_size + varint(nested_size) + nested_size;
  }

  /**
   * @brief Calculates and adds the size of a nested message field to the total message size
   *
   * This templated version directly takes a message object, calculates its size internally,
   * and updates the total_size reference. This eliminates the need for a temporary variable
   * at the call site.
   *
   * @tparam MessageType The type of the nested message (inferred from parameter)
   * @param message The nested message object
   */
  template<typename MessageType>
  static inline void add_message_object(uint32_t &total_size, uint32_t field_id_size, const MessageType &message,
                                        bool force = false) {
    uint32_t nested_size = 0;
    message.calculate_size(nested_size);

    // Use the base implementation with the calculated nested_size
    add_message_field(total_size, field_id_size, nested_size, force);
  }

  /**
   * @brief Calculates and adds the sizes of all messages in a repeated field to the total message size
   *
   * This helper processes a vector of message objects, calculating the size for each message
   * and adding it to the total size.
   *
   * @tparam MessageType The type of the nested messages in the vector
   * @param messages Vector of message objects
   */
  template<typename MessageType>
  static inline void add_repeated_message(uint32_t &total_size, uint32_t field_id_size,
                                          const std::vector<MessageType> &messages) {
    // Skip if the vector is empty
    if (messages.empty()) {
      return;
    }

    // For repeated fields, always use force=true
    for (const auto &message : messages) {
      add_message_object(total_size, field_id_size, message, true);
    }
  }
};

}  // namespace api
}  // namespace esphome
