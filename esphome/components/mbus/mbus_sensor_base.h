#pragma once
#include "esphome/components/mbus/mbus_frame.h"

namespace esphome {
namespace mbus {

class MBusSensorBase {
 public:
  virtual void publish(const std::unique_ptr<mbus::MBusValue> &value) = 0;
  int8_t get_data_index() const { return this->data_index_; };
  float get_factor() const { return this->factor_; };

 protected:
  float factor_{0.0};
  uint8_t data_index_{0};
};

}  // namespace mbus
}  // namespace esphome
