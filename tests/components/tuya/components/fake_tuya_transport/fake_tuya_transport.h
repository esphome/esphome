#pragma once

#include "esphome/components/tuya/tuya.h"

namespace esphome::fake_tuya_transport {

/// A Tuya controller with no transport: datapoint writes are dropped, and
/// initialization completes immediately in setup() so entities receive
/// their initial-state callbacks.
class FakeTuyaTransport : public tuya::Tuya {
 public:
  void setup() override { this->mark_initialized_(); }

 protected:
  void send_datapoint_command(uint8_t datapoint_id, tuya::TuyaDatapointType datapoint_type, const uint8_t *data,
                              size_t len) override {}
};

}  // namespace esphome::fake_tuya_transport
