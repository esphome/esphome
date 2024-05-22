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
  Packet(RawPacket &&pkt)
      : pkt_(std::move(pkt)){};  // TODO: Confirm this needs std::move if call to constructor ALSO has move
  Packet();                      // For optional<> construction

  // Returns a (more) human readable string of the packet
  virtual std::string to_string() const;

  // Is a response packet expected when this packet is sent.  Defaults to true since
  // most requests receive a response.
  bool isResponseExpected() const { return responseExpected; };
  void setResponseExpected(bool expectResponse) { responseExpected = expectResponse; };

  // Passthrough methods to RawPacket
  RawPacket &rawPacket() { return pkt_; };
  uint8_t getPacketType() const { return pkt_.getPacketType(); }
  bool isChecksumValid() const { return pkt_.isChecksumValid(); };

  // Returns flags (ONLY APPLICABLE FOR SOME COMMANDS)
  uint8_t getFlags() const { return pkt_.getPayloadByte(PLINDEX_FLAGS); }
  // Sets flags (ONLY APPLICABLE FOR SOME COMMANDS)
  void setFlags(const uint8_t flagValue);
  // Adds a flag (ONLY APPLICABLE FOR SOME COMMANDS)
  void addFlag(const uint8_t flagToAdd);
  // Adds a flag2 (ONLY APPLICABLE FOR SOME COMMANDS)
  void addFlag2(const uint8_t flag2ToAdd);

  SourceBridge getSourceBridge() const { return pkt_.getSourceBridge(); }
  ControllerAssociation getControllerAssociation() const { return pkt_.getControllerAssociation(); }

 protected:
  static const int PLINDEX_FLAGS = 1;
  static const int PLINDEX_FLAGS2 = 2;

  RawPacket pkt_;

 private:
  bool responseExpected = true;
};

////
// Connect
////
class ConnectRequestPacket : public Packet {
 public:
  using Packet::Packet;
  static ConnectRequestPacket &instance() {
    static ConnectRequestPacket INSTANCE;
    return INSTANCE;
  }

  std::string to_string() const override;

 private:
  ConnectRequestPacket() : Packet(RawPacket(PacketType::connect_request, 2)) {
    pkt_.setPayloadByte(0, 0xca);
    pkt_.setPayloadByte(1, 0x01);
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
    static ExtendedConnectRequestPacket INSTANCE;
    return INSTANCE;
  }
  using Packet::Packet;

 private:
  ExtendedConnectRequestPacket() : Packet(RawPacket(PacketType::extended_connect_request, 1)) {
    pkt_.setPayloadByte(0, 0xc9);
  }
};

class ExtendedConnectResponsePacket : public Packet {
  using Packet::Packet;

 public:
  // Byte 7
  bool isHeatDisabled() const { return pkt_.getPayloadByte(7) & 0x02; }
  bool supportsVane() const { return pkt_.getPayloadByte(7) & 0x20; }
  bool supportsVaneSwing() const { return pkt_.getPayloadByte(7) & 0x40; }

  // Byte 8
  bool isDryDisabled() const { return pkt_.getPayloadByte(8) & 0x01; }
  bool isFanDisabled() const { return pkt_.getPayloadByte(8) & 0x02; }
  bool hasExtendedTemperatureRange() const { return pkt_.getPayloadByte(8) & 0x04; }
  bool autoFanSpeedDisabled() const { return pkt_.getPayloadByte(8) & 0x10; }
  bool supportsInstallerSettings() const { return pkt_.getPayloadByte(8) & 0x20; }
  bool supportsTestMode() const { return pkt_.getPayloadByte(8) & 0x40; }
  bool supportsDryTemperature() const { return pkt_.getPayloadByte(8) & 0x80; }

  // Byte 9
  bool hasStatusDisplay() const { return pkt_.getPayloadByte(9) & 0x01; }

  // Bytes 10-15
  float getMinCoolDrySetpoint() const { return MUARTUtils::TempScaleAToDegC(pkt_.getPayloadByte(10)); }
  float getMaxCoolDrySetpoint() const { return MUARTUtils::TempScaleAToDegC(pkt_.getPayloadByte(11)); }
  float getMinHeatingSetpoint() const { return MUARTUtils::TempScaleAToDegC(pkt_.getPayloadByte(12)); }
  float getMaxHeatingSetpoint() const { return MUARTUtils::TempScaleAToDegC(pkt_.getPayloadByte(13)); }
  float getMinAutoSetpoint() const { return MUARTUtils::TempScaleAToDegC(pkt_.getPayloadByte(14)); }
  float getMaxAutoSetpoint() const { return MUARTUtils::TempScaleAToDegC(pkt_.getPayloadByte(15)); }

  // Things that have to exist, but we don't know where yet.
  bool supportsHVane() const { return true; }

  // Fan Speeds TODO: Probably move this to .cpp?
  uint8_t getSupportedFanSpeeds() const;

  // Convert a temperature response into ClimateTraits. This will *not* include library-provided features.
  // This will also not handle things like MHK2 humidity detection.
  climate::ClimateTraits asTraits() const;

  std::string to_string() const override;
};

