#include "somfy_rts_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.somfy_rts";

static constexpr uint8_t FRAME_SIZE_IN_BYTES = 7;
static constexpr uint16_t BIT_LENGTH_US_HIGH = 600;
static constexpr uint16_t BIT_LENGTH_US_LOW = 600;

static constexpr uint16_t WAKEUP_HIGH_US = 10000;
static constexpr uint32_t WAKEUP_LOW_US = 98000;

static constexpr uint16_t HW_SYNC_US = 2500;

static constexpr uint16_t SW_SYNC_HIGH_US = 5000;
static constexpr uint16_t SW_SYNC_LOW_US = 650;

static constexpr uint16_t INTERFRAME_GAP_US = 30415;

// Private
void SomfyRtsProtocol::wakeup_(RemoteTransmitData *dst) const { dst->item(WAKEUP_HIGH_US, WAKEUP_LOW_US); }

// Private
void SomfyRtsProtocol::hw_sync_(RemoteTransmitData *dst) const {
  dst->item(HW_SYNC_US, HW_SYNC_US);
  dst->item(HW_SYNC_US, HW_SYNC_US);
}

// Private
void SomfyRtsProtocol::sw_sync_(RemoteTransmitData *dst) const { dst->item(SW_SYNC_HIGH_US, SW_SYNC_LOW_US); }

// Private (Manchester encoding)
void SomfyRtsProtocol::one_(RemoteTransmitData *dst) const {
  dst->space(BIT_LENGTH_US_LOW);
  dst->mark(BIT_LENGTH_US_HIGH);
}

// Private (Manchester encoding)
void SomfyRtsProtocol::zero_(RemoteTransmitData *dst) const {
  dst->mark(BIT_LENGTH_US_HIGH);
  dst->space(BIT_LENGTH_US_LOW);
}

void SomfyRtsProtocol::encode(RemoteTransmitData *dst, const SomfyRtsData &data) {
  dst->set_carrier_frequency(0);

  // Send wakeup
  this->wakeup_(dst);

  // Hardware sync
  this->hw_sync_(dst);

  // Software sync
  this->sw_sync_(dst);

  uint8_t frame[FRAME_SIZE_IN_BYTES];
  // Some non standard implementations of the SomfyRTS protocol encode the commands inside the key instead of inside the
  // control field
  if (data.control_in_key) {
    frame[0] = data.key | data.control;
    frame[1] = (0xf << 4);
  } else {
    frame[0] = data.key;
    frame[1] = (data.control << 4);
  }
  frame[2] = (data.rolling_code >> 8);
  frame[3] = (data.rolling_code & 0xFF);
  frame[4] = (data.address >> 16);
  frame[5] = (data.address >> 8) & 0xFF;
  frame[6] = (data.address & 0xFF);

  // Create checksum
  uint8_t crc = 0;
  for (uint8_t i = 0; i < FRAME_SIZE_IN_BYTES; i++)
    crc = crc ^ (frame[i] >> 4) ^ (frame[i] & 0xF);
  frame[1] |= (crc & 0x0F);

  // Obfuscate with XOR
  for (uint8_t i = 1; i < FRAME_SIZE_IN_BYTES; i++)
    frame[i] ^= frame[i - 1];

  // Manchester encode bits
  for (uint8_t i = 0; i < FRAME_SIZE_IN_BYTES; i++) {
    for (int8_t y = 7; y >= 0; y--) {  // Send MSB first
      if (frame[i] & (1 << y))
        this->one_(dst);
      else
        this->zero_(dst);
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
