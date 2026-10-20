#include "receiver_mitsubishi_heavy.h"
#include "heatpumpir.h"

#if defined(USE_ARDUINO) || defined(USE_ESP32)

#include <HeatpumpIRFactory.h>

namespace esphome::heatpumpir {

static constexpr uint8_t FRAME_PREFIX[5] = {0x52, 0xAE, 0xC3, 0x26, 0xD9};
static constexpr uint8_t XOR_CHECKSUM_EXPECTED = 0xFF;
static constexpr uint8_t PREFIX_LENGTH = 5;
static constexpr uint8_t MODE_TEMP_BYTE = 9;
static constexpr uint8_t MODE_MASK = 0x07;
static constexpr uint8_t TEMP_MASK = 0x0F;
static constexpr uint8_t TEMPERATURE_BASE = 17;
static constexpr uint8_t TEMPERATURE_SHIFT = 4;
static constexpr remote_base::BitTimings MITSUBISHI_HEAVY_BIT_TIMINGS{.bit_mark = MITSUBISHI_HEAVY_BIT_MARK,
                                                                      .one_space = MITSUBISHI_HEAVY_ONE_SPACE,
                                                                      .zero_space = MITSUBISHI_HEAVY_ZERO_SPACE};

static bool read_raw_bytes(remote_base::RemoteReceiveData &data, uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH]) {
  if (!data.expect_item(MITSUBISHI_HEAVY_HDR_MARK, MITSUBISHI_HEAVY_HDR_SPACE))
    return false;

  return data.expect_bytes_lsb<MITSUBISHI_HEAVY_BIT_TIMINGS>(frame, MITSUBISHI_HEAVY_FRAME_LENGTH);
}

static bool validate_prefix(const uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH]) {
  for (uint8_t i = 0; i < PREFIX_LENGTH; i++) {
    if (frame[i] != FRAME_PREFIX[i])
      return false;
  }
  return true;
}

static bool validate_checksums(const uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH]) {
  for (uint8_t i = PREFIX_LENGTH; i < MITSUBISHI_HEAVY_FRAME_LENGTH; i += 2) {
    if ((uint8_t) (frame[i] ^ frame[i + 1]) != XOR_CHECKSUM_EXPECTED)
      return false;
  }
  return true;
}

static bool decode_mode_field(const uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH], climate::ClimateMode &mode) {
  if (frame[MODE_TEMP_BYTE] & MITSUBISHI_HEAVY_MODE_OFF) {
    mode = climate::CLIMATE_MODE_OFF;
    return true;
  }
  switch (frame[MODE_TEMP_BYTE] & MODE_MASK) {
    case MITSUBISHI_HEAVY_MODE_AUTO:
      mode = climate::CLIMATE_MODE_HEAT_COOL;
      return true;
    case MITSUBISHI_HEAVY_MODE_HEAT:
      mode = climate::CLIMATE_MODE_HEAT;
      return true;
    case MITSUBISHI_HEAVY_MODE_COOL:
      mode = climate::CLIMATE_MODE_COOL;
      return true;
    case MITSUBISHI_HEAVY_MODE_DRY:
      mode = climate::CLIMATE_MODE_DRY;
      return true;
    case MITSUBISHI_HEAVY_MODE_FAN:
      mode = climate::CLIMATE_MODE_FAN_ONLY;
      return true;
    default:
      return false;
  }
}

static float decode_temperature_field(const uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH]) {
  uint8_t temp_nibble = frame[MODE_TEMP_BYTE] >> TEMPERATURE_SHIFT;
  uint8_t inverted_nibble = ~temp_nibble;
  uint8_t temp_offset = inverted_nibble & TEMP_MASK;
  return TEMPERATURE_BASE + temp_offset;
}

static bool apply_climate_state(const uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH], HeatpumpIRClimate &climate) {
  climate::ClimateMode mode;
  if (!decode_mode_field(frame, mode))
    return false;
  climate.mode = mode;
  climate.target_temperature = decode_temperature_field(frame);
  return true;
}

bool decode_mitsubishi_heavy_frame(remote_base::RemoteReceiveData &data, uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH],
                                   HeatpumpIRClimate &climate) {
  if (!read_raw_bytes(data, frame))
    return false;
  if (!validate_prefix(frame))
    return false;
  if (!validate_checksums(frame))
    return false;
  return apply_climate_state(frame, climate);
}

}  // namespace esphome::heatpumpir

#endif
