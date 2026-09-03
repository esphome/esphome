#pragma once

#include <array>
#include <memory>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "datalink.h"
#include "flag_bits.h"

namespace esphome::opentherm42 {

// §5.2/§5.3: which conversation is currently in flight, so handle_response_() knows how to interpret
// the reply. Every kind maps to exactly one data-id; see build_next_request_()/handle_response_().
enum class RequestKind : uint8_t {
  // §5.3.2 Class 2, ID 3: boiler configuration flags + boiler MemberID code. Read once at startup
  // (§5.2.1: "Must sent message with READ_DATA (at least at start up)").
  BOILER_CONFIG,
  // §5.3.1 Class 1, ID 0: the master/boiler status exchange -- the protocol's mandatory heartbeat.
  STATUS,
  // §5.3.1 Class 1, ID 1: control setpoint.
  CONTROL_SETPOINT,
  // §5.3.1 Class 1, ID 8: control setpoint 2 (TsetCH2).
  CONTROL_SETPOINT_2,
  // §5.3.1 Class 1, ID 70: master status for ventilation/heat-recovery <-> its status reply, the same
  // one-conversation write+read pattern as STATUS.
  VENTILATION_STATUS,
  // §5.3.1 Class 1, ID 71: control setpoint ventilation/heat-recovery.
  CONTROL_SETPOINT_VENTILATION,
  // §5.3.1 Class 1, ID 5: application-specific fault flags + OEM fault code.
  FAULT_FLAGS,
  // §5.3.1 Class 1, ID 72: application-specific fault flags + OEM fault code, ventilation/heat-recovery.
  VENTILATION_FAULT_FLAGS,
  // §5.3.1 Class 1, ID 101: master/boiler solar storage status.
  SOLAR_STORAGE_STATUS,
  // §5.3.1 Class 1, ID 102: solar storage specific fault flags (HB reserved) + OEM fault code.
  SOLAR_STORAGE_FAULT_FLAGS,
  // §5.3.1 Class 1, ID 115: OEM diagnostic code.
  OEM_DIAGNOSTIC_CODE,
  // §5.3.1 Class 1, ID 73: OEM diagnostic code, ventilation/heat-recovery.
  OEM_DIAGNOSTIC_CODE_VENTILATION,
  // §5.3.2 Class 2, ID 2: this master's own configuration flags + MemberID code. Written once at
  // startup (recommended by §5.3.2 before control/status information is transmitted).
  MASTER_CONFIG,
  // §5.3.2 Class 2, ID 124: this master's own OpenTherm protocol version. Written once at startup.
  MASTER_OPENTHERM_VERSION,
  // §5.3.2 Class 2, ID 126: this master's own product version number and type. Written once at startup.
  MASTER_PRODUCT_VERSION,
  // §5.3.2 Class 2, ID 74: configuration ventilation/heat-recovery.
  VENTILATION_CONFIGURATION,
  // §5.3.2 Class 2, ID 103: Solar Storage configuration.
  SOLAR_STORAGE_CONFIGURATION,
  // §5.3.2 Class 2, ID 125: OpenTherm version implemented by the boiler.
  OPENTHERM_VERSION_BOILER,
  // §5.3.2 Class 2, ID 127: boiler product version number and type.
  PRODUCT_VERSION_BOILER,
  // §5.3.2 Class 2, ID 75: OpenTherm version implemented by the ventilation/heat-recovery system.
  OPENTHERM_VERSION_VENTILATION,
  // §5.3.2 Class 2, ID 76: ventilation/heat-recovery product version number and type.
  PRODUCT_VERSION_VENTILATION,
  // §5.3.2 Class 2, ID 104: Solar Storage product version number and type.
  PRODUCT_VERSION_SOLAR_STORAGE,
  // §5.3.2 Class 2, ID 93: brand identification string, read one character at a time. Read once at
  // startup, only if a text_sensor is configured for it.
  BRAND,
  // §5.3.2 Class 2, ID 94: brand version string, same one-character-at-a-time protocol as ID 93.
  BRAND_VERSION,
  // §5.3.2 Class 2, ID 95: brand serial number string, same protocol as ID 93.
  BRAND_SERIAL_NUMBER,
  // §5.3.3 Class 3, ID 4: a remote request command. Sent on demand (button press), not scheduled.
  REMOTE_REQUEST,

  // §5.3.4 Class 4: write-only numbers this master provides to the boiler.
  ROOM_SETPOINT,      // ID 16
  ROOM_SETPOINT_CH2,  // ID 23
  ROOM_TEMPERATURE,   // ID 24
  TRCH2,              // ID 37
  // §5.3.4 Class 4, IDs 20/21/22: Day-of-week/Time, Date, Year -- written once per essential rotation
  // from the configured time_id, so the boiler's clock stays in sync with this master's.
  DAY_TIME,
  DATE,
  YEAR,
  // §5.3.4 Class 4, IDs 27/38/78/79: R/W ids -- WRITE_DATA if the "_set" number is configured,
  // otherwise READ_DATA if the plain sensor is configured (see build_next_request_()).
  OUTSIDE_TEMPERATURE,            // ID 27
  RELATIVE_HUMIDITY,              // ID 38
  RELATIVE_HUMIDITY_EXHAUST_AIR,  // ID 78
  CO2_LEVEL,                      // ID 79
  // §5.3.4 Class 4, ID 35: HB Boiler fan speed Setpoint + LB Boiler fan speed -- two sensors from one
  // conversation, so it doesn't fit the single-sensor SimpleSensorInfo table below.
  BOILER_FAN_SPEED,

