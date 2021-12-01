#include "midea_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.midea";

uint8_t MideaData::calc_cs_() const {
  uint8_t cs = 0;
  for (unsigned idx = 0; idx < OFFSET_CS; idx++)
    cs -= reverse_bits_8(this->data_[idx]);
  return reverse_bits_8(cs);
}

bool MideaData::is_compliment(const MideaData &rhs) const {
  return std::equal(this->data_.begin(), this->data_.end(), rhs.data_.begin(), [](const uint8_t &a, const uint8_t &b){
    return a + b == 255;
  });
}

void MideaProtocol::encode(RemoteTransmitData *dst, const MideaData &src) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 48 * 2 + 2 + 2 + 48 * 2 + 1);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  for (unsigned idx = 0; idx < 6; idx++)
    for (uint8_t mask = 1 << 7; mask; mask >>= 1)
      dst->item(BIT_HIGH_US, (src[idx] & mask) ? BIT_ONE_LOW_US : BIT_ZERO_LOW_US);
  dst->item(BIT_HIGH_US, MIN_GAP_US);
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  for (unsigned idx = 0; idx < 6; idx++)
    for (uint8_t mask = 1 << 7; mask; mask >>= 1)
      dst->item(BIT_HIGH_US, (src[idx] & mask) ? BIT_ZERO_LOW_US : BIT_ONE_LOW_US);
  dst->mark(BIT_HIGH_US);
}

bool MideaProtocol::read_data(RemoteReceiveData &src, MideaData &data) {
  for (unsigned idx = 0; idx < 6; idx++) {
    for (uint8_t mask = 1 << 7; mask; mask >>= 1) {
      if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US))
        data[idx] |= mask;
      else if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US))
        return false;
    }
  }
  return true;
}

optional<MideaData> MideaProtocol::decode(RemoteReceiveData src) {
  MideaData out, inv;
  if (src.expect_item(HEADER_HIGH_US, HEADER_LOW_US) && MideaProtocol::read_data(src, out) && src.expect_item(BIT_HIGH_US, MIN_GAP_US) &&
      src.expect_item(HEADER_HIGH_US, HEADER_LOW_US) && MideaProtocol::read_data(src, inv) && src.expect_mark(BIT_HIGH_US) &&
      out.is_valid() && out.is_compliment(inv))
    return out;
  return {};
}

void MideaProtocol::dump(const MideaData &data) { ESP_LOGD(TAG, "Received Midea: %s", data.to_string().c_str()); }

}  // namespace remote_base
}  // namespace esphome
