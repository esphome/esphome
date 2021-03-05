#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace heap {

class HeapSensor : public PollingComponent {
 public:
  void update() override {
    if (this->free_sensor_ != nullptr) {
      int free = ESP.getFreeHeap();
      this->free_sensor_->publish_state(free);
    }
    
    if (this->fragmentation_sensor_ != nullptr) {
      int frag = ESP.getHeapFragmentation();
      this->fragmentation_sensor_->publish_state(frag);
    }
    
    if (this->block_sensor_ != nullptr) {
      int block = ESP.getMaxFreeBlockSize();
      this->block_sensor_->publish_state(block);
    }
  }

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_free_sensor(sensor::Sensor *free_sensor) { free_sensor_ = free_sensor; }
  void set_fragmentation_sensor(sensor::Sensor *fragmentation_sensor) { fragmentation_sensor_ = fragmentation_sensor; }
  void set_block_sensor(sensor::Sensor *block_sensor) { block_sensor_ = block_sensor; }

 protected:
  sensor::Sensor *free_sensor_{nullptr};
  sensor::Sensor *fragmentation_sensor_{nullptr};
  sensor::Sensor *block_sensor_{nullptr};
};

}  // namespace heap
}  // namespace esphome
