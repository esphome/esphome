#include "keeloq_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *TAG = "remote.keeloq";

static const uint8_t NBITS = 156;

static const uint32_t PREAMBLE_HIGH_US = 425;
static const uint32_t PREAMBLE_LOW_US = 325;
static const uint32_t HEADER_LOW_US = 2700;

void KeeloqProtocol::encode(RemoteTransmitData *dst, const KeeloqData &data) {
	// not implemented
}

optional<KeeloqData> KeeloqProtocol::decode(RemoteReceiveData src) {
  KeeloqData out;

  // check if we have enough bits
  if (src.size() != NBITS) {
    if (src.size() != 199)
      ESP_LOGV(TAG, "Received %d bits", src.size());
    return {};
  }

  // check if header matches
  for (uint8_t i = 0; i < 11; i++) {
    if (!src.expect_item(PREAMBLE_HIGH_US, PREAMBLE_LOW_US)) {
      ESP_LOGD(TAG, "Preamble failure on bit %d, got (%d,%d)", i, src.peek(), src.peek(1));
      return {};
	  }
  }

  if (!src.expect_mark(PREAMBLE_HIGH_US)) {
    ESP_LOGD(TAG, "Preamble failure, mark %d", src.peek());
    return {};
  }
  // te is the elemental period, derived from the actual 
  // HEADER_LOW period, divided by 10
  uint32_t te = 380;
  if (src.peek_space_at_least(HEADER_LOW_US)) {
    int32_t header_low = src.peek();
    te = (-header_low) / 10;
  	src.advance();
  } else {
    ESP_LOGD(TAG, "Header failure: %d", src.peek());
    return {};
  }

  out.encrypted = 0;
  // get the first 32 encrypted bits
  for (uint8_t i = 0; i < 32; i++) {
    if (src.expect_item(te, 2 * te)) {
      out.encrypted |= (1UL << i);
    } else if (!src.expect_item(2 * te, te)) {
      ESP_LOGD(TAG, "Failure reading encrypted bit %d, got (%d,%d)", i, src.peek(), src.peek(1));
      return {};
    }
  }

  out.serial = 0;
  // get the serial number bits
  for (uint8_t i = 0; i < 28; i++) {
    if (src.expect_item(te, 2 * te)) {
      out.serial |= (1UL << i);
    } else if (!src.expect_item(2 * te, te)) {
      ESP_LOGD(TAG, "Failure reading serial bit %d, got (%d,%d)", i, src.peek(), src.peek(1));
      return {};
    }
  }

  out.button = 0;
  // get the button bits
  for (uint8_t i = 0; i < 4; i++) {
    if (src.expect_item(te, 2 * te)) {
      out.button |= (1UL << i);
    } else if (!src.expect_item(2 * te, te)) {
      ESP_LOGD(TAG, "Failure reading button bit %d, got (%d,%d)", i, src.peek(), src.peek(1));
      return {};
    }
  }

  if (src.expect_item(te, 2 * te)) {
    out.low = 1;
  } else if (!src.expect_item(2 * te, te)) {
    ESP_LOGD(TAG, "Failure reading low voltage bit, got (%d,%d)", src.peek(), src.peek(1));
    return {};
  }

  // the space for the last bit merges with the space for the trailer
  // so we just check the mark
  if (src.expect_mark(te)) {
    out.repeat = 1;
  } else if (!src.expect_mark(2 * te)) {
    ESP_LOGD(TAG, "Failure reading repeat bit, got (%d)", src.peek());
    return {};
  }

  return out;
}
void KeeloqProtocol::dump(const KeeloqData &data) {
  ESP_LOGD(TAG, "Received Keeloq: serial=%07X, encrypted=%08X, button=%01X%s%s", data.serial, data.encrypted, data.button, data.low ? " LOW": "", data.repeat? " REPEAT" : "");

}

}  // namespace remote_base
}  // namespace esphome
