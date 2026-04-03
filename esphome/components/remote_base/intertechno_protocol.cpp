#include "intertechno_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.intertechno";

static const uint32_t BIT_LONG_US = 3 * 400;
static const uint32_t BIT_SHORT_US = 400;
static const uint32_t TRAILER_HIGH_US = 400;
static const uint32_t TRAILER_LOW_US = 31 * 400;

void IntertechnoProtocol::encode(RemoteTransmitData *dst, const IntertechnoData &data) {
  dst->set_carrier_frequency(433920000);
  dst->reserve(2 + data.code.length() * 4u);

  for (char c : data.code) {
    if (c == '0') {
      encode_0(dst);
    } else if (c == '1') {
      encode_1(dst);
    } else if (c == 'F') {
      encode_F(dst);
    }
  }

  dst->item(TRAILER_HIGH_US, TRAILER_LOW_US);
}
void IntertechnoProtocol::encode_0(RemoteTransmitData *dst) {
  dst->item(BIT_SHORT_US, BIT_LONG_US);
  dst->item(BIT_SHORT_US, BIT_LONG_US);
}
void IntertechnoProtocol::encode_1(RemoteTransmitData *dst) {
  dst->item(BIT_LONG_US, BIT_SHORT_US);
  dst->item(BIT_LONG_US, BIT_SHORT_US);
}
void IntertechnoProtocol::encode_F(RemoteTransmitData *dst) {
  dst->item(BIT_SHORT_US, BIT_LONG_US);
  dst->item(BIT_LONG_US, BIT_SHORT_US);
}
optional<IntertechnoData> IntertechnoProtocol::decode(RemoteReceiveData src) {
  IntertechnoData out{
      .code = "",
  };

  for (size_t i = 0; i < 12; i++) {
    if (src.peek_item(BIT_SHORT_US, BIT_LONG_US)) {  // 0 or H
      src.advance(2);
      if (src.peek_item(BIT_SHORT_US, BIT_LONG_US)) {  // 0
        src.advance(2);
        out.code += "0";
      } else if (src.expect_item(BIT_SHORT_US, BIT_LONG_US)) {  // H
        out.code += "H";
      } else {
        return {};
      }
    } else if (src.expect_item(BIT_LONG_US, BIT_SHORT_US)) {  // must be 1
      if (src.expect_item(BIT_LONG_US, BIT_SHORT_US)) {
        out.code += "1";
      }
    } else {
      return {};
    }
  }

  if (!src.expect_item(TRAILER_HIGH_US, TRAILER_LOW_US))
    return {};

  return out;
}
void IntertechnoProtocol::dump(const IntertechnoData &data) {
  ESP_LOGI(TAG, "Received Intertechno: code=%s", data.code);
}

}  // namespace remote_base
}  // namespace esphome
