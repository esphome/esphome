#include "ira211_protocol.h"
#include "esphome/core/log.h"
#include <cstdlib>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.ira211";

static constexpr uint32_t T_US = 800;
static constexpr uint32_t CARRIER_FREQ = 38000;
static constexpr uint32_t PREAMBLE_MARK1 = 7600;
static constexpr uint32_t PREAMBLE_SPACE1 = 800;
static constexpr uint32_t PREAMBLE_MARK2 = 800;
static constexpr uint32_t PREAMBLE_SPACE2 = 7600;

void IRA211Protocol::encode(RemoteTransmitData *dst, const IRA211Data &data) {
  dst->set_carrier_frequency(CARRIER_FREQ);

  uint8_t num_bits = data.get_frame_bits();
  if (num_bits == 0)
    return;

  // Worst case: each bit alternates → num_bits mark/space pairs + 4 preamble + 1 trailing
  dst->reserve(4 + num_bits + 1);

  // Preamble
  dst->mark(PREAMBLE_MARK1);
  dst->space(PREAMBLE_SPACE1);
  dst->mark(PREAMBLE_MARK2);
  dst->space(PREAMBLE_SPACE2);

  // NRZ: scan runs of identical bits, emit mark (1s) or space (0s) of run_count * T
  const uint8_t *frame = data.get_frame_data();
  uint8_t pos = 0;
  while (pos < num_bits) {
    bool bit_val = (frame[pos >> 3] >> (7 - (pos & 7))) & 1;
    uint8_t run = 1;
    while (pos + run < num_bits) {
      bool next = (frame[(pos + run) >> 3] >> (7 - ((pos + run) & 7))) & 1;
      if (next != bit_val)
        break;
      run++;
    }
    if (bit_val) {
      dst->mark(run * T_US);
    } else {
      dst->space(run * T_US);
    }
    pos += run;
  }

  // Trailing mark to end the last space (standard IR convention)
  // Only needed if the frame ends with a space (last bit = 0, which it always does: boundary bit)
  dst->mark(T_US);
}

optional<IRA211Data> IRA211Protocol::decode(RemoteReceiveData src) {
  // Validate preamble: 7600µs mark, 800µs space, 800µs mark, 7600µs space
  if (!src.expect_mark(PREAMBLE_MARK1))
    return {};
  if (!src.expect_space(PREAMBLE_SPACE1))
    return {};
  if (!src.expect_mark(PREAMBLE_MARK2))
    return {};
  if (!src.expect_space(PREAMBLE_SPACE2))
    return {};

  // Convert remaining NRZ mark/space timings to a packed bitstream.
  // Marks → 1-bits, spaces → 0-bits. Duration / T gives bit count.
  // We decode in two passes: first 20 bits (2 packets = device ID + command)
  // to determine the expected frame length, then read exactly that many bits.
  std::array<uint8_t, IRA211_MAX_FRAME_BYTES> bitstream{};
  uint8_t bit_pos = 0;
  uint8_t target_bits = IRA211_MAX_FRAME_BITS;  // initial limit, refined after 20 bits

  while (src.is_valid() && bit_pos < target_bits) {
    int32_t raw = src.peek();
    bool is_mark = raw > 0;
    uint32_t duration = static_cast<uint32_t>(std::abs(raw));

    // A duration >= 7600µs (preamble-length gap) after data starts means end-of-frame
    if (bit_pos > 0 && duration >= PREAMBLE_MARK1)
      break;

    // Round to nearest multiple of T
    uint8_t count = static_cast<uint8_t>((duration + T_US / 2) / T_US);
    if (count == 0)
      break;

    for (uint8_t i = 0; i < count && bit_pos < target_bits; i++) {
      if (is_mark) {
        bitstream[bit_pos >> 3] |= (1 << (7 - (bit_pos & 7)));
      }
      bit_pos++;
    }

    src.advance();

    // After 20 bits (2 packets), we can determine the expected frame length
    if (bit_pos >= 20 && target_bits == IRA211_MAX_FRAME_BITS) {
      // Extract command from packet 1 (bits 11..18)
      uint8_t cmd_wire = 0;
      for (uint8_t i = 0; i < 8; i++) {
        cmd_wire = (cmd_wire << 1) | ((bitstream[(10 + 1 + i) >> 3] >> (7 - ((10 + 1 + i) & 7))) & 1);
      }
      auto cmd = static_cast<IRA211Command>(IRA211Data::wire_decode_public(cmd_wire));
      uint8_t expected = IRA211Data::packet_count_public(cmd);
      if (expected > 0) {
        target_bits = expected * 10;
      }
    }
  }

  // Must be a valid 10-bit-aligned frame
  if (bit_pos < 40 || bit_pos % 10 != 0)
    return {};

  IRA211Data data(bitstream.data(), bit_pos);
  if (!data.is_valid()) {
    ESP_LOGV(TAG, "Received invalid IRA211 frame (%u bits)", bit_pos);
    return {};
  }

  return data;
}

