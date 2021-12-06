#include "emporia_vue.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emporia_vue {

static const char *const TAG = "emporia_vue";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
EmporiaVueComponent *global_emporia_vue_component = nullptr;

void EmporiaVueComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Emporia Vue");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Sensor Poll Interval: %dms", this->sensor_poll_interval_);

  for (auto *phase : this->phases_) {
    std::string wire;
    switch (phase->get_input_wire()) {
      case PhaseInputWire::BLACK:
        wire = "BLACK";
        break;
      case PhaseInputWire::RED:
        wire = "RED";
        break;
      case PhaseInputWire::BLUE:
        wire = "BLUE";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Phase Config");
    ESP_LOGCONFIG(TAG, "    Wire: %s", wire.c_str());
    ESP_LOGCONFIG(TAG, "    Calibration: %f", phase->get_calibration());
    LOG_SENSOR("    ", "Voltage", phase->get_voltage_sensor());
  }

  for (auto *ct_sensor : this->ct_sensors_) {
    LOG_SENSOR("  ", "CT", ct_sensor);
    ESP_LOGCONFIG(TAG, "    Phase Calibration: %f", ct_sensor->get_phase()->get_calibration());
    ESP_LOGCONFIG(TAG, "    CT Port Index: %d", ct_sensor->get_input_port());
  }
}

void EmporiaVueComponent::setup() {
#ifdef USING_OTA_COMPONENT
  // OTA callback to prevent the i2c task to crash
  if (this->ota_) {
    ESP_LOGV(TAG, "Adding OTA state callback");
    this->ota_->add_on_state_callback([this](ota::OTAState state, float var, uint8_t error_code) {
      eTaskState i2c_request_task_status = eTaskGetState(this->i2c_request_task_);

      if (state == ota::OTAState::OTA_STARTED &&
          (i2c_request_task_status == eRunning || i2c_request_task_status == eReady ||
           i2c_request_task_status == eBlocked)) {
        ESP_LOGV(TAG, "OTA Update started - Suspending i2c_request_task_");
        vTaskSuspend(this->i2c_request_task_);
      }
      if (state == ota::OTAState::OTA_ERROR && i2c_request_task_status == eSuspended) {
        ESP_LOGV(TAG, "OTA Update failed - Resuming i2c_request_task_");
        vTaskResume(this->i2c_request_task_);
      }
    });
  }
#endif

  global_emporia_vue_component = this;

  this->i2c_data_queue_ = xQueueCreate(1, sizeof(SensorReading));
  xTaskCreatePinnedToCore(&EmporiaVueComponent::i2c_request_task, "i2c_request_task", 4096, nullptr, 0,
                          &this->i2c_request_task_, 0);
}

void EmporiaVueComponent::i2c_request_task(void *pv) {
  const TickType_t poll_interval = global_emporia_vue_component->get_sensor_poll_interval() / portTICK_PERIOD_MS;
  TickType_t last_poll;
  uint8_t last_sequence_num = 0;

  while (true) {
    last_poll = xTaskGetTickCount();
    SensorReading sensor_reading;

    i2c::ErrorCode err =
        global_emporia_vue_component->read(reinterpret_cast<uint8_t *>(&sensor_reading), sizeof(sensor_reading));

    if (err != i2c::ErrorCode::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to read from sensor due to I2C error %d", err);
    } else if (sensor_reading.end != 0) {
      ESP_LOGE(TAG, "Failed to read from sensor due to a malformed reading, should end in null bytes but is %d",
               sensor_reading.end);
    } else if (!sensor_reading.is_unread) {
      ESP_LOGV(TAG, "Ignoring sensor reading that is marked as read");
    } else {
      if (last_sequence_num && sensor_reading.sequence_num > last_sequence_num + 1) {
        ESP_LOGW(TAG, "Detected %d missing reading(s), data may not be accurate!",
                 sensor_reading.sequence_num - last_sequence_num - 1);
      }

      xQueueOverwrite(global_emporia_vue_component->i2c_data_queue_, &sensor_reading);
      ESP_LOGV(TAG, "Added sensor reading with sequence number %d to queue", sensor_reading.sequence_num);

      last_sequence_num = sensor_reading.sequence_num;
    }
    vTaskDelayUntil(&last_poll, poll_interval);
  }
}

void EmporiaVueComponent::loop() {
  SensorReading sensor_reading;

  if (xQueueReceive(this->i2c_data_queue_, &sensor_reading, 0) == pdTRUE) {
    ESP_LOGV(TAG, "Received sensor reading with sequence number %d from queue", sensor_reading.sequence_num);
    for (PhaseConfig *phase : this->phases_) {
      phase->update_from_reading(sensor_reading);
    }

    for (CTSensor *ct_sensor : this->ct_sensors_) {
      ct_sensor->update_from_reading(sensor_reading);
    }
  }
}

void PhaseConfig::update_from_reading(const SensorReading &sensor_reading) {
  if (this->voltage_sensor_) {
    float calibrated_voltage = sensor_reading.voltage[this->input_wire_] * this->calibration_;
    this->voltage_sensor_->publish_state(calibrated_voltage);
  }
}

int32_t PhaseConfig::extract_power_for_phase(const ReadingPowerEntry &power_entry) {
  switch (this->input_wire_) {
    case PhaseInputWire::BLACK:
      return power_entry.phase_black;
    case PhaseInputWire::RED:
      return power_entry.phase_red;
    case PhaseInputWire::BLUE:
      return power_entry.phase_blue;
    default:
      ESP_LOGE(TAG, "Unsupported phase input wire, this should never happen");
      return -1;
  }
}

void CTSensor::update_from_reading(const SensorReading &sensor_reading) {
  ReadingPowerEntry power_entry = sensor_reading.power[this->input_port_];
  int32_t raw_power = this->phase_->extract_power_for_phase(power_entry);
  float calibrated_power = this->get_calibrated_power(raw_power);
  this->publish_state(calibrated_power);
}

float CTSensor::get_calibrated_power(int32_t raw_power) const {
  float calibration = this->phase_->get_calibration();

  float correction_factor = (this->input_port_ < 3) ? 5.5 : 22;

  return (raw_power * calibration) / correction_factor;
}

}  // namespace emporia_vue
}  // namespace esphome
