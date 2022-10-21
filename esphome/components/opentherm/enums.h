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
  STATUS,                        // flag8 / flag8  Device and Boiler Status flags.
  T_SET,                         // f8.8  Control setpoint  ie CH  water temperature setpoint (°C)
  M_CONFIG_M_MEMBER_I_DCODE,     // flag8 / u8  Device Configuration Flags /  Device MemberID Code
  S_CONFIG_S_MEMBER_I_DCODE,     // flag8 / u8  Boiler Configuration Flags /  Boiler MemberID Code
  COMMAND,                       // u8 / u8  Remote Command
  AS_FFLAGS,                     // / OEM-fault-code  flag8 / u8  Application-specific fault flags and OEM fault code
  RB_PFLAGS,                     // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags
  COOLING_CONTROL,               // f8.8  Cooling control signal (%)
  TSET_C_H2,                     // f8.8  Control setpoint for 2e CH circuit (°C)
  TR_OVERRIDE,                   // f8.8  Remote override room setpoint
  TSP,                           // u8 / u8  Number of Transparent-Boiler-Parameters supported by boiler
  TS_PINDEX_TS_PVALUE,           // u8 / u8  Index number / Value of referred-to transparent boiler parameter.
  FH_BSIZE,                      // u8 / u8  Size of Fault-History-Buffer supported by boiler
  FH_BINDEX_FH_BVALUE,           // u8 / u8  Index number / Value of referred-to fault-history buffer entry.
  MAX_REL_MOD_LEVEL_SETTING,     // f8.8  Maximum relative modulation level setting (%)
  MAX_CAPACITY_MIN_MOD_LEVEL,    // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%)
  TR_SET,                        // f8.8  Room Setpoint (°C)
  REL_MOD_LEVEL,                 // f8.8  Relative Modulation Level (%)
  CH_PRESSURE,                   // f8.8  Water pressure in CH circuit  (bar)
  DHW_FLOW_RATE,                 // f8.8  Water flow rate in DHW circuit. (litres/minute)
  DAY_TIME,                      // special / u8  Day of Week and Time of Day
  DATE,                          // u8 / u8  Calendar date
  YEAR,                          // u16  Calendar year
  TR_SET_C_H2,                   // f8.8  Room Setpoint for 2nd CH circuit (°C)
  TR,                            // f8.8  Room temperature (°C)
  TBOILER,                       // f8.8  Boiler flow water temperature (°C)
  TDHW,                          // f8.8  DHW temperature (°C)
  TOUTSIDE,                      // f8.8  Outside temperature (°C)
  TRET,                          // f8.8  Return water temperature (°C)
  TSTORAGE,                      // f8.8  Solar storage temperature (°C)
  TCOLLECTOR,                    // f8.8  Solar collector temperature (°C)
  TFLOW_C_H2,                    // f8.8  Flow water temperature CH2 circuit (°C)
  TDHW2,                         // f8.8  Domestic hot water temperature 2 (°C)
  TEXHAUST,                      // s16  Boiler exhaust temperature (°C)
  TDHW_SET_UB_TDHW_SET_LB = 48,  // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
  MAX_T_SET_UB_MAX_T_SET_LB,     // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
  HCRATIO_UB_HCRATIO_LB,         // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment
  TDHW_SET = 56,                 // f8.8  DHW setpoint (°C)    (Remote parameter 1)
  MAX_T_SET,                     // f8.8  Max CH water setpoint (°C)  (Remote parameters 2)
  HCRATIO,                       // f8.8  OTC heat curve ratio (°C)  (Remote parameter 3)
  REMOTE_OVERRIDE_FUNCTION =
      100,                    // flag8 / -  Function of manual and program changes in device and remote room setpoint.
  OEM_DIAGNOSTIC_CODE = 115,  // u16  OEM-specific diagnostic/service code
  BURNER_STARTS,              // u16  Number of starts burner
  CH_PUMP_STARTS,             // u16  Number of starts CH pump
  DHW_PUMP_VALVE_STARTS,      // u16  Number of starts DHW pump/valve
  DHW_BURNER_STARTS,          // u16  Number of starts burner during DHW mode
  BURNER_OPERATION_HOURS,     // u16  Number of hours that burner is in operation (i.e. flame on)
  CH_PUMP_OPERATION_HOURS,    // u16  Number of hours that CH pump has been running
  DHW_PUMP_VALVE_OPERATION_HOURS,  // u16  Number of hours that DHW pump has been running or DHW valve has been opened
  DHW_BURNER_OPERATION_HOURS,      // u16  Number of hours that burner is in operation during DHW mode
  OPEN_THERM_VERSION_DEVICE,  // f8.8  The implemented version of the OpenTherm Protocol Specification in the device.
  OPEN_THERM_VERSION_BOILER,  // f8.8  The implemented version of the OpenTherm Protocol Specification in the boiler.
  DEVICE_VERSION,             // u8 / u8  Device product version number and type
  BOILER_VERSION,             // u8 / u8  Boiler product version number and type
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
