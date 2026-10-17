#include "mt6701_spi.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mt6701_spi {

ESPHOME_LOG_TAG(TAG, "mt6701_spi");

// The frame's 6-bit CRC (x^6 + x + 1, MSB first) over its first 18 bits is the core 8-bit CRC with the
// polynomial shifted up by two, so a good frame checks to zero as a whole
static constexpr uint8_t CRC6_AS_CRC8_POLY = 0x0C;

bool decode_frame(const uint8_t frame[3], uint16_t &count, uint8_t &status) {
  if (crc8(frame, 3, 0x00, CRC6_AS_CRC8_POLY, true) != 0)
    return false;
  const uint32_t bits = encode_uint24(frame[0], frame[1], frame[2]);
  count = bits >> 10;
  status = (bits >> 6) & 0x0F;
  return true;
}

ProbeResult probe_result(uint8_t valid_frames, bool any_nonzero) {
  if (valid_frames < PROBE_MIN_VALID)
    return ProbeResult::PROBE_RESULT_NO_ENCODER;
  return any_nonzero ? ProbeResult::PROBE_RESULT_OK : ProbeResult::PROBE_RESULT_ALL_ZERO;
}

void MT6701SPIComponent::setup() {
  this->spi_setup();
  // An all-zero result only warns: a real encoder resting at count 0 with no flags set looks
  // like a stuck-low data line and must keep working
  uint8_t valid = 0;
  bool nonzero = false;
  for (uint8_t i = 0; i < PROBE_FRAMES && (valid < PROBE_MIN_VALID || !nonzero); i++) {
    uint16_t count;
    if (this->read_count(count)) {
      valid++;
      nonzero |= count != 0 || this->status_ != 0;
    }
  }
  switch (probe_result(valid, nonzero)) {
    case ProbeResult::PROBE_RESULT_NO_ENCODER:
      this->mark_failed(LOG_STR("no valid frame received, check the wiring and CS pin"));
      break;
    case ProbeResult::PROBE_RESULT_ALL_ZERO:
      ESP_LOGW(TAG, "Every frame read zero: the data line may be stuck low");
      break;
    case ProbeResult::PROBE_RESULT_OK:
      break;
  }
}

void MT6701SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MT6701 (SSI/SPI):");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

bool MT6701SPIComponent::read_count(uint16_t &count) {
  // The 24-bit SSI frame is a 14-bit angle, 4-bit status and 6-bit CRC, most significant bit first
  uint8_t buffer[3] = {0, 0, 0};
  this->enable();
  this->read_array(buffer, 3);
  this->disable();
  return decode_frame(buffer, count, this->status_);
}

}  // namespace esphome::mt6701_spi
