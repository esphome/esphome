#include "hp303b.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace hp303b {

void HP303BComponent::setup() {
  ESP_LOGCONFIG(TAGhp, "Setting up HP303B...");
  this->spi_setup();
  hp.begin();
}

void MPL3115A2Component::update() {
  int32_t pressure = 0;
  hp.measurePressureOnce(pressure);  // library returns value in in Pa, which equals 1/100 hPa
  float hPa = pressure / 100.0;
  if (this->pressure_ != nullptr)
    this->pressure_->publish_state(hPa);

  ESP_LOGD(TAG, "Got Pressure=%.1f", hPa);

  this->status_clear_warning();
}

}  // namespace hp303b
}  // namespace esphome