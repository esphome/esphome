#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "muart_rawpacket.h"
#include "muart_utils.h"
#include <sstream>

namespace esphome {
namespace mitsubishi_uart {
static const char *PACKETS_TAG = "mitsubishi_uart.packets";

#define CONSOLE_COLOR_NONE "\033[0m"
#define CONSOLE_COLOR_GREEN "\033[0;32m"
#define CONSOLE_COLOR_PURPLE "\033[0;35m"
#define CONSOLE_COLOR_CYAN "\033[0;36m"
#define CONSOLE_COLOR_CYAN_BOLD "\033[1;36m"
#define CONSOLE_COLOR_WHITE "\033[0;37m"

class PacketProcessor;

// Generic Base Packet wrapper over RawPacket
class Packet {
 public:
  Packet(RawPacket &&pkt) : pkt_(pkt){};  // TODO: Confirm this needs std::move if call to constructor ALSO has move
  Packet();                               // For optional<> construction

  // Returns a (more) human readable string of the packet
  virtual std::string to_string() const;

  // Is a response packet expected when this packet is sent.  Defaults to true since
  // most requests receive a response.
  bool is_response_expected() const { return response_expected_; };
  void set_response_expected(bool expect_response) { response_expected_ = expect_response; };

  // Passthrough methods to RawPacket
  RawPacket &raw_packet() { return pkt_; };
  uint8_t get_packet_type() const { return pkt_.get_packet_type(); }
  bool is_checksum_valid() const { return pkt_.is_checksum_valid(); };

  // Returns flags (ONLY APPLICABLE FOR SOME COMMANDS)
  uint8_t get_flags() const { return pkt_.get_payload_byte(PLINDEX_FLAGS); }
  // Sets flags (ONLY APPLICABLE FOR SOME COMMANDS)
  void set_flags(uint8_t flag_value);
  // Adds a flag (ONLY APPLICABLE FOR SOME COMMANDS)
  void add_flag(uint8_t flag_to_add);
  // Adds a flag2 (ONLY APPLICABLE FOR SOME COMMANDS)
  void add_flag2(uint8_t flag2_to_add);

  SourceBridge get_source_bridge() const { return pkt_.get_source_bridge(); }
  ControllerAssociation get_controller_association() const { return pkt_.get_controller_association(); }

 protected:
  static const int PLINDEX_FLAGS = 1;
  static const int PLINDEX_FLAGS2 = 2;

  RawPacket pkt_;

 private:
  bool response_expected_ = true;
};

////
// Connect
////
class ConnectRequestPacket : public Packet {
 public:
  using Packet::Packet;
  static ConnectRequestPacket &instance() {
    static ConnectRequestPacket instance;
    return instance;
  }

  std::string to_string() const override;

 private:
  ConnectRequestPacket() : Packet(RawPacket(PacketType::CONNECT_REQUEST, 2)) {
    pkt_.set_payload_byte(0, 0xca);
    pkt_.set_payload_byte(1, 0x01);
  }
};

class ConnectResponsePacket : public Packet {
 public:
  using Packet::Packet;
  std::string to_string() const override;
};

////
// Extended Connect
////
class ExtendedConnectRequestPacket : public Packet {
 public:
  static ExtendedConnectRequestPacket &instance() {
    static ExtendedConnectRequestPacket instance;
    return instance;
  }
  using Packet::Packet;

 private:
  ExtendedConnectRequestPacket() : Packet(RawPacket(PacketType::EXTENDED_CONNECT_REQUEST, 1)) {
    pkt_.set_payload_byte(0, 0xc9);
  }
};

class ExtendedConnectResponsePacket : public Packet {
  using Packet::Packet;

 public:
  // Byte 7
  bool is_heat_disabled() const { return pkt_.get_payload_byte(7) & 0x02; }
  bool supports_vane() const { return pkt_.get_payload_byte(7) & 0x20; }
  bool supports_vane_swing() const { return pkt_.get_payload_byte(7) & 0x40; }

  // Byte 8
  bool is_dry_disabled() const { return pkt_.get_payload_byte(8) & 0x01; }
  bool is_fan_disabled() const { return pkt_.get_payload_byte(8) & 0x02; }
  bool has_extended_temperature_range() const { return pkt_.get_payload_byte(8) & 0x04; }
  bool auto_fan_speed_disabled() const { return pkt_.get_payload_byte(8) & 0x10; }
  bool supports_installer_settings() const { return pkt_.get_payload_byte(8) & 0x20; }
  bool supports_test_mode() const { return pkt_.get_payload_byte(8) & 0x40; }
  bool supports_dry_temperature() const { return pkt_.get_payload_byte(8) & 0x80; }

