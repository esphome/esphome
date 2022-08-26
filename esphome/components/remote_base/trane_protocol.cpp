//
// Created by felipeflores on 8/18/22.
//

#include "trane_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.trane";

static const uint32_t TICK_US = 570;
static const uint32_t HEADER_HIGH_US = 16*TICK_US;
static const uint32_t HEADER_LOW_US  = 8*TICK_US;
static const uint32_t BIT_HIGH_US = TICK_US;
static const uint32_t BIT_ONE_LOW_US = 3*TICK_US;
static const uint32_t BIT_ZERO_LOW_US = TICK_US;
static const uint32_t PAUSE_US = 20000;
static const uint32_t NBITS_1 = 35;
static const uint32_t NBITS_2 = 32;


void TraneProtocol::encode(RemoteTransmitData *dst, const TraneData &data) {
  ESP_LOGD(TAG, "Protocol: mode = 0x%03X", data.mode);
  ESP_LOGD(TAG, "Protocol: data1 = 0x%08X", data.trane_data_1);
  ESP_LOGD(TAG, "Protocol: data2 = 0x%08X", data.trane_data_2);

  dst->set_carrier_frequency(38100);
  dst->reserve(139);

  dst->item(HEADER_HIGH_US,HEADER_LOW_US);

  for (uint8_t mask = 1; mask <=4 ; mask <<= 1){
    if (data.mode & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (uint32_t mask = 1; mask ; mask <<= 1) {
    if (data.trane_data_1 & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US,BIT_ZERO_LOW_US);
    }
  }

  dst->mark(BIT_HIGH_US);
  dst->space(PAUSE_US);

  for (uint32_t mask = 1; mask ; mask <<= 1){
    if (data.trane_data_2 & mask){
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }
  dst->mark(BIT_HIGH_US);
  dst->space(PAUSE_US);
}

optional<TraneData> TraneProtocol::decode(RemoteReceiveData src) {
  TraneData data{
      .trane_data_1 = 0,
      .trane_data_2 = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint64_t mask = 1; mask<=17179869184; mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.trane_data_1 |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      data.trane_data_1 &= ~mask;
    } else {
      return {};
    }
  }

  src.expect_mark(BIT_HIGH_US);
  src.expect_space(PAUSE_US);

  for (uint32_t mask = 1; mask; mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.trane_data_2 |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      data.trane_data_2 &= ~mask;
    } else {
      return {};
    }
  }

  src.expect_mark(BIT_HIGH_US);
  src.expect_space(PAUSE_US);
  return data;
}

void TraneProtocol::dump(const TraneData &data) {
  ESP_LOGD(TAG, "Received Trane: data=0x%09X, word2=0x%08X", data.trane_data_1, data.trane_data_2);
}
}
}