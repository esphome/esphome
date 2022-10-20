#include "globals.h"
#include "diyless_opentherm.h"

namespace esphome {
namespace diyless_opentherm {

ihormelnyk::OpenTherm *openTherm;

ICACHE_RAM_ATTR void handleInterrupt() {
  openTherm->handleInterrupt();
}

void responseCallback(unsigned long response, ihormelnyk::OpenThermResponseStatus responseStatus) {
  if (responseStatus == ihormelnyk::OpenThermResponseStatus::SUCCESS) {
    component->logMessage(ESP_LOG_DEBUG, "Received response", response);
    switch (openTherm->getDataID(response)) {
      case ihormelnyk::OpenThermMessageID::Status:
        component->publish_binary_sensor_state(component->ch_active_, openTherm->isCentralHeatingActive(response));
        component->publish_binary_sensor_state(component->dhw_active_, openTherm->isHotWaterActive(response));
        component->publish_binary_sensor_state(component->cooling_active_, openTherm->isCoolingActive(response));
        component->publish_binary_sensor_state(component->flame_active_, openTherm->isFlameOn(response));
        component->publish_binary_sensor_state(component->fault_, openTherm->isFault(response));
        component->publish_binary_sensor_state(component->diagnostic_, openTherm->isDiagnostic(response));
        break;
      case ihormelnyk::OpenThermMessageID::Tret:
        component->publish_sensor_state(component->return_temperature_, openTherm->getFloat(response));
        break;
      case ihormelnyk::OpenThermMessageID::Tboiler:
        component->publish_sensor_state(component->boiler_temperature_, openTherm->getFloat(response));
        break;
      case ihormelnyk::OpenThermMessageID::CHPressure:
        component->publish_sensor_state(component->pressure_, openTherm->getFloat(response));
        break;
      case ihormelnyk::OpenThermMessageID::RelModLevel:
        component->publish_sensor_state(component->modulation_, openTherm->getFloat(response));
        break;
      case ihormelnyk::OpenThermMessageID::TdhwSetUBTdhwSetLB:
        component->DHWMinMaxRead = true;
        component->publish_sensor_state(component->dhw_max_temperature_, response >> 8 & 0xFF);
        component->publish_sensor_state(component->dhw_min_temperature_, response & 0xFF);
        break;
      case ihormelnyk::OpenThermMessageID::MaxTSetUBMaxTSetLB:
        component->CHMinMaxRead = true;
        component->publish_sensor_state(component->ch_max_temperature_, response >> 8 & 0xFF);
        component->publish_sensor_state(component->ch_min_temperature_, response & 0xFF);
        break;
      case ihormelnyk::OpenThermMessageID::TdhwSet:
        if (openTherm->getMessageType(response) == ihormelnyk::OpenThermMessageType::WRITE_ACK) {
          component->confirmedDHWSetpoint = openTherm->getFloat(response);
        }
        break;
      default: break;
    }
  } else if (responseStatus == ihormelnyk::OpenThermResponseStatus::NONE) {
    ESP_LOGW(TAG, "OpenTherm is not initialized");
  } else if (responseStatus == ihormelnyk::OpenThermResponseStatus::TIMEOUT) {
    ESP_LOGW(TAG, "Request timeout");
  } else if (responseStatus == ihormelnyk::OpenThermResponseStatus::INVALID) {
    component->logMessage(ESP_LOG_WARN, "Received invalid response", response);
  }
}

} // namespace diyless_opentherm
} // namespace esphome