  // Byte 9
  bool has_status_display() const { return pkt_.get_payload_byte(9) & 0x01; }

  // Bytes 10-15
  float get_min_cool_dry_setpoint() const { return MUARTUtils::temp_scale_a_to_deg_c(pkt_.get_payload_byte(10)); }
  float get_max_cool_dry_setpoint() const { return MUARTUtils::temp_scale_a_to_deg_c(pkt_.get_payload_byte(11)); }
  float get_min_heating_setpoint() const { return MUARTUtils::temp_scale_a_to_deg_c(pkt_.get_payload_byte(12)); }
  float get_max_heating_setpoint() const { return MUARTUtils::temp_scale_a_to_deg_c(pkt_.get_payload_byte(13)); }
  float get_min_auto_setpoint() const { return MUARTUtils::temp_scale_a_to_deg_c(pkt_.get_payload_byte(14)); }
  float get_max_auto_setpoint() const { return MUARTUtils::temp_scale_a_to_deg_c(pkt_.get_payload_byte(15)); }

  // Things that have to exist, but we don't know where yet.
  bool supports_h_vane() const { return true; }

  // Fan Speeds TODO: Probably move this to .cpp?
  uint8_t get_supported_fan_speeds() const;

  // Convert a temperature response into ClimateTraits. This will *not* include library-provided features.
  // This will also not handle things like MHK2 humidity detection.
  climate::ClimateTraits as_traits() const;

  std::string to_string() const override;
};

////
// Get
////
class GetRequestPacket : public Packet {
 public:
  static GetRequestPacket &get_settings_instance() {
    static GetRequestPacket instance = GetRequestPacket(GetCommand::SETTINGS);
    return instance;
  }
  static GetRequestPacket &get_current_temp_instance() {
    static GetRequestPacket instance = GetRequestPacket(GetCommand::CURRENT_TEMP);
    return instance;
  }
  static GetRequestPacket &get_status_instance() {
    static GetRequestPacket instance = GetRequestPacket(GetCommand::STANDBY);
    return instance;
  }
  static GetRequestPacket &get_standby_instance() {
    static GetRequestPacket instance = GetRequestPacket(GetCommand::STATUS);
    return instance;
  }
  static GetRequestPacket &get_error_info_instance() {
    static GetRequestPacket instance = GetRequestPacket(GetCommand::ERROR_INFO);
    return instance;
  }
  using Packet::Packet;

 private:
  GetRequestPacket(GetCommand get_command) : Packet(RawPacket(PacketType::GET_REQUEST, 1)) {
    pkt_.set_payload_byte(0, static_cast<uint8_t>(get_command));
  }
};

class SettingsGetResponsePacket : public Packet {
  static const int PLINDEX_POWER = 3;
  static const int PLINDEX_MODE = 4;
  static const int PLINDEX_TARGETTEMP_LEGACY = 5;
  static const int PLINDEX_FAN = 6;
  static const int PLINDEX_VANE = 7;
  static const int PLINDEX_PROHIBITFLAGS = 8;
  static const int PLINDEX_HVANE = 10;
  static const int PLINDEX_TARGETTEMP = 11;
  using Packet::Packet;

 public:
  uint8_t get_power() const { return pkt_.get_payload_byte(PLINDEX_POWER); }
  uint8_t get_mode() const { return pkt_.get_payload_byte(PLINDEX_MODE); }
  uint8_t get_fan() const { return pkt_.get_payload_byte(PLINDEX_FAN); }
  uint8_t get_vane() const { return pkt_.get_payload_byte(PLINDEX_VANE); }
  bool locked_power() const { return pkt_.get_payload_byte(PLINDEX_PROHIBITFLAGS) & 0x01; }
  bool locked_mode() const { return pkt_.get_payload_byte(PLINDEX_PROHIBITFLAGS) & 0x02; }
  bool locked_temp() const { return pkt_.get_payload_byte(PLINDEX_PROHIBITFLAGS) & 0x04; }
  uint8_t get_horizontal_vane() const { return pkt_.get_payload_byte(PLINDEX_HVANE) & 0x7F; }
  bool get_horizontal_vane_msb() const { return pkt_.get_payload_byte(PLINDEX_HVANE) & 0x80; }

