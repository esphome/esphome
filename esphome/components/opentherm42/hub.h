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
};

}  // namespace esphome::opentherm42