////
// Get
////
class GetRequestPacket : public Packet {
 public:
  static GetRequestPacket &getSettingsInstance() {
    static GetRequestPacket INSTANCE = GetRequestPacket(GetCommand::settings);
    return INSTANCE;
  }
  static GetRequestPacket &getCurrentTempInstance() {
    static GetRequestPacket INSTANCE = GetRequestPacket(GetCommand::current_temp);
    return INSTANCE;
  }
  static GetRequestPacket &getStatusInstance() {
    static GetRequestPacket INSTANCE = GetRequestPacket(GetCommand::standby);
    return INSTANCE;
  }
  static GetRequestPacket &getStandbyInstance() {
    static GetRequestPacket INSTANCE = GetRequestPacket(GetCommand::status);
    return INSTANCE;
  }
  static GetRequestPacket &getErrorInfoInstance() {
    static GetRequestPacket INSTANCE = GetRequestPacket(GetCommand::error_info);
    return INSTANCE;
  }
  using Packet::Packet;

 private:
  GetRequestPacket(GetCommand get_command) : Packet(RawPacket(PacketType::get_request, 1)) {
    pkt_.setPayloadByte(0, static_cast<uint8_t>(get_command));
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
  uint8_t getPower() const { return pkt_.getPayloadByte(PLINDEX_POWER); }
  uint8_t getMode() const { return pkt_.getPayloadByte(PLINDEX_MODE); }
  uint8_t getFan() const { return pkt_.getPayloadByte(PLINDEX_FAN); }
  uint8_t getVane() const { return pkt_.getPayloadByte(PLINDEX_VANE); }
  bool lockedPower() const { return pkt_.getPayloadByte(PLINDEX_PROHIBITFLAGS) & 0x01; }
  bool lockedMode() const { return pkt_.getPayloadByte(PLINDEX_PROHIBITFLAGS) & 0x02; }
  bool lockedTemp() const { return pkt_.getPayloadByte(PLINDEX_PROHIBITFLAGS) & 0x04; }
  uint8_t getHorizontalVane() const { return pkt_.getPayloadByte(PLINDEX_HVANE) & 0x7F; }
  bool getHorizontalVaneMSB() const { return pkt_.getPayloadByte(PLINDEX_HVANE) & 0x80; }

  float getTargetTemp() const;

  std::string to_string() const override;
};

class CurrentTempGetResponsePacket : public Packet {
  static const int PLINDEX_CURRENTTEMP_LEGACY = 3;
  static const int PLINDEX_CURRENTTEMP = 6;
  using Packet::Packet;

 public:
  float getCurrentTemp() const;
  std::string to_string() const override;
};

class StatusGetResponsePacket : public Packet {
  static const int PLINDEX_COMPRESSOR_FREQUENCY = 3;
  static const int PLINDEX_OPERATING = 4;

  using Packet::Packet;

 public:
  uint8_t getCompressorFrequency() const { return pkt_.getPayloadByte(PLINDEX_COMPRESSOR_FREQUENCY); }
  bool getOperating() const { return pkt_.getPayloadByte(PLINDEX_OPERATING); }
  std::string to_string() const override;
};

class StandbyGetResponsePacket : public Packet {
  static const int PLINDEX_STATUSFLAGS = 3;
  static const int PLINDEX_ACTUALFAN = 4;
  static const int PLINDEX_AUTOMODE = 5;
  using Packet::Packet;

 public:
  bool serviceFilter() const { return pkt_.getPayloadByte(PLINDEX_STATUSFLAGS) & 0x01; }
  bool inDefrost() const { return pkt_.getPayloadByte(PLINDEX_STATUSFLAGS) & 0x02; }
  bool inHotAdjust() const { return pkt_.getPayloadByte(PLINDEX_STATUSFLAGS) & 0x04; }
  bool inStandby() const { return pkt_.getPayloadByte(PLINDEX_STATUSFLAGS) & 0x08; }
  uint8_t getActualFanSpeed() const { return pkt_.getPayloadByte(PLINDEX_ACTUALFAN); }
  uint8_t getAutoMode() const { return pkt_.getPayloadByte(PLINDEX_AUTOMODE); }
  std::string to_string() const override;
};

class ErrorStateGetResponsePacket : public Packet {
  using Packet::Packet;

 public:
  uint16_t getErrorCode() const { return pkt_.getPayloadByte(4) << 8 | pkt_.getPayloadByte(5); }
  uint8_t getRawShortCode() const { return pkt_.getPayloadByte(6); }
  std::string getShortCode() const;

  bool errorPresent() const { return getErrorCode() != 0x8000 || getRawShortCode() != 0x00; }

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

  enum SETTING_FLAG : uint8_t {
    SF_POWER = 0x01,
    SF_MODE = 0x02,
    SF_TARGET_TEMPERATURE = 0x04,
    SF_FAN = 0x08,
    SF_VANE = 0x10
  };

  enum SETTING_FLAG2 : uint8_t {
    SF2_HORIZONTAL_VANE = 0x01,
  };