  float get_target_temp() const;

  bool isISeeEnabled() const;

  std::string to_string() const override;
};

class CurrentTempGetResponsePacket : public Packet {
  static const int PLINDEX_CURRENTTEMP_LEGACY = 3;
  static const int PLINDEX_CURRENTTEMP = 6;
  using Packet::Packet;

 public:
  float get_current_temp() const;
  std::string to_string() const override;
};

class StatusGetResponsePacket : public Packet {
  static const int PLINDEX_COMPRESSOR_FREQUENCY = 3;
  static const int PLINDEX_OPERATING = 4;

  using Packet::Packet;

 public:
  uint8_t get_compressor_frequency() const { return pkt_.get_payload_byte(PLINDEX_COMPRESSOR_FREQUENCY); }
  bool get_operating() const { return pkt_.get_payload_byte(PLINDEX_OPERATING); }
  std::string to_string() const override;
};

class StandbyGetResponsePacket : public Packet {
  static const int PLINDEX_STATUSFLAGS = 3;
  static const int PLINDEX_ACTUALFAN = 4;
  static const int PLINDEX_AUTOMODE = 5;
  using Packet::Packet;

 public:
  bool service_filter() const { return pkt_.get_payload_byte(PLINDEX_STATUSFLAGS) & 0x01; }
  bool in_defrost() const { return pkt_.get_payload_byte(PLINDEX_STATUSFLAGS) & 0x02; }
  bool in_hot_adjust() const { return pkt_.get_payload_byte(PLINDEX_STATUSFLAGS) & 0x04; }
  bool in_standby() const { return pkt_.get_payload_byte(PLINDEX_STATUSFLAGS) & 0x08; }
  uint8_t get_actual_fan_speed() const { return pkt_.get_payload_byte(PLINDEX_ACTUALFAN); }
  uint8_t get_auto_mode() const { return pkt_.get_payload_byte(PLINDEX_AUTOMODE); }
  std::string to_string() const override;
};

class ErrorStateGetResponsePacket : public Packet {
  using Packet::Packet;

 public:
  uint16_t get_error_code() const { return pkt_.get_payload_byte(4) << 8 | pkt_.get_payload_byte(5); }
  uint8_t get_raw_short_code() const { return pkt_.get_payload_byte(6); }
  std::string get_short_code() const;

  bool error_present() const { return get_error_code() != 0x8000 || get_raw_short_code() != 0x00; }

  std::string to_string() const override;
};

////
// Set
////

class SettingsSetRequestPacket : public Packet {
  static const int PLINDEX_POWER = 3;
  static const int PLINDEX_MODE = 4;
  static const int PLINDEX_TARGET_TEMPERATURE_CODE = 5;
  static const int PLINDEX_FAN = 6;
  static const int PLINDEX_VANE = 7;
  static const int PLINDEX_HORIZONTAL_VANE = 13;
  static const int PLINDEX_TARGET_TEMPERATURE = 14;

  enum SettingFlag : uint8_t {
    SF_POWER = 0x01,
    SF_MODE = 0x02,
    SF_TARGET_TEMPERATURE = 0x04,
    SF_FAN = 0x08,
    SF_VANE = 0x10
  };

  enum SettingFlaG2 : uint8_t {
    SF2_HORIZONTAL_VANE = 0x01,
  };

 public:
  enum ModeByte : uint8_t {
    MODE_BYTE_HEAT = 0x01,
    MODE_BYTE_DRY = 0x02,
    MODE_BYTE_COOL = 0x03,
    MODE_BYTE_FAN = 0x07,
    MODE_BYTE_AUTO = 0x08,
  };

  enum FanByte : uint8_t {
    FAN_AUTO = 0x00,
    FAN_QUIET = 0x01,
    FAN_1 = 0x02,
    FAN_2 = 0x03,
    FAN_3 = 0x05,
    FAN_4 = 0x06,
  };

  enum VaneByte : uint8_t {
    VANE_AUTO = 0x00,
    VANE_1 = 0x01,
    VANE_2 = 0x02,
    VANE_3 = 0x03,
    VANE_4 = 0x04,
    VANE_5 = 0x05,
    VANE_SWING = 0x07,
  };

