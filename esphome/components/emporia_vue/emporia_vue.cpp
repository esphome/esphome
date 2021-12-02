#include "emporia_vue.h"
#include "esphome/core/log.h"

#include <freertos/task.h>

namespace esphome {
namespace emporia_vue {

static const char *const TAG = "emporia_vue";

EmporiaVueComponent *global_emporia_vue_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void EmporiaVueComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Emporia Vue");
  LOG_I2C_DEVICE(this);

  // TODO: Log phases
  for (PhaseConfig *phase : this->phases_) {
    //LOG_SENSOR("  ", "Phase", phase);
  }


  for (CTSensor *ct_sensor : this->ct_sensors_) {
    LOG_SENSOR("  ", "CT", ct_sensor);
    // TODO: Log other details
  }
}

void EmporiaVueComponent::setup() {
  global_emporia_vue_component = this;

  this->i2c_data_queue_ = xQueueCreate(1, sizeof(EmporiaSensorData));
  xTaskCreatePinnedToCore(&EmporiaVueComponent::i2c_request_task,
                          "i2c_request_task",
                          8192,
                          nullptr,
                          0,
                          nullptr,
                          1
  );
}

void EmporiaVueComponent::i2c_request_task(void *pv) {
  const uint32_t NULL_CHECKSUM = 4294967295;
  TickType_t xLastWakeTime;
  const TickType_t xDelay = 240 / portTICK_PERIOD_MS;
  uint32_t last_checksum = NULL_CHECKSUM;

  while (true) {
    xLastWakeTime = xTaskGetTickCount();
    EmporiaSensorData data;
    i2c::ErrorCode error = global_emporia_vue_component->read(reinterpret_cast<uint8_t *>(&data), sizeof(data));

    if (data.read_flag == 0) {
      last_checksum = NULL_CHECKSUM;
    }

    // Discard messages not ending by 0x0000
    if (error == i2c::ErrorCode::ERROR_OK && data.read_flag == 3 && (last_checksum != data.checksum) && data.end == 0) {
      last_checksum = data.checksum;
      xQueueOverwrite(global_emporia_vue_component->i2c_data_queue_, &data);
    }

    vTaskDelayUntil(&xLastWakeTime, xDelay);
  }
}

void EmporiaVueComponent::loop() {
  EmporiaSensorData data;

  if (xQueueReceive(this->i2c_data_queue_, &data, 0 ) == pdTRUE) {
    for (PhaseConfig *phase : this->phases_) {
      phase->update_from_data(data);
    }

    for (CTSensor *ct_sensor : this->ct_sensors_) {
      ct_sensor->update_from_data(data);
    }
  }
}

void PhaseConfig::update_from_data(EmporiaSensorData &data) {
  float calibrated_voltage = data.voltage[this->input_wire_] * this->calibration_;
  this->voltage_sensor_.publish_state(calibrated_voltage);
}

int32_t PhaseConfig::extract_power_for_phase(const PowerDataEntry &entry) {
  switch (this->input_wire_) {
    case PhaseInputColor::BLACK:
      return entry.phase_one;
    case PhaseInputColor::RED:
      return entry.phase_two;
    case PhaseInputColor::BLUE:
      return entry.phase_three;
    default:
      return -1;
  }
}

void CTSensor::update_from_data(const EmporiaSensorData &data) {
  PowerDataEntry entry = data.power[this->ct_input_];
  int32_t raw_power = this->phase_->extract_power_for_phase(entry);
  float calibrated_power = this->get_calibrated_power(raw_power);
  this->publish_state(calibrated_power);
}

double CTSensor::get_calibrated_power(int32_t raw_power) {
  double calibration = this->phase_->get_calibration();

  double correction_factor = (this->ct_input_ < 3) ? 5.5 : 22;

  return CTSensor::get_calibrated_power(raw_power, calibration, correction_factor);
}

double CTSensor::get_calibrated_power(int32_t raw_power, double calibration, double correction_factor) {
  return (raw_power * calibration) / correction_factor;
}


}  // namespace emporia_vue
}  // namespace esphome