 public:
  enum MODE_BYTE : uint8_t {
    MODE_BYTE_HEAT = 0x01,
    MODE_BYTE_DRY = 0x02,
    MODE_BYTE_COOL = 0x03,
    MODE_BYTE_FAN = 0x07,
    MODE_BYTE_AUTO = 0x08,
  };

  enum FAN_BYTE : uint8_t {
    FAN_AUTO = 0x00,
    FAN_QUIET = 0x01,
    FAN_1 = 0x02,
    FAN_2 = 0x03,
    FAN_3 = 0x05,
    FAN_4 = 0x06,
  };

  enum VANE_BYTE : uint8_t {
    VANE_AUTO = 0x00,
    VANE_1 = 0x01,
    VANE_2 = 0x02,
    VANE_3 = 0x03,
    VANE_4 = 0x04,
    VANE_5 = 0x05,
    VANE_SWING = 0x07,
  };

  enum HORIZONTAL_VANE_BYTE : uint8_t {
    HV_AUTO = 0x00,
    HV_LEFT_FULL = 0x01,
    HV_LEFT = 0x02,
    HV_CENTER = 0x03,
    HV_RIGHT = 0x04,
    HV_RIGHT_FULL = 0x05,
    HV_SPLIT = 0x08,
    HV_SWING = 0x0c,
  };

  SettingsSetRequestPacket() : Packet(RawPacket(PacketType::set_request, 16)) {
    pkt_.setPayloadByte(0, static_cast<uint8_t>(SetCommand::settings));
  }
  using Packet::Packet;

  SettingsSetRequestPacket &setPower(bool isOn);
  SettingsSetRequestPacket &setMode(MODE_BYTE mode);
  SettingsSetRequestPacket &setTargetTemperature(float temperatureDegressC);
  SettingsSetRequestPacket &setFan(FAN_BYTE fan);
  SettingsSetRequestPacket &setVane(VANE_BYTE vane);
  SettingsSetRequestPacket &setHorizontalVane(HORIZONTAL_VANE_BYTE horizontal_vane);

 private:
  void addSettingsFlag(SETTING_FLAG flagToAdd);
  void addSettingsFlag2(SETTING_FLAG2 flag2ToAdd);
};

class RemoteTemperatureSetRequestPacket : public Packet {
  static const uint8_t PLINDEX_LEGACY_REMOTE_TEMPERATURE = 2;
  static const uint8_t PLINDEX_REMOTE_TEMPERATURE = 3;

 public:
  RemoteTemperatureSetRequestPacket() : Packet(RawPacket(PacketType::set_request, 4)) {
    pkt_.setPayloadByte(0, static_cast<uint8_t>(SetCommand::remote_temperature));
  }
  using Packet::Packet;

  float getRemoteTemperature() const;

  RemoteTemperatureSetRequestPacket &setRemoteTemperature(float temperatureDegressC);
  RemoteTemperatureSetRequestPacket &useInternalTemperature();

  std::string to_string() const override;
};

class SetResponsePacket : public Packet {
  using Packet::Packet;

 public:
  SetResponsePacket() : Packet(RawPacket(PacketType::set_response, 16)) {}

  uint8_t getResultCode() const { return pkt_.getPayloadByte(0); }
  bool isSuccessful() const { return getResultCode() == 0; }
};

// Sent by MHK2 but with no response; defined to allow setResponseExpected(false)
class ThermostatHelloRequestPacket : public Packet {
  using Packet::Packet;

 public:
  ThermostatHelloRequestPacket() : Packet(RawPacket(PacketType::set_request, 4)) {
    pkt_.setPayloadByte(0, static_cast<uint8_t>(SetCommand::thermostat_hello));
  }

  std::string getThermostatModel() const;
  std::string getThermostatSerial() const;
  std::string getThermostatVersionString() const;

  std::string to_string() const override;
};

// Sent by MHK2 but with no response; defined to allow setResponseExpected(false)
class A9GetRequestPacket : public Packet {
  using Packet::Packet;

 public:
  A9GetRequestPacket() : Packet(RawPacket(PacketType::get_request, 10)) {
    pkt_.setPayloadByte(0, static_cast<uint8_t>(GetCommand::a_9));
  }
};

class PacketProcessor {
 public:
  virtual void processPacket(const Packet &packet){};
  virtual void processPacket(const ConnectRequestPacket &packet){};
  virtual void processPacket(const ConnectResponsePacket &packet){};
  virtual void processPacket(const ExtendedConnectRequestPacket &packet){};
  virtual void processPacket(const ExtendedConnectResponsePacket &packet){};
  virtual void processPacket(const GetRequestPacket &packet){};
  virtual void processPacket(const SettingsGetResponsePacket &packet){};
  virtual void processPacket(const CurrentTempGetResponsePacket &packet){};
  virtual void processPacket(const StatusGetResponsePacket &packet){};
  virtual void processPacket(const StandbyGetResponsePacket &packet){};
  virtual void processPacket(const ErrorStateGetResponsePacket &packet){};
  virtual void processPacket(const RemoteTemperatureSetRequestPacket &packet){};
  virtual void processPacket(const SetResponsePacket &packet){};
};

}  // namespace mitsubishi_uart
}  // namespace esphome
