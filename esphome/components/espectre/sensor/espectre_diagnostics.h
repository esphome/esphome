#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESPECTRE

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../espectre.h"

namespace esphome::espectre {

/// Publishes all diagnostic sensors together from the same runtime sample.
class DiagnosticsUpdater final : public PollingComponent {
 public:
  explicit DiagnosticsUpdater(ESPectreComponent *parent) : parent_(parent) {}
  void update() override;
  void dump_config() override;

  SUB_SENSOR(generator_rate)
  SUB_SENSOR(traffic_tx_rate)
  SUB_SENSOR(traffic_rx_rate)
  SUB_SENSOR(csi_accepted_rate)
  SUB_SENSOR(csi_occupancy)

 protected:
  ESPectreComponent *parent_;
};

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
