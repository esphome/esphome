#include "opentherm_base.h"
#include "esphome/core/helpers.h"
#include <string>
#include <esp_err.h>

namespace esphome {
namespace opentherm {

using std::string;
using std::to_string;

static const char *const TAG = "opentherm";

#define TO_STRING_MEMBER(name) \
  case name: \
    return #name;

const char *operation_mode_to_str(OperationMode mode) {
  switch (mode) {
    TO_STRING_MEMBER(IDLE)
    TO_STRING_MEMBER(LISTEN)
    TO_STRING_MEMBER(READ)
    TO_STRING_MEMBER(RECEIVED)
    TO_STRING_MEMBER(WRITE)
    TO_STRING_MEMBER(SENT)
    TO_STRING_MEMBER(ERROR_PROTOCOL)
    TO_STRING_MEMBER(ERROR_TIMEOUT)
    TO_STRING_MEMBER(ERROR_RMT)
    default:
      return "<INVALID>";
  }
}
const char *protocol_error_to_str(ProtocolErrorType error_type) {
  switch (error_type) {
    TO_STRING_MEMBER(NO_ERROR)
    TO_STRING_MEMBER(NO_TRANSITION)
    TO_STRING_MEMBER(INVALID_START_STOP_BIT)
    TO_STRING_MEMBER(PARITY_ERROR)
    TO_STRING_MEMBER(NO_CHANGE_TOO_LONG)
    TO_STRING_MEMBER(INVALID_DURATION)
    TO_STRING_MEMBER(INSUFFICIENT_DATA)
    default:
      return "<INVALID>";
  }
}
const char *timer_error_to_str(TimerErrorType error_type) {
  switch (error_type) {
    TO_STRING_MEMBER(NO_TIMER_ERROR)
    TO_STRING_MEMBER(SET_ALARM_VALUE_ERROR)
    TO_STRING_MEMBER(TIMER_START_ERROR)
    TO_STRING_MEMBER(TIMER_PAUSE_ERROR)
    TO_STRING_MEMBER(SET_COUNTER_VALUE_ERROR)
    default:
      return "<INVALID>";
  }
}
const char *message_type_to_str(MessageType message_type) {
  switch (message_type) {
    TO_STRING_MEMBER(READ_DATA)
    TO_STRING_MEMBER(READ_ACK)
    TO_STRING_MEMBER(WRITE_DATA)
    TO_STRING_MEMBER(WRITE_ACK)
    TO_STRING_MEMBER(INVALID_DATA)
    TO_STRING_MEMBER(DATA_INVALID)
    TO_STRING_MEMBER(UNKNOWN_DATAID)
    default:
      return "<INVALID>";
  }
}

const char *message_id_to_str(MessageId id) {
  switch (id) {
    TO_STRING_MEMBER(STATUS)
    TO_STRING_MEMBER(CH_SETPOINT)
    TO_STRING_MEMBER(CONTROLLER_CONFIG)
    TO_STRING_MEMBER(DEVICE_CONFIG)
    TO_STRING_MEMBER(COMMAND_CODE)
    TO_STRING_MEMBER(FAULT_FLAGS)
    TO_STRING_MEMBER(REMOTE)
    TO_STRING_MEMBER(COOLING_CONTROL)
    TO_STRING_MEMBER(CH2_SETPOINT)
    TO_STRING_MEMBER(CH_SETPOINT_OVERRIDE)
    TO_STRING_MEMBER(TSP_COUNT)
    TO_STRING_MEMBER(TSP_COMMAND)
    TO_STRING_MEMBER(FHB_SIZE)
    TO_STRING_MEMBER(FHB_COMMAND)
    TO_STRING_MEMBER(MAX_MODULATION_LEVEL)
    TO_STRING_MEMBER(MAX_BOILER_CAPACITY)
    TO_STRING_MEMBER(ROOM_SETPOINT)
    TO_STRING_MEMBER(MODULATION_LEVEL)
    TO_STRING_MEMBER(CH_WATER_PRESSURE)
    TO_STRING_MEMBER(DHW_FLOW_RATE)
    TO_STRING_MEMBER(DAY_TIME)
    TO_STRING_MEMBER(DATE)
    TO_STRING_MEMBER(YEAR)
    TO_STRING_MEMBER(ROOM_SETPOINT_CH2)
    TO_STRING_MEMBER(ROOM_TEMP)
    TO_STRING_MEMBER(FEED_TEMP)
    TO_STRING_MEMBER(DHW_TEMP)
    TO_STRING_MEMBER(OUTSIDE_TEMP)
    TO_STRING_MEMBER(RETURN_WATER_TEMP)
    TO_STRING_MEMBER(SOLAR_STORE_TEMP)
    TO_STRING_MEMBER(SOLAR_COLLECT_TEMP)
    TO_STRING_MEMBER(FEED_TEMP_CH2)
    TO_STRING_MEMBER(DHW2_TEMP)
    TO_STRING_MEMBER(EXHAUST_TEMP)
    TO_STRING_MEMBER(FAN_SPEED)
    TO_STRING_MEMBER(FLAME_CURRENT)
    TO_STRING_MEMBER(ROOM_TEMP_CH2)
    TO_STRING_MEMBER(REL_HUMIDITY)
    TO_STRING_MEMBER(DHW_BOUNDS)
    TO_STRING_MEMBER(CH_BOUNDS)
    TO_STRING_MEMBER(OTC_CURVE_BOUNDS)
    TO_STRING_MEMBER(DHW_SETPOINT)
    TO_STRING_MEMBER(MAX_CH_SETPOINT)
    TO_STRING_MEMBER(OTC_CURVE_RATIO)
    TO_STRING_MEMBER(HVAC_STATUS)
    TO_STRING_MEMBER(REL_VENT_SETPOINT)
    TO_STRING_MEMBER(DEVICE_VENT)
    TO_STRING_MEMBER(HVAC_VER_ID)
    TO_STRING_MEMBER(REL_VENTILATION)
    TO_STRING_MEMBER(REL_HUMID_EXHAUST)
    TO_STRING_MEMBER(EXHAUST_CO2)
    TO_STRING_MEMBER(SUPPLY_INLET_TEMP)
    TO_STRING_MEMBER(SUPPLY_OUTLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_INLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_OUTLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_FAN_SPEED)
    TO_STRING_MEMBER(SUPPLY_FAN_SPEED)
    TO_STRING_MEMBER(REMOTE_VENTILATION_PARAM)
    TO_STRING_MEMBER(NOM_REL_VENTILATION)
    TO_STRING_MEMBER(HVAC_NUM_TSP)
    TO_STRING_MEMBER(HVAC_IDX_TSP)
    TO_STRING_MEMBER(HVAC_FHB_SIZE)
    TO_STRING_MEMBER(HVAC_FHB_IDX)
    TO_STRING_MEMBER(RF_SIGNAL)
    TO_STRING_MEMBER(DHW_MODE)
    TO_STRING_MEMBER(OVERRIDE_FUNC)
    TO_STRING_MEMBER(SOLAR_MODE_FLAGS)
    TO_STRING_MEMBER(SOLAR_ASF)
    TO_STRING_MEMBER(SOLAR_VERSION_ID)
    TO_STRING_MEMBER(SOLAR_PRODUCT_ID)
    TO_STRING_MEMBER(SOLAR_NUM_TSP)
    TO_STRING_MEMBER(SOLAR_IDX_TSP)
    TO_STRING_MEMBER(SOLAR_FHB_SIZE)
    TO_STRING_MEMBER(SOLAR_FHB_IDX)
    TO_STRING_MEMBER(SOLAR_STARTS)
    TO_STRING_MEMBER(SOLAR_HOURS)
    TO_STRING_MEMBER(SOLAR_ENERGY)
    TO_STRING_MEMBER(SOLAR_TOTAL_ENERGY)
    TO_STRING_MEMBER(FAILED_BURNER_STARTS)
    TO_STRING_MEMBER(BURNER_FLAME_LOW)
    TO_STRING_MEMBER(OEM_DIAGNOSTIC)
    TO_STRING_MEMBER(BURNER_STARTS)
    TO_STRING_MEMBER(CH_PUMP_STARTS)
    TO_STRING_MEMBER(DHW_PUMP_STARTS)
    TO_STRING_MEMBER(DHW_BURNER_STARTS)
    TO_STRING_MEMBER(BURNER_HOURS)
    TO_STRING_MEMBER(CH_PUMP_HOURS)
    TO_STRING_MEMBER(DHW_PUMP_HOURS)
    TO_STRING_MEMBER(DHW_BURNER_HOURS)
    TO_STRING_MEMBER(OT_VERSION_CONTROLLER)
    TO_STRING_MEMBER(OT_VERSION_DEVICE)
    TO_STRING_MEMBER(VERSION_CONTROLLER)
    TO_STRING_MEMBER(VERSION_DEVICE)
    default:
      return "<INVALID>";
  }
}

void debug_data(OpenthermData &data) {
  ESP_LOGD(TAG, "%s %s %s %s", format_bin(data.type).c_str(), format_bin(data.id).c_str(),
           format_bin(data.valueHB).c_str(), format_bin(data.valueLB).c_str());
  ESP_LOGD(TAG, "type: %s; id: %s; HB: %s; LB: %s; uint_16: %s; float: %s",
           message_type_to_str((MessageType) data.type), to_string(data.id).c_str(), to_string(data.valueHB).c_str(),
           to_string(data.valueLB).c_str(), to_string(data.u16()).c_str(), to_string(data.f88()).c_str());
}

void debug_error(OpenThermProtocolError &error) {
  ESP_LOGD(TAG,
           "OpenTherm protocol error: %s\n"
           "Bit index: %u\n"
           "Data: %s",
           protocol_error_to_str(error.error_type), error.bit_index, format_hex(error.data).c_str());
}

float OpenthermData::f88() { return ((float) this->s16()) / 256.0; }

void OpenthermData::f88(float value) { this->s16((int16_t) (value * 256)); }

uint16_t OpenthermData::u16() {
  uint16_t const value = this->valueHB;
  return (value << 8) | this->valueLB;
}

void OpenthermData::u16(uint16_t value) {
  this->valueLB = value & 0xFF;
  this->valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData::s16() {
  int16_t const value = this->valueHB;
  return (value << 8) | this->valueLB;
}

void OpenthermData::s16(int16_t value) {
  this->valueLB = value & 0xFF;
  this->valueHB = (value >> 8) & 0xFF;
}

}  // namespace opentherm
}  // namespace esphome
