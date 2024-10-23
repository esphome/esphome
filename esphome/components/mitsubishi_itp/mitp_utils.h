#pragma once

namespace esphome {
namespace mitsubishi_itp {

class MITPUtils {
 public:
  /// Read a string out of data, wordSize bits at a time.
  /// Used to decode serial numbers and other information from a thermostat.
  static std::string decode_n_bit_string(const uint8_t data[], size_t data_length, size_t word_size = 6) {
    auto result = std::string();

    for (int i = 0; i < data_length; i++) {
      auto bits = bit_slice(data, i * word_size, ((i + 1) * word_size) - 1);
      if (bits <= 0x1F)
        bits += 0x40;
      result += (char) bits;
    }

    return result;
  }

  static float temp_scale_a_to_deg_c(const uint8_t value) { return (float) (value - 128) / 2.0f; }

  static uint8_t deg_c_to_temp_scale_a(const float value) {
    // Special cases
    if (value < -64)
      return 0;
    if (value > 63.5f)
      return 0xFF;

    return (uint8_t) round(value * 2) + 128;
  }

  static float legacy_target_temp_to_deg_c(const uint8_t value) {
    return ((float) (31 - (value & 0x0F)) + (((value & 0xF0) > 0) ? 0.5f : 0));
  }

  static uint8_t deg_c_to_legacy_target_temp(const float value) {
    // Special cases per docs
    if (value < 16)
      return 0x0F;
    if (value > 31.5)
      return 0x10;

    return ((31 - (uint8_t) value) & 0xF) + (((int) (value * 2) % 2) << 4);
  }

  static float legacy_room_temp_to_deg_c(const uint8_t value) { return (float) value + 10; }

  static uint8_t deg_c_to_legacy_room_temp(const float value) {
    if (value < 10)
      return 0x00;
    if (value > 41)
      return 0x1F;

    return (uint8_t) value - 10;
  }

 private:
  /// Extract the specified bits (inclusive) from an arbitrarily-sized byte array. Does not perform bounds checks.
  /// Max extraction is 64 bits. Preserves endianness of incoming data stream.
  static uint64_t bit_slice(const uint8_t ds[], size_t start, size_t end) {
    if ((end - start) >= 64)
      return 0;

    uint64_t result = 0;

    size_t start_byte = (start) / 8;
    size_t end_byte = ((end) / 8) + 1;  // exclusive, used for length calc

    // raw copy the relevant bytes into our int64, preserving endian-ness
    std::memcpy(&result, &ds[start_byte], end_byte - start_byte);
    result = byteswap(result);

    // shift out the bits we don't want from the end (64 + credit any pre-sliced bits)
    result >>= (sizeof(uint64_t) * 8) + (start_byte * 8) - end - 1;

    // mask out the number of bits we want
    result &= (1 << (end - start + 1)) - 1;

    return result;
  }
};

}  // namespace mitsubishi_itp
}  // namespace esphome
