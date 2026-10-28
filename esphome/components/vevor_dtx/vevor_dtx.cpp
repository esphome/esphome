#include "vevor_dtx.h"

#include <algorithm>
#include <cinttypes>
#include <cstdlib>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::vevor_dtx {

ESPHOME_LOG_TAG(TAG, "vevor_dtx");

// The five sync bytes and the two constant header bytes that start every frame, matched bit by bit
static constexpr uint64_t SYNC_AND_HEADER = 0xAAAACACA54AA00ULL;
static constexpr uint64_t SYNC_AND_HEADER_MASK = (1ULL << 56) - 1;
static constexpr uint8_t HEADER_SIZE = 2;
// Wind arrives in 1/8.333 km/h steps and gust in 1/1.25 km/h (0.8 km/h) steps; both are published in m/s
static constexpr float WIND_SPEED_MS_PER_RAW = 1.0f / (8.333f * 3.6f);
static constexpr float WIND_GUST_MS_PER_RAW = 1.0f / (1.25f * 3.6f);
static constexpr float RAIN_MM_PER_RAW = 0.233f;

// Wind, direction, rain and light arrive with an offset of one on each byte; 0x00 wraps to 0xFF
static uint16_t offset_word(uint8_t high, uint8_t low) {
  return encode_uint16(static_cast<uint8_t>(high - 1), static_cast<uint8_t>(low - 1));
}

void VevorDtxComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Vevor DTX:\n"
                "  Bit time: %u us",
                this->bit_time_us_);
  if (this->station_id_ != 0) {
    ESP_LOGCONFIG(TAG, "  Station ID: 0x%04X", this->station_id_);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Wind speed", this->wind_speed_sensor_);
  LOG_SENSOR("  ", "Wind gust", this->wind_gust_sensor_);
  LOG_SENSOR("  ", "Wind direction", this->wind_direction_sensor_);
  LOG_SENSOR("  ", "Rain", this->rain_sensor_);
  LOG_SENSOR("  ", "UV index", this->uv_index_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "TX counter", this->tx_counter_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Sensor ID", this->sensor_id_text_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Low battery", this->low_battery_binary_sensor_);
#endif
}

bool VevorDtxComponent::on_receive(remote_base::RemoteReceiveData data) {
  Payload payload{};
  if (!this->decode_timings_(data.get_raw_data(), payload)) {
    ESP_LOGVV(TAG, "No Vevor DTX frame in %" PRId32 " timings", data.size());
    return false;
  }
  return this->publish_(payload);
}

// The frame is NRZ at a fixed bit time: a high run of n bit times is n one bits. The bits are shifted
// through a register until the sync word appears, then the payload is collected and its checksum tested.
// A run is never treated as a gap, since a payload may hold long runs of zeros; a gap simply shifts zeros
// through the register, and one longer than a frame is cut to a frame's worth, which has the same effect.
bool VevorDtxComponent::decode_timings_(const remote_base::RawTimings &timings, Payload &payload) const {
  uint64_t stream = 0;
  uint16_t collected = 0;  // payload bits gathered since the sync word, 0 while still searching for it

  for (const int32_t timing : timings) {
    const uint32_t duration = std::abs(timing);
    const uint32_t cells =
        std::clamp<uint32_t>((duration + this->bit_time_us_ / 2) / this->bit_time_us_, 1, FRAME_BITS);
    const uint64_t bit = timing > 0 ? 1 : 0;
    for (uint32_t i = 0; i < cells; i++) {
      stream = (stream << 1) | bit;
      if (collected == 0) {
        if ((stream & SYNC_AND_HEADER_MASK) != SYNC_AND_HEADER) {
          continue;
        }
        payload[0] = uint8_t(SYNC_AND_HEADER >> 8);
        payload[1] = uint8_t(SYNC_AND_HEADER);
        collected = HEADER_SIZE * 8;
        continue;
      }
      payload[collected / 8] = uint8_t((payload[collected / 8] << 1) | bit);
      if (++collected < FRAME_BITS) {
        continue;
      }
      // The byte after the checksum repeats the counter plus one; it rejects frames whose sum matches by chance
      if (sum_bytes(payload.data(), CHECKSUM_INDEX) == payload[CHECKSUM_INDEX] &&
          payload[TX_COUNTER_INDEX + 2] == uint8_t(payload[TX_COUNTER_INDEX] + 1)) {
        return true;
      }
      ESP_LOGV(TAG, "Rejected frame: sum 0x%02X received 0x%02X, counter %u followed by %u",
               sum_bytes(payload.data(), CHECKSUM_INDEX), payload[CHECKSUM_INDEX], payload[TX_COUNTER_INDEX],
               payload[TX_COUNTER_INDEX + 2]);
      collected = 0;
    }
  }
  return false;
}

bool VevorDtxComponent::publish_(const Payload &payload) {
  const uint16_t sensor_id = encode_uint16(payload[2], payload[3]);
  if (this->station_id_ != 0 && sensor_id != this->station_id_) {
    ESP_LOGV(TAG, "Ignoring station 0x%04X", sensor_id);
    return false;
  }
  const float temperature = (float(encode_uint16(payload[5], payload[6])) - 500.0f) / 10.0f;
  const uint8_t humidity = payload[7];
  const float wind_speed = float(offset_word(payload[8], payload[9])) * WIND_SPEED_MS_PER_RAW;
  const float wind_gust = float(payload[10]) * WIND_GUST_MS_PER_RAW;
  const float wind_direction = float(offset_word(payload[11], payload[12]) & 0x0FFF);
  const float rain = float(offset_word(payload[13], payload[14])) * RAIN_MM_PER_RAW;
  const int8_t uv_index = int8_t(payload[15] & 0x1F) - 1;
  // The top bit of the light word selects a tenfold range
  const uint16_t lux_raw = offset_word(payload[16], payload[17]);
  const float illuminance = (lux_raw & 0x8000) != 0 ? float(lux_raw & 0x7FFF) * 10.0f : float(lux_raw);
  const uint8_t tx_counter = payload[18];

  ESP_LOGD(TAG,
           "Vevor DTX id=0x%04X status=0x%02X temp=%.1fC hum=%u%% wind=%.1fm/s gust=%.1fm/s dir=%.0f "
           "rain=%.1f uv=%d raw=0x%02X lux=%.0f raw=0x%04X tx=%u",
           sensor_id, payload[4], temperature, humidity, wind_speed, wind_gust, wind_direction, rain, uv_index,
           payload[15], illuminance, lux_raw, tx_counter);

#ifdef USE_SENSOR
  const auto publish = [](sensor::Sensor *sensor, float value) { PUBLISH_STATE(sensor, value); };
  publish(this->temperature_sensor_, temperature);
  publish(this->humidity_sensor_, humidity);
  publish(this->wind_speed_sensor_, wind_speed);
  publish(this->wind_gust_sensor_, wind_gust);
  publish(this->wind_direction_sensor_, wind_direction);
  publish(this->rain_sensor_, rain);
  publish(this->uv_index_sensor_, uv_index);
  publish(this->illuminance_sensor_, illuminance);
  publish(this->tx_counter_sensor_, tx_counter);
#endif
#ifdef USE_TEXT_SENSOR
  if (this->sensor_id_text_sensor_ != nullptr) {
    char text[7];
    snprintf(text, sizeof(text), "0x%04X", sensor_id);
    this->sensor_id_text_sensor_->publish_state(text);
  }
#endif
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->low_battery_binary_sensor_, (payload[4] & 0x80) != 0);
#endif
  return true;
}

}  // namespace esphome::vevor_dtx
