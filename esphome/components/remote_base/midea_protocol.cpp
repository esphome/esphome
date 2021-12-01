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

bool MideaData::is_compliment(const MideaData &rhs) const {
  return std::equal(this->data_.begin(), this->data_.end(), rhs.data_.begin(), [](const uint8_t &a, const uint8_t &b){
    return a + b == 255;
  });
}

void MideaProtocol::data(RemoteTransmitData *dst, const MideaData &src, bool compliment) {
  for (const uint8_t *it = src.data(); it != src.data() + src.size(); ++it) {
    const uint8_t data = compliment ? ~(*it) : *it;
    for (uint8_t mask = 1 << 7; mask; mask >>= 1) {
      if (data & mask)
        MideaProtocol::one(dst);
      else
        MideaProtocol::zero(dst);
    }
  }
}

void MideaProtocol::encode(RemoteTransmitData *dst, const MideaData &src) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 48 * 2 + 2 + 2 + 48 * 2 + 1);
  MideaProtocol::header(dst);
  MideaProtocol::data(dst, src);
  MideaProtocol::footer(dst);
  MideaProtocol::header(dst);
  MideaProtocol::data(dst, src, true);
  dst->mark(BIT_HIGH_US);
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
      MideaProtocol::expect_header(src) && MideaProtocol::expect_data(src, inv) && src.expect_mark(BIT_HIGH_US) &&
      out.is_valid() && out.is_compliment(inv))
    return out;
  return {};
}

void MideaProtocol::dump(const MideaData &data) { ESP_LOGD(TAG, "Received Midea: %s", data.to_string().c_str()); }

}  // namespace remote_base
}  // namespace esphome
