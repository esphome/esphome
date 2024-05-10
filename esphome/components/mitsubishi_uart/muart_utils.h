#pragma once

namespace esphome {
namespace mitsubishi_uart {

class MUARTUtils {
 public:
  /// Read a string out of data, wordSize bits at a time.
  /// Used to decode serial numbers and other information from a thermostat.
  static std::string DecodeNBitString(const uint8_t data[], size_t dataLength, size_t wordSize) {
    auto resultLength = (dataLength / wordSize) + (dataLength % wordSize != 0);
    auto result = std::string();

    for (int i = 0; i < resultLength; i++) {
      auto bits = BitSlice(data, i * wordSize, ((i + 1) * wordSize) - 1);
      if (bits <= 0x1F)
        bits += 0x40;
      result += (char) bits;
    }

    return result;
  }

  static float TempScaleAToDegC(const uint8_t value) { return (float) (value - 128) / 2.0f; }

  static uint8_t DegCToTempScaleA(const float value) {
    // Special cases
    if (value < -64)
      return 0;
    if (value > 63.5f)
      return 0xFF;

    return (uint8_t) round(value * 2) + 128;
  }

  static float LegacyTargetTempToDegC(const uint8_t value) {
    return ((float) (31 - (value & 0x0F)) + (((value & 0xF0) > 0) ? 0.5f : 0));
  }

  static uint8_t DegCToLegacyTargetTemp(const float value) {
    // Special cases per docs
    if (value < 16)
      return 0x0F;
    if (value > 31.5)
      return 0x10;

    return ((31 - (uint8_t) value) & 0xF) + (((int) (value * 2) % 2) << 4);
  }

  static float LegacyRoomTempToDegC(const uint8_t value) { return (float) value + 10; }

  static uint8_t DegCToLegacyRoomTemp(const float value) {
    if (value < 10)
      return 0x00;
    if (value > 41)
      return 0x1F;

    return (uint8_t) value - 10;
  }

 private:
  /// Extract the specified bits (inclusive) from an arbitrarily-sized byte array. Does not perform bounds checks.
  static uint64_t BitSlice(const uint8_t ds[], size_t start, size_t end) {
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