  // §5.3.4 Class 4: read-only sensors dispatched generically through the SimpleSensorInfo table in
  // hub.cpp (single value per conversation, no bit decomposition) -- see find_simple_sensor_().
  RELATIVE_MODULATION_LEVEL,             // ID 17
  CH_WATER_PRESSURE,                     // ID 18
  DHW_FLOW_RATE,                         // ID 19
  BOILER_WATER_TEMPERATURE,              // ID 25
  DHW_TEMPERATURE,                       // ID 26
  RETURN_WATER_TEMPERATURE,              // ID 28
  SOLAR_STORAGE_TEMPERATURE,             // ID 29
  SOLAR_COLLECTOR_TEMPERATURE,           // ID 30
  FLOW_TEMPERATURE_CH2,                  // ID 31
  DHW2_TEMPERATURE,                      // ID 32
  EXHAUST_TEMPERATURE,                   // ID 33
  BOILER_HEAT_EXCHANGER_TEMPERATURE,     // ID 34
  FLAME_CURRENT,                         // ID 36
  RELATIVE_VENTILATION,                  // ID 77
  SUPPLY_INLET_TEMPERATURE,              // ID 80
  SUPPLY_OUTLET_TEMPERATURE,             // ID 81
  EXHAUST_INLET_TEMPERATURE,             // ID 82
  EXHAUST_OUTLET_TEMPERATURE,            // ID 83
  ACTUAL_EXHAUST_FAN_SPEED,              // ID 84
  ACTUAL_INLET_FAN_SPEED,                // ID 85
  COOLING_OPERATION_HOURS,               // ID 96
  POWER_CYCLES,                          // ID 97
  ELECTRICITY_PRODUCER_STARTS,           // ID 109
  ELECTRICITY_PRODUCER_HOURS,            // ID 110
  ELECTRICITY_PRODUCTION,                // ID 111
  CUMULATIVE_ELECTRICITY_PRODUCTION,     // ID 112
  NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS,  // ID 113
  NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW,  // ID 114
  SUCCESSFUL_BURNER_STARTS,              // ID 116
  CH_PUMP_STARTS,                        // ID 117
  DHW_PUMP_VALVE_STARTS,                 // ID 118
  DHW_BURNER_STARTS,                     // ID 119
  BURNER_OPERATION_HOURS,                // ID 120
  CH_PUMP_OPERATION_HOURS,               // ID 121
  DHW_PUMP_VALVE_OPERATION_HOURS,        // ID 122
  DHW_BURNER_OPERATION_HOURS,            // ID 123

  // §5.3.5 Class 5, ID 6: Remote-parameter transfer-enable + read/write flags for DHW Setpoint / max
  // CHsetpoint (HB and LB read in one conversation).
  REMOTE_PARAMETER_FLAGS,
  // §5.3.5 Class 5, ID 86: same, for ventilation/heat-recovery's Nominal ventilation value.
  REMOTE_PARAMETER_FLAGS_VENTILATION,
  // §5.3.5 Class 5, ID 48: HB DHWsetp upp-bound, LB DHWsetp low-bound (two signed 8-bit values).
  DHWSETP_BOUNDS,
  // §5.3.5 Class 5, ID 49: HB max CHsetp upp-bound, LB max CHsetp low-bnd.
  MAX_CHSETP_BOUNDS,
  // §5.3.5 Class 5, IDs 56/57/87: R/W ids -- WRITE_DATA if the "_set" number is configured, otherwise
  // READ_DATA if the plain sensor is configured (same pattern as Class 4's IDs 27/38/78/79).
  DHW_SETPOINT,
  MAX_CH_WATER_SETPOINT,
  NOMINAL_VENTILATION_VALUE,

  // §5.3.6 Class 6, IDs 10/88/105 HB: number of TSPs supported, one per family.
  NUMBER_OF_TSPS,
  NUMBER_OF_TSPS_VENTILATION,
  NUMBER_OF_TSPS_SOLAR_STORAGE,
  // §5.3.6 Class 6, IDs 11/89/106: a transparent-boiler-parameter read or write, at whichever
  // configured slot (see TspSlot) is due next -- see build_next_request_()'s tsp_write_pending_ check
  // for on-demand writes and the informational-rotation case for the periodic read cycle.
  TSP,

