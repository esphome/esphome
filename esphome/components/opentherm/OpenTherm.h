/*
OpenTherm.h - OpenTherm Library for the ESP8266/Arduino platform
https://github.com/ihormelnyk/OpenTherm
http://ihormelnyk.com/pages/OpenTherm
Licensed under MIT license
Copyright 2018, Ihor Melnyk

Adapted for ESPHome by Oleg Tarasov in 2023

Frame Structure:
P MGS-TYPE SPARE DATA-ID  DATA-VALUE
0 000      0000  00000000 00000000 00000000
*/

#ifndef OpenTherm_h
#define OpenTherm_h

#include "esphome/core/hal.h"

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

namespace esphome {
namespace opentherm {

enum OpenThermResponseStatus { NONE, SUCCESS, INVALID, TIMEOUT };

enum OpenThermMessageType {
  /*  Master to Slave */
  READ_DATA = 0,
  READ = READ_DATA,  // for backwared compatibility
  WRITE_DATA = 1,
  WRITE = WRITE_DATA,  // for backwared compatibility
  INVALID_DATA = 2,
  RESERVED = 3,
  /* Slave to Master */
  READ_ACK = 4,
  WRITE_ACK = 5,
  DATA_INVALID = 6,
  UNKNOWN_DATA_ID = 7
};

// A lot of obscure stuff is using these exact constants, so just mute clang-tidy for now.
// NOLINTBEGIN
enum OpenThermMessageID {
  Status,                   // flag8 / flag8  Master and Slave Status flags.
  TSet,                     // f8.8  Control setpoint  ie CH  water temperature setpoint (°C)
  MConfigMMemberIDcode,     // flag8 / u8  Master Configuration Flags /  Master MemberID Code
  SConfigSMemberIDcode,     // flag8 / u8  Slave Configuration Flags /  Slave MemberID Code
  Command,                  // u8 / u8  Remote Command
  ASFflags,                 // / OEM-fault-code  flag8 / u8  Application-specific fault flags and OEM fault code
  RBPflags,                 // flag8 / flag8  Remote boiler parameter transfer-enable & read/write flags
  CoolingControl,           // f8.8  Cooling control signal (%)
  TsetCH2,                  // f8.8  Control setpoint for 2e CH circuit (°C)
  TrOverride,               // f8.8  Remote override room setpoint
  TSP,                      // u8 / u8  Number of Transparent-Slave-Parameters supported by slave
  TSPindexTSPvalue,         // u8 / u8  Index number / Value of referred-to transparent slave parameter.
  FHBsize,                  // u8 / u8  Size of Fault-History-Buffer supported by slave
  FHBindexFHBvalue,         // u8 / u8  Index number / Value of referred-to fault-history buffer entry.
  MaxRelModLevelSetting,    // f8.8  Maximum relative modulation level setting (%)
  MaxCapacityMinModLevel,   // u8 / u8  Maximum boiler capacity (kW) / Minimum boiler modulation level(%)
  TrSet,                    // f8.8  Room Setpoint (°C)
  RelModLevel,              // f8.8  Relative Modulation Level (%)
  CHPressure,               // f8.8  Water pressure in CH circuit  (bar)
  DHWFlowRate,              // f8.8  Water flow rate in DHW circuit. (litres/minute)
  DayTime,                  // special / u8  Day of Week and Time of Day
  Date,                     // u8 / u8  Calendar date
  Year,                     // u16  Calendar year
  TrSetCH2,                 // f8.8  Room Setpoint for 2nd CH circuit (°C)
  Tr,                       // f8.8  Room temperature (°C)
  Tboiler,                  // f8.8  Boiler flow water temperature (°C)
  Tdhw,                     // f8.8  DHW temperature (°C)
  Toutside,                 // f8.8  Outside temperature (°C)
  Tret,                     // f8.8  Return water temperature (°C)
  Tstorage,                 // f8.8  Solar storage temperature (°C)
  Tcollector,               // f8.8  Solar collector temperature (°C)
  TflowCH2,                 // f8.8  Flow water temperature CH2 circuit (°C)
  Tdhw2,                    // f8.8  Domestic hot water temperature 2 (°C)
  Texhaust,                 // s16  Boiler exhaust temperature (°C)
  TdhwSetUBTdhwSetLB = 48,  // s8 / s8  DHW setpoint upper & lower bounds for adjustment  (°C)
  MaxTSetUBMaxTSetLB,       // s8 / s8  Max CH water setpoint upper & lower bounds for adjustment  (°C)
  HcratioUBHcratioLB,       // s8 / s8  OTC heat curve ratio upper & lower bounds for adjustment
  TdhwSet = 56,             // f8.8  DHW setpoint (°C)    (Remote parameter 1)
  MaxTSet,                  // f8.8  Max CH water setpoint (°C)  (Remote parameters 2)
  Hcratio,                  // f8.8  OTC heat curve ratio (°C)  (Remote parameter 3)
  RemoteOverrideFunction =
      100,                     // flag8 / -  Function of manual and program changes in master and remote room setpoint.
  OEMDiagnosticCode = 115,     // u16  OEM-specific diagnostic/service code
  BurnerStarts,                // u16  Number of starts burner
  CHPumpStarts,                // u16  Number of starts CH pump
  DHWPumpValveStarts,          // u16  Number of starts DHW pump/valve
  DHWBurnerStarts,             // u16  Number of starts burner during DHW mode
  BurnerOperationHours,        // u16  Number of hours that burner is in operation (i.e. flame on)
  CHPumpOperationHours,        // u16  Number of hours that CH pump has been running
  DHWPumpValveOperationHours,  // u16  Number of hours that DHW pump has been running or DHW valve has been opened
  DHWBurnerOperationHours,     // u16  Number of hours that burner is in operation during DHW mode
  OpenThermVersionMaster,      // f8.8  The implemented version of the OpenTherm Protocol Specification in the master.
  OpenThermVersionSlave,       // f8.8  The implemented version of the OpenTherm Protocol Specification in the slave.
  MasterVersion,               // u8 / u8  Master product version number and type
  SlaveVersion,                // u8 / u8  Slave product version number and type
};
// NOLINTEND

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

class OpenTherm {
 public:
  OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, bool is_slave = false);
  volatile OpenThermStatus cur_status;
  void begin();
  void begin(void (*process_response_callback)(uint32_t, OpenThermResponseStatus));
  bool is_ready();
  uint32_t send_request(uint32_t request);
  bool send_response(uint32_t request);
  bool send_request_aync(uint32_t request);
  uint32_t build_request(OpenThermMessageType type, OpenThermMessageID id, uint16_t data);
  uint32_t build_response(OpenThermMessageType type, OpenThermMessageID id, uint16_t data);
  uint32_t get_last_response();
  OpenThermResponseStatus get_last_response_status();
  const char *status_to_string(OpenThermResponseStatus status);
  void process();
  void end();