  enum HorizontalVaneByte : uint8_t {
    HV_AUTO = 0x00,
    HV_LEFT_FULL = 0x01,
    HV_LEFT = 0x02,
    HV_CENTER = 0x03,
    HV_RIGHT = 0x04,
    HV_RIGHT_FULL = 0x05,
    HV_SPLIT = 0x08,
    HV_SWING = 0x0c,
  };

  SettingsSetRequestPacket() : Packet(RawPacket(PacketType::SET_REQUEST, 16)) {
    pkt_.set_payload_byte(0, static_cast<uint8_t>(SetCommand::SETTINGS));
  }
  using Packet::Packet;

  SettingsSetRequestPacket &set_power(bool is_on);
  SettingsSetRequestPacket &set_mode(ModeByte mode);
  SettingsSetRequestPacket &set_target_temperature(float temperature_degress_c);
  SettingsSetRequestPacket &set_fan(FanByte fan);
  SettingsSetRequestPacket &set_vane(VaneByte vane);
  SettingsSetRequestPacket &set_horizontal_vane(HorizontalVaneByte horizontal_vane);

 private:
  void add_settings_flag_(SettingFlag flag_to_add);
  void add_settings_flag2_(SettingFlaG2 flag2_to_add);
};

class RemoteTemperatureSetRequestPacket : public Packet {
  static const uint8_t PLINDEX_LEGACY_REMOTE_TEMPERATURE = 2;
  static const uint8_t PLINDEX_REMOTE_TEMPERATURE = 3;

 public:
  RemoteTemperatureSetRequestPacket() : Packet(RawPacket(PacketType::SET_REQUEST, 4)) {
    pkt_.set_payload_byte(0, static_cast<uint8_t>(SetCommand::REMOTE_TEMPERATURE));
  }
  using Packet::Packet;

  float get_remote_temperature() const;

  RemoteTemperatureSetRequestPacket &set_remote_temperature(float temperature_degress_c);
  RemoteTemperatureSetRequestPacket &use_internal_temperature();

  std::string to_string() const override;
};

class SetResponsePacket : public Packet {
  using Packet::Packet;

 public:
  SetResponsePacket() : Packet(RawPacket(PacketType::SET_RESPONSE, 16)) {}

  uint8_t get_result_code() const { return pkt_.get_payload_byte(0); }
  bool is_successful() const { return get_result_code() == 0; }
};

// Sent by MHK2 but with no response; defined to allow setResponseExpected(false)
class ThermostatHelloRequestPacket : public Packet {
  using Packet::Packet;

 public:
  ThermostatHelloRequestPacket() : Packet(RawPacket(PacketType::SET_REQUEST, 4)) {
    pkt_.set_payload_byte(0, static_cast<uint8_t>(SetCommand::THERMOSTAT_HELLO));
  }

  std::string get_thermostat_model() const;
  std::string get_thermostat_serial() const;
  std::string get_thermostat_version_string() const;

  std::string to_string() const override;
};

// Sent by MHK2 but with no response; defined to allow setResponseExpected(false)
class A9GetRequestPacket : public Packet {
  using Packet::Packet;

 public:
  A9GetRequestPacket() : Packet(RawPacket(PacketType::GET_REQUEST, 10)) {
    pkt_.set_payload_byte(0, static_cast<uint8_t>(GetCommand::A_9));
  }
};

class PacketProcessor {
 public:
  virtual void process_packet(const Packet &packet){};
  virtual void process_packet(const ConnectRequestPacket &packet){};
  virtual void process_packet(const ConnectResponsePacket &packet){};
  virtual void process_packet(const ExtendedConnectRequestPacket &packet){};
  virtual void process_packet(const ExtendedConnectResponsePacket &packet){};
  virtual void process_packet(const GetRequestPacket &packet){};
  virtual void process_packet(const SettingsGetResponsePacket &packet){};
  virtual void process_packet(const CurrentTempGetResponsePacket &packet){};
  virtual void process_packet(const StatusGetResponsePacket &packet){};
  virtual void process_packet(const StandbyGetResponsePacket &packet){};
  virtual void process_packet(const ErrorStateGetResponsePacket &packet){};
  virtual void process_packet(const RemoteTemperatureSetRequestPacket &packet){};
  virtual void process_packet(const SetResponsePacket &packet){};
};

}  // namespace mitsubishi_uart
}  // namespace esphome