  // §5.3.7 Class 7, IDs 12/90/107 HB: size of the fault history buffer, one per family.
  FAULT_HISTORY_BUFFER_SIZE,
  FAULT_HISTORY_BUFFER_SIZE_VENTILATION,
  FAULT_HISTORY_BUFFER_SIZE_SOLAR_STORAGE,
  // §5.3.7 Class 7, IDs 13/91/108: a fault-history-buffer entry read, at whichever configured slot
  // (see FhbSlot) is due next -- purely read-only, so unlike TSP there's no write-pending priority
  // check, just the informational-rotation case.
  FHB,
};

class OpenTherm42Hub;

// How a SimpleSensorInfo's raw frame bytes convert to the value passed to sensor::Sensor::publish_state().
enum class SimpleValueKind : uint8_t {
  F88,    // frame.value_f88()
  S16,    // frame.value_s16() -- a plain signed integer, not fixed-point, despite also being 16 bits
  U16,    // frame.value_u16()
  U8_LB,  // frame.value_lb -- only the low byte carries data
  U8_HB,  // frame.value_hb -- only the high byte carries data
};

// Describes one single-value, non-bit-decomposed read-only data-id: which id to READ_DATA, how to
// convert the response into a float, which member holds its sensor pointer, and what to name it in log
// messages. OpenTherm42Hub::SIMPLE_SENSORS is the table every class after Class 1 registers its "plain"
// reads into; find_simple_sensor_() is the generic fallback build_next_request_()/handle_response_()/
// invalidate_response_() dispatch to for every RequestKind not given bespoke handling.
struct SimpleSensorInfo {
  RequestKind kind;
  uint8_t id;
  SimpleValueKind value_kind;
  sensor::Sensor *OpenTherm42Hub::*member;
  const char *log_name;
};

// The order startup-only conversations happen in, before the main essential/informational rotation
// begins. BOILER_CONFIG is retried indefinitely (§5.2.1: mandatory for the boiler to support); every
// other phase is attempted once (or, for the BRAND* phases, until that one string is fully read or
// rejected) and then skipped forever after, successful or not -- see advance_startup_phase_().
enum class StartupPhase : uint8_t {
  BOILER_CONFIG,
  MASTER_CONFIG,
  MASTER_OPENTHERM_VERSION,
  MASTER_PRODUCT_VERSION,
  BRAND,
  BRAND_VERSION,
  BRAND_SERIAL_NUMBER,
  DONE,
};

// §5.3.2 Class 2, IDs 93/94/95: a brand-identification string, assembled one ASCII character per
// conversation (index in the request's HB, character count in the response's HB, character itself in
// the response's LB). 51 = the largest legal index (49, per the ID 93/94/95 table's HB range) plus one
// content byte plus a null terminator.
struct BrandRead {
  text_sensor::TextSensor *sensor{nullptr};
  uint8_t next_index{0};
  std::array<char, 51> buffer{};
};

// §5.3.6 Class 6: one user-configured transparent-boiler-parameter slot. TSP values are opaque and
// manufacturer-specific (the protocol has no idea what they mean), so unlike every other class there's
// no fixed set of ids to expose -- the user names and indexes whichever slots their boiler documents.
struct TspSlot {
  uint8_t data_id;  // 11 (main), 89 (ventilation/heat-recovery), or 106 (Solar Storage)
  uint8_t index;    // TSP-index, 0..255
  number::Number *number{nullptr};
};

// §5.3.7 Class 7: one user-configured fault-history-buffer slot. Like TSP, purely read-only here.
struct FhbSlot {
  uint8_t data_id;  // 13 (main), 91 (ventilation/heat-recovery), or 108 (Solar Storage)
  uint8_t index;    // FHB-index, 0..255
  sensor::Sensor *sensor{nullptr};
};

// Declares set_<name>_switch()/set_<name>_binary_sensor(), storing the pointer at a fixed bit position
// within one of the hub's FlagWriteBits/FlagReadBits members. Used for every flag8 byte's individual
// bits across every class -- see flag_bits.h.
#define OT42_FLAG_WRITE_BIT(name, byte, bit) \
  void set_##name##_switch(switch_::Switch *s) { this->byte.bits[bit] = s; }
#define OT42_FLAG_READ_BIT(name, byte, bit) \
  void set_##name##_binary_sensor(binary_sensor::BinarySensor *s) { this->byte.bits[bit] = s; }
// Declares set_<name>_number()/set_<name>_sensor()/set_<name>_binary_sensor() for a standalone
// (non-flag-byte) entity backed by a single named member pointer.
#define OT42_SET_NUMBER(name, member) \
  void set_##name##_number(number::Number *n) { this->member = n; }
#define OT42_SET_SENSOR(name, member) \
  void set_##name##_sensor(sensor::Sensor *s) { this->member = s; }
#define OT42_SET_BINARY_SENSOR(name, member) \
  void set_##name##_binary_sensor(binary_sensor::BinarySensor *s) { this->member = s; }
#define OT42_SET_TEXT_SENSOR(name, member) \
  void set_##name##_text_sensor(text_sensor::TextSensor *s) { this->member.sensor = s; }

// OpenTherm 4.2 master. Talks directly to a single boiler -- see the OpenTherm Protocol
// Specification v4.2, §4.3.2: this component implements the master role only, not the optional
// gateway (chained intermediate device) role.
class OpenTherm42Hub : public Component {
 public:
  void set_in_pin(InternalGPIOPin *in_pin) { this->in_pin_ = in_pin; }
  void set_out_pin(InternalGPIOPin *out_pin) { this->out_pin_ = out_pin; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  // §5.3.2 Class 2: this master's own identity, written to the boiler once at startup. Static config,
  // not entities -- see opentherm42/__init__.py's CONF_CONTROLLER_* options.
  void set_controller_member_id_code(uint8_t member_id_code) { this->controller_member_id_code_ = member_id_code; }
  void set_controller_opentherm_version(float version) { this->controller_opentherm_version_ = version; }
  void set_controller_product_type(uint8_t product_type) { this->controller_product_type_ = product_type; }
  void set_controller_product_version(uint8_t product_version) { this->controller_product_version_ = product_version; }

  // §5.3.1 Class 1, ID 0 HB: Master status.
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_ch_enable, master_status_write_, 0)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_dhw_enable, master_status_write_, 1)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_cooling_enable, master_status_write_, 2)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_otc_active, master_status_write_, 3)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_ch2_enable, master_status_write_, 4)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_summer_winter_mode, master_status_write_, 5)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_dhw_blocking, master_status_write_, 6)

  // §5.3.1 Class 1, ID 70 HB: Master status for ventilation/heat-recovery.
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_for_ventilation_heat_recovery_ventilation_enable,
                      ventilation_status_write_, 0)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_for_ventilation_heat_recovery_bypass_position,
                      ventilation_status_write_, 1)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_for_ventilation_heat_recovery_bypass_mode,
                      ventilation_status_write_, 2)
  OT42_FLAG_WRITE_BIT(control_and_status_information_master_status_for_ventilation_heat_recovery_free_ventilation_mode,
                      ventilation_status_write_, 3)

  // §5.3.1 Class 1, IDs 1/8/71: numeric setpoints.
  OT42_SET_NUMBER(control_and_status_information_control_setpoint, control_setpoint_number_)
  OT42_SET_NUMBER(control_and_status_information_control_setpoint_2_tsetch2, control_setpoint_2_number_)
  OT42_SET_NUMBER(control_and_status_information_control_setpoint_ventilation_heat_recovery,
                  control_setpoint_ventilation_number_)

  // §5.3.1 Class 1, ID 0 LB: Boiler status.
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_fault_indication, boiler_status_read_, 0)
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_ch_mode, boiler_status_read_, 1)
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_dhw_mode, boiler_status_read_, 2)
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_flame_status, boiler_status_read_, 3)
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_cooling_status, boiler_status_read_, 4)
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_ch2_mode, boiler_status_read_, 5)
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_diagnostic_service_indication, boiler_status_read_, 6)
  OT42_FLAG_READ_BIT(control_and_status_information_boiler_status_electricity_production, boiler_status_read_, 7)

  // §5.3.1 Class 1, ID 70 LB: Status ventilation/heat-recovery (bits 5 and 7 are reserved).
  OT42_FLAG_READ_BIT(control_and_status_information_status_ventilation_heat_recovery_fault_indication,
                     ventilation_status_read_, 0)
  OT42_FLAG_READ_BIT(control_and_status_information_status_ventilation_heat_recovery_ventilation_mode,
                     ventilation_status_read_, 1)
  OT42_FLAG_READ_BIT(control_and_status_information_status_ventilation_heat_recovery_bypass_status,
                     ventilation_status_read_, 2)
  OT42_FLAG_READ_BIT(control_and_status_information_status_ventilation_heat_recovery_bypass_automatic_status,
                     ventilation_status_read_, 3)
  OT42_FLAG_READ_BIT(control_and_status_information_status_ventilation_heat_recovery_free_ventilation_status,
                     ventilation_status_read_, 4)
  OT42_FLAG_READ_BIT(control_and_status_information_status_ventilation_heat_recovery_diagnostic_indication,
                     ventilation_status_read_, 6)

  // §5.3.1 Class 1, ID 5 HB: Application-specific fault flags (bits 6,7 reserved); LB: OEM fault code.
  OT42_FLAG_READ_BIT(control_and_status_information_application_specific_fault_flags_service_request, fault_flags_read_,
                     0)
  OT42_FLAG_READ_BIT(control_and_status_information_application_specific_fault_flags_lockout_reset, fault_flags_read_,
                     1)
  OT42_FLAG_READ_BIT(control_and_status_information_application_specific_fault_flags_low_water_press, fault_flags_read_,
                     2)
  OT42_FLAG_READ_BIT(control_and_status_information_application_specific_fault_flags_gas_flame_fault, fault_flags_read_,
                     3)
  OT42_FLAG_READ_BIT(control_and_status_information_application_specific_fault_flags_air_press_fault, fault_flags_read_,
                     4)
  OT42_FLAG_READ_BIT(control_and_status_information_application_specific_fault_flags_water_over_temp, fault_flags_read_,
                     5)
  OT42_SET_SENSOR(control_and_status_information_oem_fault_code, oem_fault_code_sensor_)

  // §5.3.1 Class 1, ID 72 HB: Application-specific fault flags, ventilation/heat-recovery (bits 4-7
  // reserved); LB: OEM fault code ventilation/heat-recovery.
  OT42_FLAG_READ_BIT(
      control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_service_request,
      ventilation_fault_flags_read_, 0)
  OT42_FLAG_READ_BIT(
      control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_exhaust_fan_fault,
      ventilation_fault_flags_read_, 1)
  OT42_FLAG_READ_BIT(
      control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_inlet_fan_fault,
      ventilation_fault_flags_read_, 2)
  OT42_FLAG_READ_BIT(
      control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_frost_protection,
      ventilation_fault_flags_read_, 3)
  OT42_SET_SENSOR(control_and_status_information_oem_fault_code_ventilation_heat_recovery,
                  oem_fault_code_ventilation_sensor_)

  // §5.3.1 Class 1, ID 101: Master/boiler solar storage status (HB and LB each carry their own Solar
  // mode sub-field, at different bit offsets -- see handle_response_()).
  OT42_SET_BINARY_SENSOR(control_and_status_information_solar_storage_mode_and_status_fault_indication,
                         solar_storage_fault_indication_binary_sensor_)
  OT42_SET_SENSOR(control_and_status_information_master_solar_storage_status_solar_mode,
                  master_solar_storage_status_solar_mode_sensor_)
  OT42_SET_SENSOR(control_and_status_information_solar_storage_mode_and_status_solar_mode,
                  solar_storage_mode_and_status_solar_mode_sensor_)
  OT42_SET_SENSOR(control_and_status_information_solar_storage_mode_and_status_solar_status,
                  solar_storage_mode_and_status_solar_status_sensor_)

  // §5.3.1 Class 1, ID 102 LB: OEM fault code Solar Storage (HB is entirely reserved -- no entity).
  OT42_SET_SENSOR(control_and_status_information_oem_fault_code_solar_storage, oem_fault_code_solar_storage_sensor_)

  // §5.3.1 Class 1, IDs 115/73: OEM diagnostic codes.
  OT42_SET_SENSOR(control_and_status_information_oem_diagnostic_code, oem_diagnostic_code_sensor_)
  OT42_SET_SENSOR(control_and_status_information_oem_diagnostic_code_ventilation_heat_recovery,
                  oem_diagnostic_code_ventilation_sensor_)

  // §5.3.2 Class 2, ID 3 HB: Boiler configuration; LB: Boiler MemberID code.
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_dhw_present, boiler_configuration_read_, 0)
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_control_type, boiler_configuration_read_, 1)
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_cooling_config, boiler_configuration_read_, 2)
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_dhw_config, boiler_configuration_read_, 3)
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_master_low_off_and_pump_control_function,
                     boiler_configuration_read_, 4)
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_ch2_present, boiler_configuration_read_, 5)
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_remote_water_filling_function,
                     boiler_configuration_read_, 6)
  OT42_FLAG_READ_BIT(configuration_information_boiler_configuration_heat_cool_mode_control, boiler_configuration_read_,
                     7)
  OT42_SET_SENSOR(configuration_information_boiler_member_id_code, boiler_member_id_code_sensor_)

  // §5.3.2 Class 2, ID 74 HB: Configuration ventilation/heat-recovery (bits 3-7 reserved); LB: MemberID
  // code ventilation/heat-recovery.
  OT42_FLAG_READ_BIT(configuration_information_configuration_ventilation_heat_recovery_system_type,
                     ventilation_configuration_read_, 0)
  OT42_FLAG_READ_BIT(configuration_information_configuration_ventilation_heat_recovery_bypass,
                     ventilation_configuration_read_, 1)
  OT42_FLAG_READ_BIT(configuration_information_configuration_ventilation_heat_recovery_speed_control,
                     ventilation_configuration_read_, 2)
  OT42_SET_SENSOR(configuration_information_member_id_code_ventilation_heat_recovery,
                  member_id_code_ventilation_sensor_)

  // §5.3.2 Class 2, ID 103 HB bit 0: Solar Storage configuration: system type; LB: Solar Storage member ID.
  OT42_SET_BINARY_SENSOR(configuration_information_solar_storage_configuration_system_type,
                         solar_storage_configuration_system_type_binary_sensor_)
  OT42_SET_SENSOR(configuration_information_solar_storage_member_id, solar_storage_member_id_sensor_)

  // §5.3.2 Class 2, IDs 125/127: OpenTherm version + product version/type implemented by the boiler.
  OT42_SET_SENSOR(configuration_information_opentherm_version_boiler, opentherm_version_boiler_sensor_)
  OT42_SET_SENSOR(configuration_information_boiler_product_version_number_and_type_product_type,
                  boiler_product_type_sensor_)
  OT42_SET_SENSOR(configuration_information_boiler_product_version_number_and_type_product_version,
                  boiler_product_version_sensor_)

  // §5.3.2 Class 2, IDs 75/76: OpenTherm version + product version/type of the ventilation/heat-recovery system.
  OT42_SET_SENSOR(configuration_information_opentherm_version_ventilation_heat_recovery,
                  opentherm_version_ventilation_sensor_)
  OT42_SET_SENSOR(configuration_information_ventilation_heat_recovery_product_version_number_and_type_product_type,
                  ventilation_product_type_sensor_)
  OT42_SET_SENSOR(configuration_information_ventilation_heat_recovery_product_version_number_and_type_product_version,
                  ventilation_product_version_sensor_)

  // §5.3.2 Class 2, ID 104: Solar Storage product version number and type.
  OT42_SET_SENSOR(configuration_information_solar_storage_product_version_number_and_type_product_type,
                  solar_storage_product_type_sensor_)
  OT42_SET_SENSOR(configuration_information_solar_storage_product_version_number_and_type_product_version,
                  solar_storage_product_version_sensor_)

  // §5.3.2 Class 2, IDs 93/94/95: brand identification strings.
  OT42_SET_TEXT_SENSOR(configuration_information_brand, brand_)
  OT42_SET_TEXT_SENSOR(configuration_information_brand_version, brand_version_)
  OT42_SET_TEXT_SENSOR(configuration_information_brand_serial_number, brand_serial_number_)

  // §5.3.3 Class 3, ID 4: queues a remote request to be sent from the next available conversation
  // slot. Called by OpenTherm42RemoteRequestButton::press_action(); code is one of the values listed
  // under the ID 4 HB table (0 = back to normal operation, 1 = boiler lock-out reset, ...).
  void send_remote_request(uint8_t code) {
    this->remote_request_pending_ = true;
    this->remote_request_code_ = code;
  }
  OT42_SET_SENSOR(remote_request_last_response_code, remote_request_last_response_code_sensor_)

  // §5.3.4 Class 4, IDs 20/21/22: the clock this master's Day-of-week/Time, Date and Year writes are
  // sourced from. Left unset (nullptr), those three ids are simply never sent.
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }

  // §5.3.4 Class 4: write-only numbers.
  OT42_SET_NUMBER(sensor_and_informational_data_room_setpoint, room_setpoint_number_)
  OT42_SET_NUMBER(sensor_and_informational_data_room_setpoint_ch2, room_setpoint_ch2_number_)
  OT42_SET_NUMBER(sensor_and_informational_data_room_temperature, room_temperature_number_)
  OT42_SET_NUMBER(sensor_and_informational_data_trch2, trch2_number_)

  // §5.3.4 Class 4, IDs 27/38/78/79: R/W ids -- both directions' entities, see the RequestKind comment.
  OT42_SET_NUMBER(sensor_and_informational_data_outside_temperature_set, outside_temperature_number_)
  OT42_SET_SENSOR(sensor_and_informational_data_outside_temperature, outside_temperature_sensor_)
  OT42_SET_NUMBER(sensor_and_informational_data_relative_humidity_set, relative_humidity_number_)
  OT42_SET_SENSOR(sensor_and_informational_data_relative_humidity, relative_humidity_sensor_)
  OT42_SET_NUMBER(sensor_and_informational_data_relative_humidity_exhaust_air_set,
                  relative_humidity_exhaust_air_number_)
  OT42_SET_SENSOR(sensor_and_informational_data_relative_humidity_exhaust_air, relative_humidity_exhaust_air_sensor_)
  OT42_SET_NUMBER(sensor_and_informational_data_co2_level_set, co2_level_number_)
  OT42_SET_SENSOR(sensor_and_informational_data_co2_level, co2_level_sensor_)

  // §5.3.4 Class 4, ID 35: HB Boiler fan speed Setpoint, LB Boiler fan speed.
  OT42_SET_SENSOR(sensor_and_informational_data_boiler_fan_speed_setpoint, boiler_fan_speed_setpoint_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_boiler_fan_speed, boiler_fan_speed_sensor_)

  // §5.3.4 Class 4: read-only sensors (see the SimpleSensorInfo table in hub.cpp).
  OT42_SET_SENSOR(sensor_and_informational_data_relative_modulation_level, relative_modulation_level_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_ch_water_pressure, ch_water_pressure_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_dhw_flow_rate, dhw_flow_rate_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_boiler_water_temperature, boiler_water_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_dhw_temperature, dhw_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_return_water_temperature, return_water_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_solar_storage_temperature, solar_storage_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_solar_collector_temperature, solar_collector_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_flow_temperature_ch2, flow_temperature_ch2_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_dhw2_temperature, dhw2_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_exhaust_temperature, exhaust_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_boiler_heat_exchanger_temperature,
                  boiler_heat_exchanger_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_flame_current, flame_current_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_relative_ventilation, relative_ventilation_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_supply_inlet_temperature, supply_inlet_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_supply_outlet_temperature, supply_outlet_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_exhaust_inlet_temperature, exhaust_inlet_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_exhaust_outlet_temperature, exhaust_outlet_temperature_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_actual_exhaust_fan_speed, actual_exhaust_fan_speed_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_actual_inlet_fan_speed, actual_inlet_fan_speed_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_cooling_operation_hours, cooling_operation_hours_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_power_cycles, power_cycles_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_electricity_producer_starts, electricity_producer_starts_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_electricity_producer_hours, electricity_producer_hours_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_electricity_production, electricity_production_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_cumulative_electricity_production,
                  cumulative_electricity_production_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_number_of_unsuccessful_burner_starts,
                  number_of_unsuccessful_burner_starts_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_number_of_times_flame_signal_too_low,
                  number_of_times_flame_signal_too_low_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_successful_burner_starts, successful_burner_starts_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_ch_pump_starts, ch_pump_starts_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_dhw_pump_valve_starts, dhw_pump_valve_starts_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_dhw_burner_starts, dhw_burner_starts_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_burner_operation_hours, burner_operation_hours_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_ch_pump_operation_hours, ch_pump_operation_hours_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_dhw_pump_valve_operation_hours, dhw_pump_valve_operation_hours_sensor_)
  OT42_SET_SENSOR(sensor_and_informational_data_dhw_burner_operation_hours, dhw_burner_operation_hours_sensor_)

  // §5.3.5 Class 5, ID 6: Remote-parameter transfer-enable/read-write flags.
  OT42_FLAG_READ_BIT(pre_defined_remote_boiler_parameters_transfer_enable_flags_dhw_setpoint,
                     remote_parameter_transfer_enable_flags_read_, 0)
  OT42_FLAG_READ_BIT(pre_defined_remote_boiler_parameters_transfer_enable_flags_max_chsetpoint,
                     remote_parameter_transfer_enable_flags_read_, 1)
  OT42_FLAG_READ_BIT(pre_defined_remote_boiler_parameters_read_write_flags_dhw_setpoint,
                     remote_parameter_read_write_flags_read_, 0)
  OT42_FLAG_READ_BIT(pre_defined_remote_boiler_parameters_read_write_flags_max_chsetpoint,
                     remote_parameter_read_write_flags_read_, 1)

  // §5.3.5 Class 5, ID 86: same flags, for ventilation/heat-recovery's Nominal ventilation value.
  OT42_FLAG_READ_BIT(
      pre_defined_remote_boiler_parameters_transfer_enable_flags_ventilation_heat_recovery_nominal_ventilation_value,
      remote_parameter_transfer_enable_flags_ventilation_read_, 0)
  OT42_FLAG_READ_BIT(
      pre_defined_remote_boiler_parameters_read_write_flags_ventilation_heat_recovery_nominal_ventilation_value,
      remote_parameter_read_write_flags_ventilation_read_, 0)

  // §5.3.5 Class 5, IDs 48/49: adjustment bounds.
  OT42_SET_SENSOR(pre_defined_remote_boiler_parameters_dhwsetp_upper_bound, dhwsetp_upper_bound_sensor_)
  OT42_SET_SENSOR(pre_defined_remote_boiler_parameters_dhwsetp_lower_bound, dhwsetp_lower_bound_sensor_)
  OT42_SET_SENSOR(pre_defined_remote_boiler_parameters_max_chsetp_upper_bound, max_chsetp_upper_bound_sensor_)
  OT42_SET_SENSOR(pre_defined_remote_boiler_parameters_max_chsetp_lower_bound, max_chsetp_lower_bound_sensor_)

  // §5.3.5 Class 5, IDs 56/57/87: the remote boiler parameters themselves.
  OT42_SET_NUMBER(pre_defined_remote_boiler_parameters_dhw_setpoint_set, dhw_setpoint_number_)
  OT42_SET_SENSOR(pre_defined_remote_boiler_parameters_dhw_setpoint, dhw_setpoint_sensor_)
  OT42_SET_NUMBER(pre_defined_remote_boiler_parameters_max_ch_water_setpoint_set, max_ch_water_setpoint_number_)
  OT42_SET_SENSOR(pre_defined_remote_boiler_parameters_max_ch_water_setpoint, max_ch_water_setpoint_sensor_)
  OT42_SET_NUMBER(pre_defined_remote_boiler_parameters_nominal_ventilation_value_set, nominal_ventilation_value_number_)
  OT42_SET_SENSOR(pre_defined_remote_boiler_parameters_nominal_ventilation_value, nominal_ventilation_value_sensor_)

  // §5.3.6 Class 6, IDs 10/88/105 HB: number of TSPs supported, one per family.
  OT42_SET_SENSOR(transparent_boiler_parameters_number_of_tsps, number_of_tsps_sensor_)
  OT42_SET_SENSOR(transparent_boiler_parameters_number_of_tsps_ventilation_heat_recovery,
                  number_of_tsps_ventilation_sensor_)
  OT42_SET_SENSOR(transparent_boiler_parameters_number_of_tsps_solar_storage, number_of_tsps_solar_storage_sensor_)

  // §5.3.6 Class 6, IDs 11/89/106: registers one user-configured TSP slot (data_id identifies which
  // family), returning its index into tsp_slots_ so the owning OpenTherm42TspNumber can identify
  // itself in write_tsp() calls.
  size_t add_tsp_slot(uint8_t data_id, uint8_t index, number::Number *number) {
    this->tsp_slots_.push_back(TspSlot{data_id, index, number});
    return this->tsp_slots_.size() - 1;
  }
  // Called by OpenTherm42TspNumber::control(); queues an on-demand write, serviced ahead of the next
  // essential/informational conversation (same priority tier as Class 3's remote requests).
  void write_tsp(size_t slot_index, uint8_t value) {
    this->tsp_write_pending_ = true;
    this->tsp_write_slot_index_ = slot_index;
    this->tsp_write_value_ = value;
  }

  // §5.3.7 Class 7, IDs 12/90/107 HB: size of the fault history buffer, one per family.
  OT42_SET_SENSOR(fault_history_data_size_of_fault_buffer, fault_history_buffer_size_sensor_)
  OT42_SET_SENSOR(fault_history_data_size_of_fault_buffer_ventilation_heat_recovery,
                  fault_history_buffer_size_ventilation_sensor_)
  OT42_SET_SENSOR(fault_history_data_size_of_fault_buffer_solar_storage,
                  fault_history_buffer_size_solar_storage_sensor_)

  // §5.3.7 Class 7, IDs 13/91/108: registers one user-configured fault-history-buffer slot.
  void add_fhb_slot(uint8_t data_id, uint8_t index, sensor::Sensor *sensor) {
    this->fhb_slots_.push_back(FhbSlot{data_id, index, sensor});
  }

 protected:
  // §4.3.1: minimum time between the end of one conversation and the start of the next.
  static constexpr uint32_t MASTER_WAIT_TIME_MS = 100;
  // §4.3.1: the legal boiler answering-time window is 20-400 ms from the end of the master's
  // transmission; 400 ms is the longest a compliant boiler is allowed to take.
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 400;

  // Populates essential_requests_/informational_requests_ from whichever entities got configured --
  // called once from setup().
  void build_schedule_();
  // Builds the next request to send: startup_phase_ conversations first (see StartupPhase), then
  // alternating between the essential list (things the master must keep refreshing: the mandatory ids
  // plus any active setpoint) and the informational list (optional reads, round-robined one at a time)
  // so a long informational list can never starve the essentials.
  Frame build_next_request_();
  // The startup_phase_-specific half of build_next_request_().
  Frame build_startup_request_();
  // Moves startup_phase_ to the next phase, skipping any BRAND* phase with no text_sensor configured.
  void advance_startup_phase_();
  bool startup_phase_actionable_(StartupPhase phase) const;
  // Interprets a received frame according to which request it answers; logs and discards it if the
  // boiler replied with a message type that isn't legal for that data-id.
  void handle_response_(const Frame &frame);
  // Shared by the BRAND/BRAND_VERSION/BRAND_SERIAL_NUMBER cases in handle_response_().
  void handle_brand_response_(const Frame &frame, BrandRead &brand, const char *log_name);
  // On a failed conversation, every read-only entity that conversation would have updated must show
  // unknown rather than keep stale data.
  void invalidate_response_(RequestKind kind);
  // Looks up a single-value, non-bit-decomposed read-only sensor's data-id/sensor pointer/log name --
  // the fallback every class after Class 1 dispatches "plain" reads through. Returns nullptr for kinds
  // with bespoke handling (bit-decomposed, write-only, dual-mode, ...).
  const SimpleSensorInfo *find_simple_sensor_(RequestKind kind) const;
  // Defined (sized) in hub.cpp: its pointer-to-member entries need this class complete, and an
  // out-of-class member definition has the same access to protected members as a member function does.
  static const SimpleSensorInfo SIMPLE_SENSORS[];

  InternalGPIOPin *in_pin_{nullptr};
  InternalGPIOPin *out_pin_{nullptr};

  std::unique_ptr<OpenThermDataLink> datalink_;

  uint32_t last_conversation_end_ms_{0};
  RequestKind pending_request_kind_{RequestKind::BOILER_CONFIG};
  StartupPhase startup_phase_{StartupPhase::BOILER_CONFIG};

  // Populated once at setup() from the configured entities; sized small (Class 1 alone has at most 5
  // essential and 6 informational kinds) so std::vector's one-time setup-time growth never touches the
  // heap again afterward.
  std::vector<RequestKind> essential_requests_;
  std::vector<RequestKind> informational_requests_;
  size_t essential_index_{0};
  size_t informational_index_{0};
  bool next_is_informational_{false};

  // Raw values from the §5.2 mandatory conversations -- exposed as real entities once Class 2
  // (Commit 5) lands.
  uint8_t boiler_status_{0};
  uint8_t boiler_config_flags_{0};
  uint8_t boiler_member_id_code_{0};

  // §5.3.1 Class 1 entities.
  FlagWriteBits master_status_write_;
  FlagWriteBits ventilation_status_write_;
  FlagReadBits boiler_status_read_;
  FlagReadBits ventilation_status_read_;
  FlagReadBits fault_flags_read_;
  FlagReadBits ventilation_fault_flags_read_;

  number::Number *control_setpoint_number_{nullptr};
  number::Number *control_setpoint_2_number_{nullptr};
  number::Number *control_setpoint_ventilation_number_{nullptr};

  sensor::Sensor *oem_fault_code_sensor_{nullptr};
  sensor::Sensor *oem_fault_code_ventilation_sensor_{nullptr};
  sensor::Sensor *oem_fault_code_solar_storage_sensor_{nullptr};
  sensor::Sensor *oem_diagnostic_code_sensor_{nullptr};
  sensor::Sensor *oem_diagnostic_code_ventilation_sensor_{nullptr};
  sensor::Sensor *master_solar_storage_status_solar_mode_sensor_{nullptr};
  sensor::Sensor *solar_storage_mode_and_status_solar_mode_sensor_{nullptr};
  sensor::Sensor *solar_storage_mode_and_status_solar_status_sensor_{nullptr};
  binary_sensor::BinarySensor *solar_storage_fault_indication_binary_sensor_{nullptr};

  // §5.3.2 Class 2 entities.
  uint8_t controller_member_id_code_{0};
  float controller_opentherm_version_{4.2f};
  uint8_t controller_product_type_{0};
  uint8_t controller_product_version_{0};

  FlagReadBits boiler_configuration_read_;
  FlagReadBits ventilation_configuration_read_;

  sensor::Sensor *boiler_member_id_code_sensor_{nullptr};
  sensor::Sensor *member_id_code_ventilation_sensor_{nullptr};
  sensor::Sensor *solar_storage_member_id_sensor_{nullptr};
  sensor::Sensor *opentherm_version_boiler_sensor_{nullptr};
  sensor::Sensor *boiler_product_type_sensor_{nullptr};
  sensor::Sensor *boiler_product_version_sensor_{nullptr};
  sensor::Sensor *opentherm_version_ventilation_sensor_{nullptr};
  sensor::Sensor *ventilation_product_type_sensor_{nullptr};
  sensor::Sensor *ventilation_product_version_sensor_{nullptr};
  sensor::Sensor *solar_storage_product_type_sensor_{nullptr};
  sensor::Sensor *solar_storage_product_version_sensor_{nullptr};
  binary_sensor::BinarySensor *solar_storage_configuration_system_type_binary_sensor_{nullptr};

  BrandRead brand_;
  BrandRead brand_version_;
  BrandRead brand_serial_number_;

  // §5.3.3 Class 3 entities.
  bool remote_request_pending_{false};
  uint8_t remote_request_code_{0};
  sensor::Sensor *remote_request_last_response_code_sensor_{nullptr};

  // §5.3.4 Class 4 entities.
  time::RealTimeClock *time_id_{nullptr};

  number::Number *room_setpoint_number_{nullptr};
  number::Number *room_setpoint_ch2_number_{nullptr};
  number::Number *room_temperature_number_{nullptr};
  number::Number *trch2_number_{nullptr};

  number::Number *outside_temperature_number_{nullptr};
  sensor::Sensor *outside_temperature_sensor_{nullptr};
  number::Number *relative_humidity_number_{nullptr};
  sensor::Sensor *relative_humidity_sensor_{nullptr};
  number::Number *relative_humidity_exhaust_air_number_{nullptr};
  sensor::Sensor *relative_humidity_exhaust_air_sensor_{nullptr};
  number::Number *co2_level_number_{nullptr};
  sensor::Sensor *co2_level_sensor_{nullptr};

  sensor::Sensor *boiler_fan_speed_setpoint_sensor_{nullptr};
  sensor::Sensor *boiler_fan_speed_sensor_{nullptr};

  sensor::Sensor *relative_modulation_level_sensor_{nullptr};
  sensor::Sensor *ch_water_pressure_sensor_{nullptr};
  sensor::Sensor *dhw_flow_rate_sensor_{nullptr};
  sensor::Sensor *boiler_water_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_temperature_sensor_{nullptr};
  sensor::Sensor *return_water_temperature_sensor_{nullptr};
  sensor::Sensor *solar_storage_temperature_sensor_{nullptr};
  sensor::Sensor *solar_collector_temperature_sensor_{nullptr};
  sensor::Sensor *flow_temperature_ch2_sensor_{nullptr};
  sensor::Sensor *dhw2_temperature_sensor_{nullptr};
  sensor::Sensor *exhaust_temperature_sensor_{nullptr};
  sensor::Sensor *boiler_heat_exchanger_temperature_sensor_{nullptr};
  sensor::Sensor *flame_current_sensor_{nullptr};
  sensor::Sensor *relative_ventilation_sensor_{nullptr};
  sensor::Sensor *supply_inlet_temperature_sensor_{nullptr};
  sensor::Sensor *supply_outlet_temperature_sensor_{nullptr};
  sensor::Sensor *exhaust_inlet_temperature_sensor_{nullptr};
  sensor::Sensor *exhaust_outlet_temperature_sensor_{nullptr};
  sensor::Sensor *actual_exhaust_fan_speed_sensor_{nullptr};
  sensor::Sensor *actual_inlet_fan_speed_sensor_{nullptr};
  sensor::Sensor *cooling_operation_hours_sensor_{nullptr};
  sensor::Sensor *power_cycles_sensor_{nullptr};
  sensor::Sensor *electricity_producer_starts_sensor_{nullptr};
  sensor::Sensor *electricity_producer_hours_sensor_{nullptr};
  sensor::Sensor *electricity_production_sensor_{nullptr};
  sensor::Sensor *cumulative_electricity_production_sensor_{nullptr};
  sensor::Sensor *number_of_unsuccessful_burner_starts_sensor_{nullptr};
  sensor::Sensor *number_of_times_flame_signal_too_low_sensor_{nullptr};
  sensor::Sensor *successful_burner_starts_sensor_{nullptr};
  sensor::Sensor *ch_pump_starts_sensor_{nullptr};
  sensor::Sensor *dhw_pump_valve_starts_sensor_{nullptr};
  sensor::Sensor *dhw_burner_starts_sensor_{nullptr};
  sensor::Sensor *burner_operation_hours_sensor_{nullptr};
  sensor::Sensor *ch_pump_operation_hours_sensor_{nullptr};
  sensor::Sensor *dhw_pump_valve_operation_hours_sensor_{nullptr};
  sensor::Sensor *dhw_burner_operation_hours_sensor_{nullptr};

  // §5.3.5 Class 5 entities.
  FlagReadBits remote_parameter_transfer_enable_flags_read_;
  FlagReadBits remote_parameter_read_write_flags_read_;
  FlagReadBits remote_parameter_transfer_enable_flags_ventilation_read_;
  FlagReadBits remote_parameter_read_write_flags_ventilation_read_;

  sensor::Sensor *dhwsetp_upper_bound_sensor_{nullptr};
  sensor::Sensor *dhwsetp_lower_bound_sensor_{nullptr};
  sensor::Sensor *max_chsetp_upper_bound_sensor_{nullptr};
  sensor::Sensor *max_chsetp_lower_bound_sensor_{nullptr};

  number::Number *dhw_setpoint_number_{nullptr};
  sensor::Sensor *dhw_setpoint_sensor_{nullptr};
  number::Number *max_ch_water_setpoint_number_{nullptr};
  sensor::Sensor *max_ch_water_setpoint_sensor_{nullptr};
  number::Number *nominal_ventilation_value_number_{nullptr};
  sensor::Sensor *nominal_ventilation_value_sensor_{nullptr};

  // §5.3.6 Class 6 entities.
  sensor::Sensor *number_of_tsps_sensor_{nullptr};
  sensor::Sensor *number_of_tsps_ventilation_sensor_{nullptr};
  sensor::Sensor *number_of_tsps_solar_storage_sensor_{nullptr};

  std::vector<TspSlot> tsp_slots_;
  size_t tsp_read_index_{0};
  bool tsp_write_pending_{false};
  size_t tsp_write_slot_index_{0};
  uint8_t tsp_write_value_{0};
  // Set immediately before build_next_request_() returns a TSP frame; tells handle_response_()/
  // invalidate_response_() which slot and direction that conversation was for.
  size_t pending_tsp_slot_index_{0};
  bool pending_tsp_is_write_{false};

  // §5.3.7 Class 7 entities.
  sensor::Sensor *fault_history_buffer_size_sensor_{nullptr};
  sensor::Sensor *fault_history_buffer_size_ventilation_sensor_{nullptr};
  sensor::Sensor *fault_history_buffer_size_solar_storage_sensor_{nullptr};

  std::vector<FhbSlot> fhb_slots_;
  size_t fhb_read_index_{0};
  // Set immediately before build_next_request_() returns an FHB frame; tells handle_response_()/
  // invalidate_response_() which slot that conversation was for.
  size_t pending_fhb_slot_index_{0};
};

}  // namespace esphome::opentherm42
