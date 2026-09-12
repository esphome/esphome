#include "vevor_dtx.h"

#include <cinttypes>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::vevor_dtx {

static const char *const TAG = "vevor_dtx";

static uint16_t vevor_encode_uint16(uint8_t msb, uint8_t lsb) { return (uint16_t(msb) << 8) | lsb; }

void IRAM_ATTR HOT VevorDtxStore::gpio_intr(VevorDtxStore *arg) {
  if (arg->available) {
    return;
  }

  const bool level = arg->pin.digital_read();
  const uint32_t now = micros();
  const uint32_t delta = now - arg->last_micros;
  if (delta < arg->filter_us) {
    return;
  }

  uint16_t index = arg->buffer_index;
  if (index >= arg->buffer_size) {
    arg->overflow = true;
    arg->available = true;
    return;
  }

  arg->buffer[index] = arg->last_level ? int32_t(delta) : -int32_t(delta);
  arg->buffer_index = index + 1;
  arg->last_micros = now;
  arg->last_level = level;
}

void VevorDtxComponent::setup() {
  this->pin_->setup();
  this->store_.pin = this->pin_->to_isr();
  this->store_.buffer = new int32_t[this->buffer_size_];
  this->store_.buffer_size = this->buffer_size_;
  this->store_.filter_us = this->filter_us_;
  this->store_.last_level = this->pin_->digital_read();
  this->store_.last_micros = micros();
  this->pin_->attach_interrupt(VevorDtxStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
}

void VevorDtxComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Vevor DTX:\n"
                "  Bit time: %" PRIu32 " us\n"
                "  Max run gap: %" PRIu32 " us\n"
                "  Filter pulses shorter than: %" PRIu32 " us\n"
                "  Buffer Size: %" PRIu16,
                this->bit_time_us_, this->max_gap_us_, this->filter_us_, this->buffer_size_);
  LOG_PIN("  Pin: ", this->pin_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Wind speed", this->wind_speed_sensor_);
  LOG_SENSOR("  ", "Wind gust", this->wind_gust_sensor_);
  LOG_SENSOR("  ", "Wind direction", this->wind_direction_sensor_);
  LOG_SENSOR("  ", "Rain", this->rain_sensor_);
  LOG_SENSOR("  ", "UV index", this->uv_index_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_TEXT_SENSOR("  ", "Sensor ID", this->sensor_id_text_sensor_);
  LOG_SENSOR("  ", "TX counter", this->tx_counter_sensor_);
  LOG_BINARY_SENSOR("  ", "Low battery", this->low_battery_sensor_);
}

void VevorDtxComponent::loop() {
  auto &s = this->store_;

  if (!s.available && s.buffer_index > 0 && micros() - s.last_micros >= this->max_gap_us_) {
    InterruptLock lock;
    if (!s.available && s.buffer_index > 0 && micros() - s.last_micros >= this->max_gap_us_) {
      const uint16_t index = s.buffer_index;
      if (index < s.buffer_size) {
        const uint32_t delta = micros() - s.last_micros;
        s.buffer[index] = s.last_level ? int32_t(delta) : -int32_t(delta);
        s.buffer_index = index + 1;
      } else {
        s.overflow = true;
      }
      s.available = true;
    }
  }

  if (!s.available) {
    return;
  }

  if (s.overflow) {
    ESP_LOGW(TAG, "Buffer overflow");
  }

  std::array<uint8_t, PAYLOAD_SIZE> payload{};
  if (this->decode_timings_(s.buffer, s.buffer_index, payload)) {
    this->publish_(payload);
  } else {
    ESP_LOGVV(TAG, "No Vevor DTX frame found in %" PRIu16 " timings", s.buffer_index);
  }
  this->reset_store_();
}

bool VevorDtxComponent::decode_timings_(const volatile int32_t *timings, uint16_t timing_count,
                                        std::array<uint8_t, PAYLOAD_SIZE> &payload) {
  std::array<bool, MAX_BITS> bits{};
  uint16_t bit_count = 0;

  for (uint16_t timing_index = 0; timing_index < timing_count; timing_index++) {
    const int32_t timing = timings[timing_index];
    uint32_t duration = timing < 0 ? uint32_t(-timing) : uint32_t(timing);
    if (duration >= this->max_gap_us_) {
      if (bit_count > 0) {
        break;
      }
      continue;
    }

    uint32_t cells = (duration + (this->bit_time_us_ / 2)) / this->bit_time_us_;
    if (cells == 0) {
      cells = 1;
    }
    if (cells > 32) {
      ESP_LOGVV(TAG, "Ignoring implausible run: %" PRIu32 " us -> %" PRIu32 " cells", duration, cells);
      return false;
    }

    const bool level = timing > 0;
    for (uint32_t i = 0; i < cells && bit_count < MAX_BITS; i++) {
      bits[bit_count++] = level;
    }
    if (bit_count >= MAX_BITS) {
      ESP_LOGV(TAG, "Bit buffer full before frame end");
      break;
    }
  }

  return this->decode_bits_(bits, bit_count, payload);
}

bool VevorDtxComponent::decode_bits_(const std::array<bool, MAX_BITS> &bits, uint16_t bit_count,
                                     std::array<uint8_t, PAYLOAD_SIZE> &payload) const {
  for (uint8_t bit_offset = 0; bit_offset < 8; bit_offset++) {
    std::array<uint8_t, MAX_BYTES> bytes{};
    uint16_t byte_count = 0;
    for (uint16_t pos = bit_offset; pos + 7 < bit_count && byte_count < MAX_BYTES; pos += 8) {
      uint8_t value = 0;
      for (uint8_t bit = 0; bit < 8; bit++) {
        value = (value << 1) | uint8_t(bits[pos + bit]);
      }
      bytes[byte_count++] = value;
    }
    if (this->extract_payload_(bytes, byte_count, payload)) {
      ESP_LOGVV(TAG, "Decoded with bit offset %u", bit_offset);
      return true;
    }
  }
  return false;
}

void VevorDtxComponent::reset_store_() {
  InterruptLock lock;
  this->store_.buffer_index = 0;
  this->store_.last_level = this->pin_->digital_read();
  this->store_.last_micros = micros();
  this->store_.available = false;
  this->store_.overflow = false;
}

bool VevorDtxComponent::extract_payload_(const std::array<uint8_t, MAX_BYTES> &bytes, uint16_t byte_count,
                                         std::array<uint8_t, PAYLOAD_SIZE> &payload) const {
  static constexpr std::array<uint8_t, SYNC_SIZE> SYNC{{0xAA, 0xAA, 0xCA, 0xCA, 0x54}};

  if (byte_count < SYNC_SIZE + PAYLOAD_SIZE) {
    return false;
  }

  for (uint32_t i = 0; i <= byte_count - SYNC_SIZE - PAYLOAD_SIZE; i++) {
    bool sync_match = true;
    for (uint8_t j = 0; j < SYNC_SIZE; j++) {
      if (bytes[i + j] != SYNC[j]) {
        sync_match = false;
        break;
      }
    }
    if (!sync_match) {
      continue;
    }

    for (uint8_t j = 0; j < PAYLOAD_SIZE; j++) {
      payload[j] = bytes[i + SYNC_SIZE + j];
    }

    if (payload[0] != 0xAA || payload[1] != 0x00) {
      ESP_LOGVV(TAG, "Sync matched but payload header was 0x%02X 0x%02X", payload[0], payload[1]);
      continue;
    }

    uint8_t checksum = 0;
    for (uint8_t j = 0; j < 19; j++) {
      checksum += payload[j];
    }
    if (checksum != payload[19]) {
      ESP_LOGV(TAG, "Checksum mismatch: calculated 0x%02X, received 0x%02X", checksum, payload[19]);
      continue;
    }

    return true;
  }
  return false;
}

void VevorDtxComponent::publish_(const std::array<uint8_t, PAYLOAD_SIZE> &payload) {
  const uint16_t sensor_id = vevor_encode_uint16(payload[2], payload[3]);
  const bool low_battery = (payload[4] & 0x80) != 0;
  const float temperature = (float(vevor_encode_uint16(payload[5], payload[6])) - 500.0f) / 10.0f;
  const uint8_t humidity = payload[7];

  const uint16_t wind_raw = vevor_encode_uint16(payload[8], payload[9]);
  const float wind_speed_kmh = (float(wind_raw) - 0x0101) / 8.333f;
  const float wind_gust_kmh = float(payload[10]) / 1.25f;
  const float wind_speed = wind_speed_kmh / 3.6f;
  const float wind_gust = wind_gust_kmh / 3.6f;
  const uint16_t direction_raw = vevor_encode_uint16(payload[11] & 0x0F, payload[12]);
  const float wind_direction = float(direction_raw) - 0x0101;
  const uint16_t rain_raw = vevor_encode_uint16(payload[13], payload[14]);
  const float rain = (float(rain_raw) - 0x0101) * 0.233f;
  const uint8_t uv_byte = payload[15];
  const int8_t uv_index = int8_t(uv_byte & 0x1F) - 1;

  const uint16_t lux_raw = vevor_encode_uint16(payload[16], payload[17]);
  float illuminance = float((lux_raw & 0x7FFF) - 0x0101);
  if ((lux_raw & 0x8000) != 0) {
    illuminance *= 10.0f;
  }

  const uint8_t tx_counter_raw = payload[18];
  if (this->has_last_tx_counter_ && tx_counter_raw < this->last_tx_counter_raw_ &&
      uint8_t(this->last_tx_counter_raw_ - tx_counter_raw) > 128) {
    this->tx_counter_wraps_++;
  }
  this->last_tx_counter_raw_ = tx_counter_raw;
  this->has_last_tx_counter_ = true;
  const uint32_t tx_counter = (uint32_t(this->tx_counter_wraps_) << 8) | tx_counter_raw;

  ESP_LOGD(TAG,
           "Vevor DTX id=0x%04X status=0x%02X temp=%.1fC hum=%u%% wind=%.1fm/s gust=%.1fm/s dir=%.0f "
           "rain=%.1f uv=%d raw=0x%02X lux=%.0f raw=0x%04X tx=%" PRIu32 "/%u",
           sensor_id, payload[4], temperature, humidity, wind_speed, wind_gust, wind_direction, rain, uv_index, uv_byte,
           illuminance, lux_raw, tx_counter, payload[20]);

  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(humidity);
  if (this->wind_speed_sensor_ != nullptr)
    this->wind_speed_sensor_->publish_state(wind_speed);
  if (this->wind_gust_sensor_ != nullptr)
    this->wind_gust_sensor_->publish_state(wind_gust);
  if (this->wind_direction_sensor_ != nullptr)
    this->wind_direction_sensor_->publish_state(wind_direction);
  if (this->rain_sensor_ != nullptr)
    this->rain_sensor_->publish_state(rain);
  if (this->uv_index_sensor_ != nullptr)
    this->uv_index_sensor_->publish_state(uv_index);
  if (this->illuminance_sensor_ != nullptr)
    this->illuminance_sensor_->publish_state(illuminance);
  if (this->sensor_id_text_sensor_ != nullptr) {
    char sensor_id_text[7];
    sensor_id_text[0] = '0';
    sensor_id_text[1] = 'x';
    sensor_id_text[2] = format_hex_pretty_char((sensor_id >> 12) & 0x0F);
    sensor_id_text[3] = format_hex_pretty_char((sensor_id >> 8) & 0x0F);
    sensor_id_text[4] = format_hex_pretty_char((sensor_id >> 4) & 0x0F);
    sensor_id_text[5] = format_hex_pretty_char(sensor_id & 0x0F);
    sensor_id_text[6] = '\0';
    this->sensor_id_text_sensor_->publish_state(sensor_id_text);
  }
  if (this->tx_counter_sensor_ != nullptr)
    this->tx_counter_sensor_->publish_state(tx_counter);
  if (this->low_battery_sensor_ != nullptr)
    this->low_battery_sensor_->publish_state(low_battery);
}

}  // namespace esphome::vevor_dtx
