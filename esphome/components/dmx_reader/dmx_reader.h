#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include <esp_dmx.h>
#include <vector>

namespace esphome {
namespace dmx_reader {

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

  // Vast kanaal
  void register_channel_sensor(uint16_t channel, sensor::Sensor *sens);

  // Dynamisch kanaal via Number-entity
  void register_dynamic_channel_sensor(number::Number *channel_number, sensor::Sensor *sens);

 protected:
  InternalGPIOPin *tx_pin_{nullptr};
  InternalGPIOPin *rx_pin_{nullptr};
  InternalGPIOPin *rts_pin_{nullptr};
  int uart_num_{1};

  dmx_port_t dmx_port_;
  uint8_t data_[DMX_PACKET_SIZE]{};

  struct StaticChannelSensor {
    uint16_t channel;
    sensor::Sensor *sensor;
  };
  std::vector<StaticChannelSensor> static_sensors_;

  struct DynamicChannelSensor {
    number::Number *channel_number;
    sensor::Sensor *sensor;
  };
  std::vector<DynamicChannelSensor> dynamic_sensors_;
};

}  // namespace dmx_reader
}  // namespace esphome
