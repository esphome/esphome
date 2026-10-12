#include "somfy_rts_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.somfy_rts");

static constexpr uint8_t FRAME_SIZE_IN_BYTES = 7;
static constexpr uint16_t BIT_US = 600;

static constexpr uint16_t WAKEUP_HIGH_US = 10000;
static constexpr uint32_t WAKEUP_LOW_US = 98000;

static constexpr uint16_t HW_SYNC_US = 2500;

static constexpr uint16_t SW_SYNC_HIGH_US = 5000;
static constexpr uint16_t SW_SYNC_LOW_US = 650;

static constexpr uint16_t INTERFRAME_GAP_US = 30415;

// Wakeup, two hardware syncs and a software sync (8), two entries per bit (112), the gap (1)
static constexpr uint32_t FRAME_ENTRIES = 8 + FRAME_SIZE_IN_BYTES * 8 * 2 + 1;

void SomfyRtsProtocol::encode(RemoteTransmitData *dst, const SomfyRtsData &data) {
  dst->reserve(FRAME_ENTRIES);
  dst->set_carrier_frequency(0);
  dst->item(WAKEUP_HIGH_US, WAKEUP_LOW_US);
  dst->item(HW_SYNC_US, HW_SYNC_US);
  dst->item(HW_SYNC_US, HW_SYNC_US);
  dst->item(SW_SYNC_HIGH_US, SW_SYNC_LOW_US);

  uint8_t frame[FRAME_SIZE_IN_BYTES];
  // Some non standard implementations of the SomfyRTS protocol encode the commands inside the key instead of inside the
  // control field
  if (data.control_in_key) {
    frame[0] = data.key | data.control;
    frame[1] = 0xF0;
  } else {
    frame[0] = data.key;
    frame[1] = data.control << 4;
  }
  frame[2] = data.rolling_code >> 8;
  frame[3] = data.rolling_code;
  frame[4] = data.address >> 16;
  frame[5] = data.address >> 8;
  frame[6] = data.address;

  uint8_t crc = 0;
  for (uint8_t data_byte : frame) {
    crc ^= (data_byte >> 4) ^ (data_byte & 0xF);
  }
  frame[1] |= (crc & 0x0F);

  for (uint8_t i = 1; i < FRAME_SIZE_IN_BYTES; i++) {
    frame[i] ^= frame[i - 1];
  }

  // Manchester, MSB first: a 1 is space then mark, a 0 is mark then space
  for (uint8_t data_byte : frame) {
    for (int8_t y = 7; y >= 0; y--) {
      if (data_byte & (1 << y)) {
        dst->space(BIT_US);
        dst->mark(BIT_US);
      } else {
        dst->mark(BIT_US);
        dst->space(BIT_US);
      }
    }
  }

  dst->space(INTERFRAME_GAP_US);
}

void SomfyRtsProtocol::dump(const SomfyRtsData &data) {
  ESP_LOGD(TAG, "Somfy RTS: key=0x%02X, control=0x%X, rolling_code=%u, address=0x%06X", data.key, data.control,
           data.rolling_code, data.address);
}

optional<SomfyRtsData> SomfyRtsProtocol::decode(RemoteReceiveData src) { return {}; }

}  // namespace esphome::remote_base
