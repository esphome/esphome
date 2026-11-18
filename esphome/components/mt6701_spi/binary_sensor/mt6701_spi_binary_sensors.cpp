#include "mt6701_spi_binary_sensors.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::mt6701_spi {

ESPHOME_LOG_TAG(TAG, "mt6701_spi.binary_sensor");

void MT6701SPIBinarySensors::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_read_ < this->update_interval_)
    return;
  this->last_read_ = now;
  if (!this->parent_->read_encoder())
    return;
  const uint8_t status = this->parent_->get_status();
  PUBLISH_STATE(this->push_button_binary_sensor_, (status & 0x04) != 0);
  PUBLISH_STATE(this->track_loss_binary_sensor_, (status & 0x08) != 0);
}

void MT6701SPIBinarySensors::dump_config() {
  ESP_LOGCONFIG(TAG, "MT6701 status:\n  Update Interval: %" PRIu32 " ms", this->update_interval_);
  LOG_BINARY_SENSOR("  ", "Push button", this->push_button_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Track loss", this->track_loss_binary_sensor_);
}

}  // namespace esphome::mt6701_spi
