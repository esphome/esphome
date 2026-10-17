#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/remote_base/remote_base.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <array>

namespace esphome::vevor_dtx {

class VevorDtxComponent : public Component, public remote_base::RemoteReceiverListener {
 public:
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void set_bit_time_us(uint32_t bit_time_us) { this->bit_time_us_ = bit_time_us; }
  void set_station_id(uint16_t station_id) { this->station_id_ = station_id; }

#ifdef USE_SENSOR
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_wind_speed_sensor(sensor::Sensor *sensor) { this->wind_speed_sensor_ = sensor; }
  void set_wind_gust_sensor(sensor::Sensor *sensor) { this->wind_gust_sensor_ = sensor; }
  void set_wind_direction_sensor(sensor::Sensor *sensor) { this->wind_direction_sensor_ = sensor; }
  void set_rain_sensor(sensor::Sensor *sensor) { this->rain_sensor_ = sensor; }
  void set_uv_index_sensor(sensor::Sensor *sensor) { this->uv_index_sensor_ = sensor; }
  void set_illuminance_sensor(sensor::Sensor *sensor) { this->illuminance_sensor_ = sensor; }
  void set_tx_counter_sensor(sensor::Sensor *sensor) { this->tx_counter_sensor_ = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_sensor_id_text_sensor(text_sensor::TextSensor *sensor) { this->sensor_id_text_sensor_ = sensor; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_low_battery_binary_sensor(binary_sensor::BinarySensor *sensor) { this->low_battery_sensor_ = sensor; }
#endif

 protected:
  static constexpr uint8_t SYNC_SIZE = 5;
  static constexpr uint8_t PAYLOAD_SIZE = 21;
  static constexpr uint16_t MAX_BITS = 1024;
  static constexpr uint16_t MAX_BYTES = MAX_BITS / 8;
  // No run inside a frame is this long; a longer one is the gap before or after it
  static constexpr uint32_t MAX_RUN_CELLS = 24;

  using Payload = std::array<uint8_t, PAYLOAD_SIZE>;
  // The received bit stream, packed most significant bit first
  using Bits = std::array<uint8_t, MAX_BYTES>;

  bool decode_timings_(const remote_base::RawTimings &timings, Payload &payload) const;
  bool decode_bits_(const Bits &bits, uint16_t bit_count, Payload &payload) const;
  static bool extract_payload_(const std::array<uint8_t, MAX_BYTES> &bytes, uint16_t byte_count, Payload &payload);
  bool publish_(const Payload &payload);

#ifdef USE_SENSOR
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *wind_speed_sensor_{nullptr};
  sensor::Sensor *wind_gust_sensor_{nullptr};
  sensor::Sensor *wind_direction_sensor_{nullptr};
  sensor::Sensor *rain_sensor_{nullptr};
  sensor::Sensor *uv_index_sensor_{nullptr};
  sensor::Sensor *illuminance_sensor_{nullptr};
  sensor::Sensor *tx_counter_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *sensor_id_text_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *low_battery_sensor_{nullptr};
#endif

  uint32_t bit_time_us_{88};
  uint16_t station_id_{0};  // 0 accepts every station in range
  uint16_t tx_counter_wraps_{0};
  uint8_t last_tx_counter_raw_{0};
  bool has_last_tx_counter_{false};
};

}  // namespace esphome::vevor_dtx
