#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#include <array>

namespace esphome::vevor_dtx {

struct VevorDtxStore {
  static void gpio_intr(VevorDtxStore *arg);

  volatile int32_t *buffer{nullptr};
  volatile uint16_t buffer_index{0};
  uint16_t buffer_size{512};
  uint32_t filter_us{20};
  ISRInternalGPIOPin pin;
  volatile uint32_t last_micros{0};
  volatile bool last_level{false};
  volatile bool available{false};
  volatile bool overflow{false};
};

class VevorDtxComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_bit_time_us(uint32_t bit_time_us) { this->bit_time_us_ = bit_time_us; }
  void set_max_gap_us(uint32_t max_gap_us) { this->max_gap_us_ = max_gap_us; }
  void set_filter_us(uint32_t filter_us) { this->filter_us_ = filter_us; }
  void set_buffer_size(uint32_t buffer_size) { this->buffer_size_ = buffer_size; }

  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_wind_speed_sensor(sensor::Sensor *sensor) { this->wind_speed_sensor_ = sensor; }
  void set_wind_gust_sensor(sensor::Sensor *sensor) { this->wind_gust_sensor_ = sensor; }
  void set_wind_direction_sensor(sensor::Sensor *sensor) { this->wind_direction_sensor_ = sensor; }
  void set_rain_sensor(sensor::Sensor *sensor) { this->rain_sensor_ = sensor; }
  void set_uv_index_sensor(sensor::Sensor *sensor) { this->uv_index_sensor_ = sensor; }
  void set_illuminance_sensor(sensor::Sensor *sensor) { this->illuminance_sensor_ = sensor; }
  void set_sensor_id_text_sensor(text_sensor::TextSensor *sensor) { this->sensor_id_text_sensor_ = sensor; }
  void set_tx_counter_sensor(sensor::Sensor *sensor) { this->tx_counter_sensor_ = sensor; }
  void set_low_battery_binary_sensor(binary_sensor::BinarySensor *sensor) { this->low_battery_sensor_ = sensor; }

 protected:
  static constexpr uint8_t SYNC_SIZE = 5;
  static constexpr uint8_t PAYLOAD_SIZE = 21;
  static constexpr uint16_t MAX_BITS = 1024;
  static constexpr uint16_t MAX_BYTES = MAX_BITS / 8;

  bool decode_bits_(const std::array<bool, MAX_BITS> &bits, uint16_t bit_count,
                    std::array<uint8_t, PAYLOAD_SIZE> &payload) const;
  bool extract_payload_(const std::array<uint8_t, MAX_BYTES> &bytes, uint16_t byte_count,
                        std::array<uint8_t, PAYLOAD_SIZE> &payload) const;
  bool decode_timings_(const volatile int32_t *timings, uint16_t timing_count,
                       std::array<uint8_t, PAYLOAD_SIZE> &payload);
  void reset_store_();
  void publish_(const std::array<uint8_t, PAYLOAD_SIZE> &payload);

  InternalGPIOPin *pin_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *wind_speed_sensor_{nullptr};
  sensor::Sensor *wind_gust_sensor_{nullptr};
  sensor::Sensor *wind_direction_sensor_{nullptr};
  sensor::Sensor *rain_sensor_{nullptr};
  sensor::Sensor *uv_index_sensor_{nullptr};
  sensor::Sensor *illuminance_sensor_{nullptr};
  text_sensor::TextSensor *sensor_id_text_sensor_{nullptr};
  sensor::Sensor *tx_counter_sensor_{nullptr};
  binary_sensor::BinarySensor *low_battery_sensor_{nullptr};

  uint32_t bit_time_us_{88};
  uint32_t max_gap_us_{2000};
  uint32_t filter_us_{20};
  uint16_t buffer_size_{512};
  uint8_t last_tx_counter_raw_{0};
  uint16_t tx_counter_wraps_{0};
  bool has_last_tx_counter_{false};
  VevorDtxStore store_;
};

}  // namespace esphome::vevor_dtx
