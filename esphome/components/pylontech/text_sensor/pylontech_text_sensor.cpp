#include "pylontech_text_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pylontech {

ESPHOME_LOG_TAG(TAG, "pylontech.textsensor");

void PylontechTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Pylontech Text Sensor:\n"
                " Battery %u",
                this->bat_num_);
  LOG_TEXT_SENSOR("  ", "Base state", this->base_state_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Voltage state", this->voltage_state_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Current state", this->current_state_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Temperature state", this->temperature_state_text_sensor_);
}

void PylontechTextSensor::on_line_read(PylontechListener::LineContents *line) {
  PUBLISH_STATE(this->base_state_text_sensor_, line->base_st);
  PUBLISH_STATE(this->voltage_state_text_sensor_, line->volt_st);
  PUBLISH_STATE(this->current_state_text_sensor_, line->curr_st);
  PUBLISH_STATE(this->temperature_state_text_sensor_, line->temp_st);
}

}  // namespace esphome::pylontech
