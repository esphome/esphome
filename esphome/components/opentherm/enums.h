#pragma once

namespace esphome {
namespace opentherm {

enum OpenThermResponseStatus { NONE, SUCCESS, INVALID, TIMEOUT };

enum OpenThermMessageType {
  /*  Device to Boiler */
  READ_DATA = 0b000,
  WRITE_DATA = 0b001,
  INVALID_DATA = 0b010,
  RESERVED = 0b011,
  /* Boiler to Device */
  READ_ACK = 0b100,
  WRITE_ACK = 0b101,
  DATA_INVALID = 0b110,
  UNKNOWN_DATA_ID = 0b111
};

enum OpenThermMessageID {
  // Class 1: Control and Status Information
  STATUS = 0,                 // flag8 / flag8 -> Device and Boiler Status flags.
  CH_SETPOINT = 1,            // f8.8 -> Control setpoint for CH circuit (°C)
  APP_SPEC_FAULT_FLAGS = 5,   // flag8 / u8 -> Application-specific fault flags and OEM fault code
  CH_SETPOINT_2 = 8,          // f8.8 -> Control setpoint for 2nd CH circuit (°C)
  OEM_DIAGNOSTIC_CODE = 115,  // u16 -> OEM-specific diagnostic/service code
  // Class 2: Configuration Information
  DEVICE_CONFIGURATION = 2,  // flag8 / u8 -> Device Configuration Flags / Device MemberID Code
  BOILER_CONFIGURATION = 3,  // flag8 / u8 -> Boiler Configuration Flags / Boiler MemberID Code
  OT_VERSION_DEVICE = 124,   // f8.8 -> The implemented version of the OpenTherm Protocol Specification in the device.
  OT_VERSION_BOILER = 125,   // f8.8 - The implemented version of the OpenTherm Protocol Specification in the boiler.
  DEVICE_VERSION = 126,      // u8 / u8 -> Device product version number and type
  BOILER_VERSION = 127,      // u8 / u8 -> Boiler product version number and type
  // Class 3: Remote Commands
  COMMAND = 4,  // u8 / u8 -> Remote Command
  // Class 4: Sensor and Informational Data
  ROOM_SETPOINT = 16,              // f8.8 -> Room Setpoint (°C)
  REL_MOD_LEVEL = 17,              // f8.8 -> Relative Modulation Level (%)
  CH_PRESSURE = 18,                // f8.8 -> Water pressure in CH circuit  (bar)
  DHW_FLOW_RATE = 19,              // f8.8 -> Water flow rate in DHW circuit. (litres/minute)
  DAY_TIME = 20,                   // special / u8 -> Day of Week and Time of Day
  DATE = 21,                       // u8 / u8 -> Calendar date
  YEAR = 22,                       // u16 -> Calendar year
  ROOM_SETPOINT_2 = 23,            // f8.8 -> Room Setpoint for 2nd CH circuit (°C)
  ROOM_TEMP = 24,                  // f8.8 -> Room temperature (°C)
  BOILER_WATER_TEMP = 25,          // f8.8 -> Boiler flow water temperature (°C)
  DHW_TEMP = 26,                   // f8.8 -> DHW temperature (°C)
  OUTSIDE_TEMP = 27,               // f8.8 -> Outside temperature (°C)
  RETURN_WATER_TEMP = 28,          // f8.8 -> Return water temperature (°C)
  SOLAR_STORAGE_TEMP = 29,         // f8.8 -> Solar storage temperature (°C)
  SOLAR_COLL_TEMP = 30,            // f8.8 -> Solar collector temperature (°C)
  BOILER_WATER_TEMP_2 = 31,        // f8.8 -> Flow water temperature CH2 circuit (°C)
  DHW_TEMP_2 = 32,                 // f8.8 -> Domestic hot water temperature 2 (°C)
  BOILER_EXHAUST_TEMP = 33,        // s16 -> Boiler exhaust temperature (°C)
  BURNER_STARTS = 116,             // u16 -> Number of starts burner
  CH_PUMP_STARTS = 117,            // u16 -> Number of starts CH pump
  DHW_PUMP_VALVE_STARTS = 118,     // u16 -> Number of starts DHW pump/valve
  DHW_BURNER_STARTS = 119,         // u16 -> Number of starts burner during DHW mode
  BURNER_OPS_HOURS = 120,          // u16 -> Number of hours that burner is in operation (i.e. flame on)
  CH_PUMP_OPS_HOURS = 121,         // u16 -> Number of hours that CH pump has been running
  DHW_PUMP_VALVE_OPS_HOURS = 122,  // u16 -> Number of hours that DHW pump has been running or DHW valve has been opened
  DHW_BURNER_OPS_HOURS = 123,      // u16 -> Number of hours that burner is in operation during DHW mode
  // Class 5: Pre-Defined Remote Boiler Parameters
  REMOTE_PARAM_FLAGS = 6,  // flag8 / flag8 -> Remote boiler parameter transfer-enable & read/write flags
  DHW_TEMP_MAX_MIN = 48,   // s8 / s8 -> DHW setpoint upper & lower bounds for adjustment (°C)
  CH_TEMP_MAX_MIN = 49,    // s8 / s8 -> Max CH water setpoint upper & lower bounds for adjustment (°C)
  DHW_SETPOINT = 56,       // f8.8 -> DHW setpoint (°C) (Remote parameter 1)
  MAX_CH_SETPOINT = 57,    // f8.8 -> Max CH water setpoint (°C) (Remote parameters 2)
  // Class 6: Transparent Slave Parameters
  NR_OF_TSPS = 10,       // u8 / u8 -> Number of Transparent-Boiler-Parameters supported by boiler
  TSP_INDEX_VALUE = 11,  // u8 / u8 -> Index number / Value of referred-to transparent boiler parameter.
  // Class 7: Fault History Data
  FHB_SIZE = 12,         // u8 / u8 -> Size of Fault-History-Buffer supported by boiler
  FHB_INDEX_VALUE = 13,  // u8 / u8 -> Index number / Value of referred-to fault-history buffer entry.
  // Class 8: Control of Special Applications
  COOLING_CONTROL = 7,              // f8.8 -> Cooling control signal (%)
  MAX_REL_MOD_LEVEL_SETTING = 14,   // f8.8 -> Maximum relative modulation level setting (%)
  MAX_CAPACITY_MIN_MOD_LEVEL = 15,  // u8 / u8 -> Maximum boiler capacity (kW) / Minimum boiler modulation level(%)
  ROOM_TEMP_OVERRIDE = 9,           // f8.8 -> Remote override room setpoint
  REMOTE_FCT_OVERRIDE = 100,  // flag8 / - -> Function of manual and program changes in device and remote room setpoint.
};

enum OpenThermStatus {
  NOT_INITIALIZED,
  READY,
  DELAY,
  REQUEST_SENDING,
  RESPONSE_WAITING,
  RESPONSE_START_BIT,
  RESPONSE_RECEIVING,
  RESPONSE_READY,
  RESPONSE_INVALID
};

}  // namespace opentherm
}  // namespace esphome
