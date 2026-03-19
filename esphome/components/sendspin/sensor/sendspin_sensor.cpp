#include "sendspin_sensor.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_SENSOR)

#include <string>

#ifdef USE_SENDSPIN_METADATA
#include <esp_timer.h>
#endif

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.sensor";

void SendspinSensor::dump_config() { LOG_SENSOR("", "Sendspin", this); }

#ifdef USE_SENDSPIN_METADATA
void SendspinSensor::publish_if_changed_(float value) {
  if (this->get_raw_state() != value) {
    this->publish_state(value);
  }
}

void SendspinSensor::schedule_publish_(const ServerMetadataStateObject &metadata, float value) {
  int64_t client_target = this->parent_->get_client_time(metadata.timestamp);
  if (client_target != 0) {
    int64_t delay_us = client_target - esp_timer_get_time();
    if (delay_us > 0) {
      uint32_t delay_ms = static_cast<uint32_t>(std::min(delay_us / 1000, (int64_t) 30000));
      this->set_timeout("metadata", delay_ms, [this, value]() { this->publish_if_changed_(value); });
      return;
    }
  }
  this->publish_if_changed_(value);
}
#endif

void SendspinSensor::setup() {
  switch (this->sensor_type_) {
#ifdef USE_SENDSPIN_METADATA
    case SendspinSensorTypes::TRACK_PROGRESS: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.progress.has_value()) {
          this->schedule_publish_(metadata, metadata.progress.value().track_progress);
        }
      });
      break;
    }
    case SendspinSensorTypes::TRACK_DURATION: {
      this->parent_->add_metadata_callback([this](const ServerMetadataStateObject &metadata) {
        if (metadata.progress.has_value()) {
          this->schedule_publish_(metadata, metadata.progress.value().track_duration);
        }
      });
      break;
    }
#endif
    default: {
      this->parent_->add_sensor_callback([this](const SendspinSensorUpdate &sensor_update) {
        if (sensor_update.type == this->sensor_type_)
          this->publish_state(sensor_update.value);
      });
      break;
    }
  }
}

}  // namespace sendspin
}  // namespace esphome

#endif
