#include "midea_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.midea";

static const uint32_t TICK_US = 560;
static const uint32_t HEADER_MARK_US = 8 * TICK_US;
static const uint32_t HEADER_SPACE_US = 8 * TICK_US;
static const uint32_t BIT_MARK_US = 1 * TICK_US;
static const uint32_t BIT_ONE_SPACE_US = 3 * TICK_US;
static const uint32_t BIT_ZERO_SPACE_US = 1 * TICK_US;
static const uint32_t FOOTER_MARK_US = 1 * TICK_US;
static const uint32_t FOOTER_SPACE_US = 10 * TICK_US;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * 48 + 2 + 2 + 2 * 48 + 2;

uint8_t MideaData::calc_cs_() const {
  uint8_t cs = 0;
  for (uint8_t idx = 0; idx < OFFSET_CS; idx++)
    cs -= reverse_bits(this->data_[idx]);
  return reverse_bits(cs);
}

bool MideaData::is_compliment(const MideaData &rhs) const {
  return std::equal(this->data_.begin(), this->data_.end(), rhs.data_.begin(),
                    [](const uint8_t &a, const uint8_t &b) { return a + b == 255; });
}

void MideaProtocol::encode(RemoteTransmitData *dst, const MideaData &src) {
  dst->set_carrier_frequency(38000);
  dst->reserve(REMOTE_DATA_SIZE);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  for (unsigned idx = 0; idx < 6; idx++)
    msb::encode<8, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, src[idx]);
  dst->item(FOOTER_MARK_US, FOOTER_SPACE_US);
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  for (unsigned idx = 0; idx < 6; idx++)
    msb::encode<8, BIT_MARK_US, BIT_ZERO_SPACE_US, BIT_ONE_SPACE_US>(dst, src[idx]);
  dst->mark(FOOTER_MARK_US);
}

static bool decode_data(RemoteReceiveData &src, MideaData &dst) {
  for (unsigned idx = 0; idx < 6; idx++) {
    uint8_t data = 0;
    if (!msb::decode<8, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, data))
      return false;
    dst[idx] = data;
  }
  return true;
}

optional<MideaData> MideaProtocol::decode(RemoteReceiveData src) {
  MideaData first, second;
  if (src.has_size(REMOTE_DATA_SIZE) && src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) && decode_data(src, first) &&
      first.is_valid() && src.expect_item(FOOTER_MARK_US, FOOTER_SPACE_US) &&
      src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) && decode_data(src, second) && src.expect_mark(FOOTER_MARK_US) &&
      first.is_compliment(second))
    return first;
  return {};
}

void MideaProtocol::dump(const MideaData &data) { ESP_LOGD(TAG, "Received Midea: %s", data.to_string().c_str()); }

}  // namespace remote_base
}  // namespace esphome
