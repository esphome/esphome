#include "govee_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.govee";

static const uint32_t HEADER_HIGH_US = 281;
static const uint32_t HEADER_LOW_US = 8719;
static const uint32_t BIT_TOTAL_US = 1125;
static const uint32_t BIT_ONE_LOW_US = 281;
static const uint32_t BIT_ZERO_LOW_US = 844;
static const uint32_t REPEAT_DETECTION_WINDOW_MILLIS = 2000;  // two seconds

void GoveeProtocol::encode(RemoteTransmitData *dst, const GoveeData &data) {
  dst->reserve(data.repeat * 49);

  uint64_t raw_code;
  uint16_t crc;
  crc = data.address ^ data.cmdopt;
  crc = (crc >> 8) ^ (crc & 0xff) ^ data.command;
  crc = ((crc >> 4) & 0xf) ^ (crc & 0xf);
  crc <<= 1;
  crc |= 0xa1;

  raw_code = (uint64_t) data.address << 32;
  raw_code |= data.command << 24;
  raw_code |= data.cmdopt << 8;
  raw_code |= crc & 0xff;

  for (int i = 0; i < data.repeat; i++) {
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);

    for (uint64_t mask = 1LL << 47; mask > 1; mask >>= 1) {
      if (raw_code & mask) {
        dst->item(BIT_TOTAL_US - BIT_ONE_LOW_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_TOTAL_US - BIT_ZERO_LOW_US, BIT_ZERO_LOW_US);
      }
    }

    if (crc & 1) {
      dst->item(BIT_TOTAL_US - BIT_ONE_LOW_US, BIT_ONE_LOW_US + BIT_TOTAL_US);
    } else {
      dst->item(BIT_TOTAL_US - BIT_ZERO_LOW_US, BIT_ZERO_LOW_US + BIT_TOTAL_US);
    }
  }
}
optional<GoveeData> GoveeProtocol::decode(RemoteReceiveData src) {
  static uint32_t last_decoded_millis = 0;
  static uint64_t last_decoded_raw = 0;
  GoveeData data{
      .address = 0,
      .command = 0,
      .cmdopt = 0,
      .crc = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  // receive 48bit data
  uint64_t raw_code = 0;
  for (uint64_t mask = 1LL << 47; mask > 1; mask >>= 1) {
    if (src.expect_item(BIT_TOTAL_US - BIT_ONE_LOW_US, BIT_ONE_LOW_US)) {
      raw_code |= mask;
    } else if (src.expect_item(BIT_TOTAL_US - BIT_ZERO_LOW_US, BIT_ZERO_LOW_US) == 0) {
      return {};
    }
  }
  if (src.expect_mark(BIT_TOTAL_US - BIT_ONE_LOW_US)) {
    raw_code |= 1;
  } else if (src.expect_mark(BIT_TOTAL_US - BIT_ZERO_LOW_US) == 0) {
    return {};
  }

  data.address = (raw_code >> 32) & 0xffff;
  data.command = (raw_code >> 24) & 0xff;
  data.cmdopt = (raw_code >> 8) & 0xffff;
  data.crc = raw_code & 0xff;

  uint16_t crc = data.address ^ data.cmdopt;
  crc = (crc >> 8) ^ (crc & 0xff) ^ data.command;
  crc = ((crc >> 4) & 0xf) ^ (crc & 0xf);
  crc <<= 1;
  crc ^= data.crc;

  // check CRC
  if (crc != 0x40 && crc != 0xa1) {
    return {};
  }

  // Govee sensors send an event 12-13 times, repeatition should be filtered.
  uint32_t now = millis();
  if (((now - last_decoded_millis) < REPEAT_DETECTION_WINDOW_MILLIS) && (last_decoded_raw == raw_code)) {
    // repeatition detected,ignore the code
    return {};
  }
  last_decoded_millis = now;
  last_decoded_raw = raw_code;
  return data;
}
void GoveeProtocol::dump(const GoveeData &data) {
  ESP_LOGD(TAG, "Received Govee: address=0x%04X, command=0x%02X, cmdopt=0x%04X, crc=0x%02X", data.address, data.command,
           data.cmdopt, data.crc);
}

}  // namespace remote_base
}  // namespace esphome