void IRA211Protocol::dump(const IRA211Data &data) {
  const char *cmd_name;
  switch (data.get_command()) {
    case IRA211Command::TEMP_UP:
      cmd_name = "TEMP_UP";
      break;
    case IRA211Command::TEMP_DOWN:
      cmd_name = "TEMP_DOWN";
      break;
    case IRA211Command::MODE:
      cmd_name = "MODE";
      break;
    case IRA211Command::FAN:
      cmd_name = "FAN";
      break;
    case IRA211Command::POWER:
      cmd_name = "POWER";
      break;
    case IRA211Command::SYNC:
      cmd_name = "SYNC";
      break;
    default:
      cmd_name = "UNKNOWN";
      break;
  }

  switch (data.get_command()) {
    case IRA211Command::SYNC:
    case IRA211Command::POWER: {
      const char *mode_name;
      switch (data.get_mode()) {
        case IRA211Mode::PROTECTION:
          mode_name = "Protection";
          break;
        case IRA211Mode::TIMER:
          mode_name = "Timer";
          break;
        case IRA211Mode::COMFORT:
          mode_name = "Comfort";
          break;
        default:
          mode_name = "Unknown";
          break;
      }
      const char *fan_name;
      switch (data.get_fan()) {
        case IRA211Fan::FAN_AUTO:
          fan_name = "Auto";
          break;
        case IRA211Fan::FAN_LOW:
          fan_name = "1/3";
          break;
        case IRA211Fan::FAN_MEDIUM:
          fan_name = "2/3";
          break;
        case IRA211Fan::FAN_HIGH:
          fan_name = "3/3";
          break;
        default:
          fan_name = "Unknown";
          break;
      }
      ESP_LOGD(TAG, "Received IRA211: %s Temp=%u.%u°C Mode=%s Fan=%s", cmd_name, data.get_temperature(),
               data.get_temp_tenths(), mode_name, fan_name);
      break;
    }
    case IRA211Command::TEMP_UP:
    case IRA211Command::TEMP_DOWN:
      ESP_LOGD(TAG, "Received IRA211: %s Temp=%u.%u°C", cmd_name, data.get_temperature(), data.get_temp_tenths());
      break;
    case IRA211Command::FAN: {
      const char *fan_name;
      switch (data.get_fan()) {
        case IRA211Fan::FAN_AUTO:
          fan_name = "Auto";
          break;
        case IRA211Fan::FAN_LOW:
          fan_name = "1/3";
          break;
        case IRA211Fan::FAN_MEDIUM:
          fan_name = "2/3";
          break;
        case IRA211Fan::FAN_HIGH:
          fan_name = "3/3";
          break;
        default:
          fan_name = "Unknown";
          break;
      }
      ESP_LOGD(TAG, "Received IRA211: %s Fan=%s", cmd_name, fan_name);
      break;
    }
    case IRA211Command::MODE: {
      const char *mode_name;
      switch (data.get_mode()) {
        case IRA211Mode::PROTECTION:
          mode_name = "Protection";
          break;
        case IRA211Mode::TIMER:
          mode_name = "Timer";
          break;
        case IRA211Mode::COMFORT:
          mode_name = "Comfort";
          break;
        default:
          mode_name = "Unknown";
          break;
      }
      ESP_LOGD(TAG, "Received IRA211: %s Mode=%s", cmd_name, mode_name);
      break;
    }
    default:
      ESP_LOGD(TAG, "Received IRA211: %s", cmd_name);
      break;
  }
}

}  // namespace remote_base
}  // namespace esphome
