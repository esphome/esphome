#include "espectre_diagnostics.h"

#ifdef USE_ESPECTRE

#include <cmath>
#include "esphome/core/log.h"

namespace esphome::espectre {

static const char *const TAG = "espectre.sensor";

static void publish_diagnostic(sensor::Sensor *sensor, const ::espectre::RuntimeDiagnosticsSample *sample,
                               float ::espectre::RuntimeDiagnosticsSample::*field, float scale = 1.0f) {
  if (sensor != nullptr)
    sensor->publish_state(sample != nullptr ? sample->*field * scale : NAN);
}

void DiagnosticsUpdater::update() {
  using Sample = ::espectre::RuntimeDiagnosticsSample;
  const auto *sample = this->parent_->diagnostics_sample();
  publish_diagnostic(this->generator_rate_sensor_, sample, &Sample::generator_pps);
  publish_diagnostic(this->traffic_tx_rate_sensor_, sample, &Sample::traffic_tx_pps);
  publish_diagnostic(this->traffic_rx_rate_sensor_, sample, &Sample::traffic_rx_pps);
  publish_diagnostic(this->csi_accepted_rate_sensor_, sample, &Sample::csi_accepted_pps);
  publish_diagnostic(this->csi_occupancy_sensor_, sample, &Sample::csi_occupancy_ratio, 100.0f);
}

void DiagnosticsUpdater::dump_config() {
  ESP_LOGCONFIG(TAG, "ESPectre diagnostics:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Generator rate", this->generator_rate_sensor_);
  LOG_SENSOR("  ", "Traffic TX rate", this->traffic_tx_rate_sensor_);
  LOG_SENSOR("  ", "Traffic RX rate", this->traffic_rx_rate_sensor_);
  LOG_SENSOR("  ", "CSI accepted rate", this->csi_accepted_rate_sensor_);
  LOG_SENSOR("  ", "CSI occupancy", this->csi_occupancy_sensor_);
}

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
