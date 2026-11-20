#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome::dallas_temp {

class DallasTemperatureSensor final : public PollingComponent, public sensor::Sensor, public one_wire::OneWireDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  /// Set the resolution for this sensor.
  void set_resolution(uint8_t resolution) { this->resolution_ = resolution; }

 protected:
  uint8_t resolution_;
  uint8_t scratch_pad_[9] = {0};

  /// Get the number of milliseconds we have to wait for the conversion phase.
  uint16_t millis_to_wait_for_conversion_() const;
  bool read_scratch_pad_();
  bool check_scratch_pad_();
  float get_temp_c_();
#ifdef USE_ONE_WIRE_RMT
  static void on_convert_done(void *context, one_wire::TransactionStatus status);
  static void on_read_done(void *context, one_wire::TransactionStatus status);

  // update() fills in what is missing here. These fields never change, so it does not matter if a node is running.
  one_wire::Transaction convert_{.write_len = 1, .on_done = on_convert_done, .context = this};
  one_wire::Transaction read_{.write_len = 1,
                              .read_data = this->scratch_pad_,
                              .read_len = sizeof(this->scratch_pad_),
                              .on_done = on_read_done,
                              .context = this};
#endif
};

}  // namespace esphome::dallas_temp
