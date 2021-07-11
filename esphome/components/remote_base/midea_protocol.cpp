#include "midea_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.midea";

uint8_t MideaData::calc_cs_() const {
  uint8_t cs = 0;
  for (const uint8_t *it = this->data(); it != this->data() + OFFSET_CS; ++it)
    cs -= reverse_bits_8(*it);
  return reverse_bits_8(cs);
}

bool MideaData::check_compliment(const MideaData &rhs) const {
  const uint8_t *it0 = rhs.data();
  for (const uint8_t *it1 = this->data(); it1 != this->data() + this->size(); ++it0, ++it1) {
    if (*it0 != ~(*it1))
      return false;
  }
  return true;
}

void MideaProtocol::data(RemoteTransmitData *dst, const MideaData &src, bool compliment) {
  for (const uint8_t *it = src.data(); it != src.data() + src.size(); ++it) {
    const uint8_t data = compliment ? ~(*it) : *it;
    for (uint8_t mask = 128; mask; mask >>= 1) {
      if (data & mask)
        one(dst);
      else
        zero(dst);
    }
  }
}

void MideaProtocol::encode(RemoteTransmitData *dst, const MideaData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 48 * 2 + 2 + 2 + 48 * 2 + 2);
  MideaProtocol::header(dst);
  MideaProtocol::data(dst, data);
  MideaProtocol::footer(dst);
  MideaProtocol::header(dst);
  MideaProtocol::data(dst, data, true);
  MideaProtocol::footer(dst);
}

bool MideaProtocol::expect_one(RemoteReceiveData &src) {
  if (!src.peek_item(BIT_HIGH_US, BIT_ONE_LOW_US))
    return false;
  src.advance(2);
  return true;
}

bool MideaProtocol::expect_zero(RemoteReceiveData &src) {
  if (!src.peek_item(BIT_HIGH_US, BIT_ZERO_LOW_US))
    return false;
  src.advance(2);
  return true;
}

bool MideaProtocol::expect_header(RemoteReceiveData &src) {
  if (!src.peek_item(HEADER_HIGH_US, HEADER_LOW_US))
    return false;
  src.advance(2);
  return true;
}

bool MideaProtocol::expect_footer(RemoteReceiveData &src) {
  if (!src.peek_item(BIT_HIGH_US, MIN_GAP_US))
    return false;
  src.advance(2);
  return true;
}

bool MideaProtocol::expect_data(RemoteReceiveData &src, MideaData &out) {
  for (uint8_t *dst = out.data(); dst != out.data() + out.size(); ++dst) {
    for (uint8_t mask = 128; mask; mask >>= 1) {
      if (MideaProtocol::expect_one(src))
        *dst |= mask;
      else if (!MideaProtocol::expect_zero(src))
        return false;
    }
  }
  return true;
}

optional<MideaData> MideaProtocol::decode(RemoteReceiveData src) {
  MideaData out, inv;
  if (MideaProtocol::expect_header(src) && MideaProtocol::expect_data(src, out) && MideaProtocol::expect_footer(src) &&
      out.is_valid() && MideaProtocol::expect_data(src, inv) && out.check_compliment(inv))
    return out;
  return {};
}

void MideaProtocol::dump(const MideaData &data) { ESP_LOGD(TAG, "Received Midea: %s", data.to_string().c_str()); }

}  // namespace remote_base
}  // namespace esphome
