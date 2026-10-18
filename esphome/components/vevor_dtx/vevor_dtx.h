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
#ifdef USE_SENSOR
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(wind_speed)
  SUB_SENSOR(wind_gust)
  SUB_SENSOR(wind_direction)
  SUB_SENSOR(rain)
  SUB_SENSOR(uv_index)
  SUB_SENSOR(illuminance)
  SUB_SENSOR(tx_counter)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(sensor_id)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(low_battery)
#endif

 public:
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void set_bit_time_us(uint8_t bit_time_us) { this->bit_time_us_ = bit_time_us; }
  void set_station_id(uint16_t station_id) { this->station_id_ = station_id; }

 protected:
  static constexpr uint8_t PAYLOAD_SIZE = 21;
  static constexpr uint8_t CHECKSUM_INDEX = 19;
  static constexpr uint8_t TX_COUNTER_INDEX = 18;
  // The frame ends with the counter echo; trailing zero bits that merge into the receiver's idle entry come
  // back when that entry is cut to a frame's worth of zeros
  static constexpr uint16_t FRAME_BITS = PAYLOAD_SIZE * 8;

  using Payload = std::array<uint8_t, PAYLOAD_SIZE>;

  bool decode_timings_(const remote_base::RawTimings &timings, Payload &payload) const;
  bool publish_(const Payload &payload);

  uint16_t station_id_{0};  // 0 accepts every station in range
  uint8_t bit_time_us_{88};
};

}  // namespace esphome::vevor_dtx