  static void handle_interrupt(OpenTherm *arg);

  bool parity(uint32_t frame);
  OpenThermMessageType get_message_type(uint32_t message);
  OpenThermMessageID get_data_id(uint32_t frame);
  const char *message_type_to_string(OpenThermMessageType message_type);
  bool is_valid_request(uint32_t request);
  bool is_valid_response(uint32_t response);

  // requests
  uint32_t build_set_boiler_status_request(bool enable_central_heating, bool enable_hot_water = false,
                                           bool enable_cooling = false,
                                           bool enable_outside_temperature_compensation = false,
                                           bool enable_central_heating_2 = false);
  uint32_t build_set_boiler_temperature_request(float temperature);
  uint32_t build_get_boiler_temperature_request();

  // responses
  bool is_fault(uint32_t response);
  bool is_central_heating_active(uint32_t response);
  bool is_hot_water_active(uint32_t response);
  bool is_flame_on(uint32_t response);
  bool is_cooling_active(uint32_t response);
  bool is_diagnostic(uint32_t response);
  uint16_t get_u_int(uint32_t response) const;
  float get_float(uint32_t response) const;
  uint16_t temperature_to_data(float temperature);

  // basic requests
  uint32_t set_boiler_status(bool enable_central_heating, bool enable_hot_water = false, bool enable_cooling = false,
                             bool enable_outside_temperature_compensation = false,
                             bool enable_central_heating_2 = false);
  bool set_boiler_temperature(float temperature);
  float get_boiler_temperature();
  float get_return_temperature();
  bool set_dhw_setpoint(float temperature);
  float get_dhw_temperature();
  float get_modulation();
  float get_pressure();
  uint8_t get_fault();

 private:
  InternalGPIOPin *in_pin_;
  InternalGPIOPin *out_pin_;
  ISRInternalGPIOPin isr_in_pin_;
  ISRInternalGPIOPin isr_out_pin_;
  bool is_slave_;

  volatile uint32_t response_;
  volatile OpenThermResponseStatus response_status_;
  volatile uint32_t response_timestamp_;
  volatile uint8_t response_bit_index_;

  int read_state_();
  void set_active_state_();
  void set_idle_state_();
  void activate_boiler_();

  void send_bit_(bool high);
  void (*process_response_callback_)(uint32_t, OpenThermResponseStatus);
};

#ifndef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR
#endif

#ifndef IRAM_ATTR
#define IRAM_ATTR ICACHE_RAM_ATTR
#endif

}  // namespace opentherm
}  // namespace esphome

#endif  // OpenTherm_h