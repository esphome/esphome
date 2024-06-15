#pragma once

namespace esphome {
namespace mitsubishi_uart {

class MUARTUtils {
 public:
  /// Read a string out of data, wordSize bits at a time.
  /// Used to decode serial numbers and other information from a thermostat.
  static std::string decode_n_bit_string(const uint8_t data[], size_t data_length, size_t word_size) {
    auto result_length = (data_length / word_size) + (data_length % word_size != 0);
    auto result = std::string();

    for (int i = 0; i < result_length; i++) {
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
  static uint64_t bit_slice(const uint8_t ds[], size_t start, size_t end) {
    // Lazies! https://stackoverflow.com/a/25297870/1817097
    uint64_t s = 0;
    size_t i, n = (end - 1) / 8;
    for (i = 0; i <= n; ++i)
      s = (s << 8) + ds[i];
    s >>= (n + 1) * 8 - end;
    uint64_t mask = (((uint64_t) 1) << (end - start + 1)) - 1;  // len = end - start + 1
    s &= mask;
    return s;
  }
};

}  // namespace mitsubishi_uart
}  // namespace esphome
