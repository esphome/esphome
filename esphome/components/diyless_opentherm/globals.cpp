#include "globals.h"
#include "diyless_opentherm.h"

namespace esphome {
namespace diyless_opentherm {

ihormelnyk::OpenTherm *openTherm;

IRAM_ATTR void handle_interrupt() { openTherm->handle_interrupt(); }

void response_callback(unsigned long response, ihormelnyk::OpenThermResponseStatus response_status) {
  if (response_status == ihormelnyk::OpenThermResponseStatus::SUCCESS) {
    component->log_message(ESP_LOG_DEBUG, "Received response", response);
    switch (openTherm->get_data_id(response)) {
      case ihormelnyk::OpenThermMessageID::STATUS:
        component->publish_binary_sensor_state(component->ch_active_binary_sensor_,
                                               openTherm->is_central_heating_active(response));
        component->publish_binary_sensor_state(component->dhw_active_binary_sensor_,
                                               openTherm->is_hot_water_active(response));
        component->publish_binary_sensor_state(component->cooling_active_binary_sensor_,
                                               openTherm->is_cooling_active(response));
        component->publish_binary_sensor_state(component->flame_active_binary_sensor_,
                                               openTherm->is_flame_on(response));
        component->publish_binary_sensor_state(component->fault_binary_sensor_, openTherm->is_fault(response));
        component->publish_binary_sensor_state(component->diagnostic_binary_sensor_,
                                               openTherm->is_diagnostic(response));
        break;
      case ihormelnyk::OpenThermMessageID::TRET:
        component->publish_sensor_state(component->return_temperature_sensor_, openTherm->get_float(response));
        break;
      case ihormelnyk::OpenThermMessageID::TBOILER:
        component->publish_sensor_state(component->boiler_temperature_sensor_, openTherm->get_float(response));
        break;
      case ihormelnyk::OpenThermMessageID::CH_PRESSURE:
        component->publish_sensor_state(component->pressure_sensor_, openTherm->get_float(response));
        break;
      case ihormelnyk::OpenThermMessageID::REL_MOD_LEVEL:
        component->publish_sensor_state(component->modulation_sensor_, openTherm->get_float(response));
        break;
      case ihormelnyk::OpenThermMessageID::TDHW_SET_UB_TDHW_SET_LB:
        component->dhw_min_max_read = true;
        component->publish_sensor_state(component->dhw_max_temperature_sensor_, response >> 8 & 0xFF);
        component->publish_sensor_state(component->dhw_min_temperature_sensor_, response & 0xFF);
        break;
      case ihormelnyk::OpenThermMessageID::MAX_T_SET_UB_MAX_T_SET_LB:
        component->ch_min_max_read = true;
        component->publish_sensor_state(component->ch_max_temperature_sensor_, response >> 8 & 0xFF);
        component->publish_sensor_state(component->ch_min_temperature_sensor_, response & 0xFF);
        break;
      case ihormelnyk::OpenThermMessageID::TDHW_SET:
        if (openTherm->get_message_type(response) == ihormelnyk::OpenThermMessageType::WRITE_ACK) {
          component->confirmed_dhw_setpoint = openTherm->get_float(response);
        }
        break;
      default:
        break;
    }
  } else if (response_status == ihormelnyk::OpenThermResponseStatus::NONE) {
    ESP_LOGW(TAG, "OpenTherm is not initialized");
  } else if (response_status == ihormelnyk::OpenThermResponseStatus::TIMEOUT) {
    ESP_LOGW(TAG, "Request timeout");
  } else if (response_status == ihormelnyk::OpenThermResponseStatus::INVALID) {
    component->log_message(ESP_LOG_WARN, "Received invalid response", response);
  }
}

}  // namespace diyless_opentherm
}  // namespace esphome
