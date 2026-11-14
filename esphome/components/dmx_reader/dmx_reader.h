#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"

#if __has_include(<esp_dmx.h>)
#include <esp_dmx.h>
#else
// Stubs for clang-tidy / non-ESP32 analysis environments where the
// PlatformIO library is not on the include path.
using dmx_port_t = int;
static constexpr size_t DMX_PACKET_SIZE = 513;
#endif

#ifndef DMX_READER_STATIC_SENSOR_COUNT
static constexpr size_t DMX_READER_STATIC_SENSOR_COUNT = 0;
#endif
#ifndef DMX_READER_DYNAMIC_SENSOR_COUNT
static constexpr size_t DMX_READER_DYNAMIC_SENSOR_COUNT = 0;
#endif

namespace esphome::dmx_reader {

struct StaticChannelSensor {
  uint16_t channel;
  sensor::Sensor *sensor;
};

struct DynamicChannelSensor {
  number::Number *channel_number;
  sensor::Sensor *sensor;
};

class DMXReader : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_tx_pin(InternalGPIOPin *pin) { this->tx_pin_ = pin; }
  void set_rx_pin(InternalGPIOPin *pin) { this->rx_pin_ = pin; }
  void set_rts_pin(InternalGPIOPin *pin) { this->rts_pin_ = pin; }
  void set_uart_num(int num) { this->uart_num_ = num; }

  uint8_t get_channel(uint16_t channel) const;

  // fixed channel — only compiled when count > 0
  SLOT_LIST_REQUIRES(DMX_READER_STATIC_SENSOR_COUNT)
  void register_channel_sensor(uint16_t channel, sensor::Sensor *sens) {
    this->static_sensors_.push_back({channel, sens});
  }

  // Dynamic channel via Number-entity — only compiled when count > 0
  SLOT_LIST_REQUIRES(DMX_READER_DYNAMIC_SENSOR_COUNT)
  void register_dynamic_channel_sensor(number::Number *channel_number, sensor::Sensor *sens) {
    this->dynamic_sensors_.push_back({channel_number, sens});
  }

 protected:
  InternalGPIOPin *tx_pin_{nullptr};
  InternalGPIOPin *rx_pin_{nullptr};
  InternalGPIOPin *rts_pin_{nullptr};
  int uart_num_{1};

  dmx_port_t dmx_port_;
  uint8_t data_[DMX_PACKET_SIZE]{};

  [[no_unique_address]] SlotList<StaticChannelSensor, DMX_READER_STATIC_SENSOR_COUNT> static_sensors_;
  [[no_unique_address]] SlotList<DynamicChannelSensor, DMX_READER_DYNAMIC_SENSOR_COUNT> dynamic_sensors_;
};

}  // namespace esphome::dmx_reader
