#include "diyless_opentherm.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace diyless_opentherm {

DiyLessOpenThermComponent *component;

void DiyLessOpenThermComponent::initialize(char pinIn, char pinOut) {
  ESP_LOGD(TAG, "Initialize connection with DIYLess board: in=%d, out=%d.", pinIn, pinOut);
  openTherm = new ihormelnyk::OpenTherm(pinIn, pinOut, false);
}

void DiyLessOpenThermComponent::setup() {
  component = this;
  openTherm->begin(handleInterrupt, responseCallback);

  if (this->ch_enabled_) {
    this->ch_enabled_->add_on_state_callback([this](bool enabled) {
      if (this->CHEnabled != enabled) {
        ESP_LOGI(TAG, "%s CH", (enabled ? "Enabled" : "Disabled"));
        this->CHEnabled = enabled;
        this->processStatus();
      }
    });
  }
  if (this->dhw_enabled_) {
    this->dhw_enabled_->add_on_state_callback([this](bool enabled) {
      if (this->DHWEnabled != enabled) {
        ESP_LOGI(TAG, "%s DHW", (enabled ? "Enabled" : "Disabled"));
        this->DHWEnabled = enabled;
        this->processStatus();
      }
    });
  }
  if (this->cooling_enabled_) {
    this->cooling_enabled_->add_on_state_callback([this](bool enabled) {
      if (this->coolingEnabled != enabled) {
        ESP_LOGI(TAG, "%s cooling", (enabled ? "Enabled" : "Disabled"));
        this->coolingEnabled = enabled;
        this->processStatus();
      }
    });
  }
  if (this->ch_setpoint_temperature_) {
    this->ch_setpoint_temperature_->setup();
    this->ch_setpoint_temperature_->add_on_state_callback(
        [this](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
  if (this->dhw_setpoint_temperature_) {
    this->dhw_setpoint_temperature_->setup();
    this->dhw_setpoint_temperature_->add_on_state_callback(
        [this](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
}

void DiyLessOpenThermComponent::loop() {
  if (openTherm->isReady() && !buffer.empty()) {
    unsigned long request = buffer.front();
    buffer.pop();
    this->logMessage(ESP_LOG_DEBUG, "Sending request", request);
    openTherm->sendRequestAsync(request);
  }
  if (millis() - lastMillis > 2000) {
    // The CH setpoint must be written at a fast interval or the boiler
    // might revert to a build-in default as a safety measure.
    lastMillis = millis();
    this->enqueueRequest(openTherm->buildRequest(ihormelnyk::OpenThermMessageType::WRITE_DATA,
                                                 ihormelnyk::OpenThermMessageID::TSet,
                                                 openTherm->temperatureToData(this->ch_setpoint_temperature_->state)));
    if (this->confirmedDHWSetpoint != this->dhw_setpoint_temperature_->state) {
      this->enqueueRequest(
          openTherm->buildRequest(ihormelnyk::OpenThermMessageType::WRITE_DATA, ihormelnyk::OpenThermMessageID::TdhwSet,
                                  openTherm->temperatureToData(this->dhw_setpoint_temperature_->state)));
    }
  }
  openTherm->process();
  yield();
}

void DiyLessOpenThermComponent::update() {
  this->enqueueRequest(
      openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA, ihormelnyk::OpenThermMessageID::Tret, 0));
  this->enqueueRequest(
      openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA, ihormelnyk::OpenThermMessageID::Tboiler, 0));
  this->enqueueRequest(openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA,
                                               ihormelnyk::OpenThermMessageID::CHPressure, 0));
  this->enqueueRequest(openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA,
                                               ihormelnyk::OpenThermMessageID::RelModLevel, 0));
  this->enqueueRequest(openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA,
                                               ihormelnyk::OpenThermMessageID::RBPflags, 0));
  if (!this->DHWMinMaxRead) {
    this->enqueueRequest(openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                 ihormelnyk::OpenThermMessageID::TdhwSetUBTdhwSetLB, 0));
  }
  if (!this->CHMinMaxRead) {
    this->enqueueRequest(openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                 ihormelnyk::OpenThermMessageID::MaxTSetUBMaxTSetLB, 0));
  }
  this->processStatus();
}

void DiyLessOpenThermComponent::logMessage(esp_log_level_t level, const char *preMessage, unsigned long message) {
  switch (level) {
    case ESP_LOG_DEBUG:
      ESP_LOGD(TAG, "%s: %s(%i, 0x%04hX)", preMessage, this->formatMessageType(message), openTherm->getDataID(message),
               openTherm->getUInt(message));
      break;
    default:
      ESP_LOGW(TAG, "%s: %s(%i, 0x%04hX)", preMessage, this->formatMessageType(message), openTherm->getDataID(message),
               openTherm->getUInt(message));
  }
}

void DiyLessOpenThermComponent::publish_sensor_state(sensor::Sensor *sensor, float state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void DiyLessOpenThermComponent::publish_binary_sensor_state(binary_sensor::BinarySensor *sensor, bool state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void DiyLessOpenThermComponent::processStatus() {
  // Fields: CH enabled | DHW enabled | cooling | outside temperature compensation | central heating 2
  unsigned int data =
      this->CHEnabled | (this->DHWEnabled << 1) | (this->coolingEnabled << 2) | (false << 3) | (false << 4);
  data <<= 8;
  this->enqueueRequest(openTherm->buildRequest(ihormelnyk::OpenThermMessageType::READ_DATA,
                                               ihormelnyk::OpenThermMessageID::Status, data));
}

void DiyLessOpenThermComponent::enqueueRequest(unsigned long request) {
  if (this->buffer.size() > 20) {
    this->logMessage(ESP_LOG_WARN, "Queue full. Discarded request", request);
  } else {
    this->buffer.push(request);
    this->logMessage(ESP_LOG_DEBUG, "Enqueued request", request);
  }
}

const char *DiyLessOpenThermComponent::formatMessageType(unsigned long message) {
  return openTherm->messageTypeToString(openTherm->getMessageType(message));
}

}  // namespace diyless_opentherm
}  // namespace esphome
