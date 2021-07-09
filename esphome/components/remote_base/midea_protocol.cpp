#include "midea_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.midea";

// Reverse bits in byte
static uint8_t s_reverse(uint8_t data) {
  static const uint8_t PROGMEM TABLE[] = {
      0b0000, 0b1000, 0b0100, 0b1100, 0b0010, 0b1010, 0b0110, 0b1110,
      0b0001, 0b1001, 0b0101, 0b1101, 0b0011, 0b1011, 0b0111, 0b1111,
  };
  return pgm_read_byte(TABLE + data % 16) * 16 + pgm_read_byte(TABLE + data / 16);
}

uint8_t MideaData::calc_cs_() const {
  uint8_t sum = 0;
  for (const uint8_t *p = this->data(); p != this->data() + OFFSET_CS; ++p)
    sum -= s_reverse(*p);
  return s_reverse(sum);
}

static char u4hex(uint8_t num) { return num + ((num < 10) ? '0' : ('A' - 10)); }

String MideaData::raw_data() const {
  String ret;
  ret.reserve(this->size() * 3);
  for (const uint8_t *it = this->data();;) {
    ret += u4hex(*it >> 4);
    ret += u4hex(*it++ & 15);
    if (it == this->data() + this->size())
      return ret;
    ret += ' ';
  }
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

void MideaProtocol::dump(const MideaData &data) { ESP_LOGD(TAG, "Received Midea: %s", data.raw_data().c_str()); }

}  // namespace remote_base
}  // namespace esphome
