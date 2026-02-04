#include "symphony_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.symphony";

// Reference implementation and timing details:
// IRremoteESP8266 ir_Symphony.cpp
// https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Symphony.cpp
// The implementation below mirrors the constant bit-time mapping and
// footer-gap handling used there.

// Symphony protocol timing specifications (tuned to handset captures)
static const uint32_t BIT_ZERO_HIGH_US = 460;  // short
static const uint32_t BIT_ZERO_LOW_US = 1260;  // long
static const uint32_t BIT_ONE_HIGH_US = 1260;  // long
static const uint32_t BIT_ONE_LOW_US = 460;    // short
static const uint32_t CARRIER_FREQUENCY = 38000;

// IRremoteESP8266 reference: kSymphonyFooterGap = 4 * (mark + space)
static const uint32_t FOOTER_GAP_US = 4 * (BIT_ZERO_HIGH_US + BIT_ZERO_LOW_US);
// Typical inter-frame gap (~34.8 ms observed)
static const uint32_t INTER_FRAME_GAP_US = 34760;

void SymphonyProtocol::encode(RemoteTransmitData *dst, const SymphonyData &data) {
  dst->set_carrier_frequency(CARRIER_FREQUENCY);
  ESP_LOGD(TAG, "Sending Symphony: data=0x%0*X nbits=%u repeats=%u", (data.nbits + 3) / 4, (uint32_t) data.data,
           data.nbits, data.repeats);
  // Each bit produces a mark+space (2 entries). We fold the inter-frame/footer gap
  // into the last bit's space of each frame to avoid over-length gaps.
  dst->reserve(data.nbits * 2u * data.repeats);

  for (uint8_t repeats = 0; repeats < data.repeats; repeats++) {
    // Data bits (MSB first)
    for (uint32_t mask = 1UL << (data.nbits - 1); mask != 0; mask >>= 1) {
      const bool is_last_bit = (mask == 1);
      const bool is_last_frame = (repeats == (data.repeats - 1));
      if (is_last_bit) {
        // Emit last bit's mark; replace its space with the proper gap
        if (data.data & mask) {
          dst->mark(BIT_ONE_HIGH_US);
        } else {
          dst->mark(BIT_ZERO_HIGH_US);
        }
        dst->space(is_last_frame ? FOOTER_GAP_US : INTER_FRAME_GAP_US);
      } else {
        if (data.data & mask) {
          dst->item(BIT_ONE_HIGH_US, BIT_ONE_LOW_US);
        } else {
          dst->item(BIT_ZERO_HIGH_US, BIT_ZERO_LOW_US);
        }
      }
    }
  }
}

optional<SymphonyData> SymphonyProtocol::decode(RemoteReceiveData src) {
  auto is_valid_len = [](uint8_t nbits) -> bool { return nbits == 8 || nbits == 12 || nbits == 16; };

  RemoteReceiveData s = src;  // copy
  SymphonyData out{0, 0, 1};

  for (; out.nbits < 32; out.nbits++) {
    if (s.expect_mark(BIT_ONE_HIGH_US)) {
      if (!s.expect_space(BIT_ONE_LOW_US)) {
        // Allow footer gap immediately after the last mark
        if (s.peek_space_at_least(FOOTER_GAP_US)) {
          uint8_t bits_with_this = out.nbits + 1;
          if (is_valid_len(bits_with_this)) {
            out.data = (out.data << 1UL) | 1UL;
            out.nbits = bits_with_this;
            return out;
          }
        }
        return {};
      }
      // Successfully consumed a '1' bit (mark + space)
      out.data = (out.data << 1UL) | 1UL;
      continue;
    } else if (s.expect_mark(BIT_ZERO_HIGH_US)) {
      if (!s.expect_space(BIT_ZERO_LOW_US)) {
        // Allow footer gap immediately after the last mark
        if (s.peek_space_at_least(FOOTER_GAP_US)) {
          uint8_t bits_with_this = out.nbits + 1;
          if (is_valid_len(bits_with_this)) {
            out.data = (out.data << 1UL) | 0UL;
            out.nbits = bits_with_this;
            return out;
          }
        }
        return {};
      }
      // Successfully consumed a '0' bit (mark + space)
      out.data = (out.data << 1UL) | 0UL;
      continue;
    } else {
      // Completed a valid-length frame followed by a footer gap
      if (is_valid_len(out.nbits) && s.peek_space_at_least(FOOTER_GAP_US)) {
        return out;
      }
      return {};
    }
  }

  if (is_valid_len(out.nbits) && s.peek_space_at_least(FOOTER_GAP_US)) {
    return out;
  }

  return {};
}

void SymphonyProtocol::dump(const SymphonyData &data) {
  const int32_t hex_width = (data.nbits + 3) / 4;  // pad to nibble width
  ESP_LOGI(TAG, "Received Symphony: data=0x%0*X, nbits=%d", hex_width, (uint32_t) data.data, data.nbits);
}

}  // namespace remote_base
}  // namespace esphome
