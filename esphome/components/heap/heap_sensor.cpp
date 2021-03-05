#include "heap_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace heap {

static const char *TAG = "heap";

void HeapSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Heap:");
  LOG_SENSOR("  ", "Free", this->free_sensor_);
  LOG_SENSOR("  ", "Fragmentation", this->fragmentation_sensor_);
  LOG_SENSOR("  ", "Max Block", this->block_sensor_);
}

}  // namespace heap
}  // namespace esphome
