#include "muart_packet.h"
#include "muart_utils.h"
#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_uart {

// Packet to_strings()

std::string ConnectRequestPacket::to_string() const { return ("Connect Request: " + Packet::to_string()); }
std::string ConnectResponsePacket::to_string() const { return ("Connect Response: " + Packet::to_string()); }
std::string ExtendedConnectResponsePacket::to_string() const {
  return ("Extended Connect Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n HeatDisabled:" + (isHeatDisabled() ? "Yes" : "No") + " SupportsVane:" + (supportsVane() ? "Yes" : "No") +
          " SupportsVaneSwing:" + (supportsVaneSwing() ? "Yes" : "No")

          + " DryDisabled:" + (isDryDisabled() ? "Yes" : "No") + " FanDisabled:" + (isFanDisabled() ? "Yes" : "No") +
          " ExtTempRange:" + (hasExtendedTemperatureRange() ? "Yes" : "No") +
          " AutoFanDisabled:" + (autoFanSpeedDisabled() ? "Yes" : "No") +
          " InstallerSettings:" + (supportsInstallerSettings() ? "Yes" : "No") +
          " TestMode:" + (supportsTestMode() ? "Yes" : "No") + " DryTemp:" + (supportsDryTemperature() ? "Yes" : "No")

          + " StatusDisplay:" + (hasStatusDisplay() ? "Yes" : "No")

          + "\n CoolDrySetpoint:" + std::to_string(getMinCoolDrySetpoint()) + "/" +
          std::to_string(getMaxCoolDrySetpoint()) + " HeatSetpoint:" + std::to_string(getMinHeatingSetpoint()) + "/" +
          std::to_string(getMaxHeatingSetpoint()) + " AutoSetpoint:" + std::to_string(getMinAutoSetpoint()) + "/" +
          std::to_string(getMaxAutoSetpoint()) + " FanSpeeds:" + std::to_string(getSupportedFanSpeeds()));
}
std::string CurrentTempGetResponsePacket::to_string() const {
  return ("Current Temp Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Temp:" + std::to_string(getCurrentTemp()));
}
std::string SettingsGetResponsePacket::to_string() const {
  return ("Settings Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n Fan:" + format_hex(getFan()) +
          " Mode:" + format_hex(getMode()) + " Power:" +
          (getPower() == 3  ? "Test"
           : getPower() > 0 ? "On"
                            : "Off") +
          " TargetTemp:" + std::to_string(getTargetTemp()) + " Vane:" + format_hex(getVane()) +
          " HVane:" + format_hex(getHorizontalVane()) + (getHorizontalVaneMSB() ? " (MSB Set)" : "") +
          "\n PowerLock:" + (lockedPower() ? "Yes" : "No") + " ModeLock:" + (lockedMode() ? "Yes" : "No") +
          " TempLock:" + (lockedTemp() ? "Yes" : "No"));
}
std::string StandbyGetResponsePacket::to_string() const {
  return ("Standby Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n ServiceFilter:" + (serviceFilter() ? "Yes" : "No") + " Defrost:" + (inDefrost() ? "Yes" : "No") +
          " HotAdjust:" + (inHotAdjust() ? "Yes" : "No") + " Standby:" + (inStandby() ? "Yes" : "No") +
          " ActualFan:" + ACTUAL_FAN_SPEED_NAMES[getActualFanSpeed()] + " (" + std::to_string(getActualFanSpeed()) +
          ")" + " AutoMode:" + format_hex(getAutoMode()));
}
std::string StatusGetResponsePacket::to_string() const {
  return ("Status Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n CompressorFrequency: " +
          std::to_string(getCompressorFrequency()) + " Operating: " + (getOperating() ? "Yes" : "No"));
}
std::string ErrorStateGetResponsePacket::to_string() const {
  return ("Error State Response: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Error State: " + (errorPresent() ? "Yes" : "No") + " ErrorCode: " + format_hex(getErrorCode()) +
          " ShortCode: " + getShortCode() + "(" + format_hex(getRawShortCode()) + ")");
}
std::string RemoteTemperatureSetRequestPacket::to_string() const {
  return ("Remote Temp Set Request: " + Packet::to_string() + CONSOLE_COLOR_PURPLE +
          "\n Temp:" + std::to_string(getRemoteTemperature()));
}

std::string ThermostatHelloRequestPacket::to_string() const {
  return ("Thermostat Hello: " + Packet::to_string() + CONSOLE_COLOR_PURPLE + "\n Model: " + getThermostatModel() +
          " Serial: " + getThermostatSerial() + " Version: " + getThermostatVersionString());
}

// TODO: Are there function implementations for packets in the .h file? (Yes)  Should they be here?

// SettingsSetRequestPacket functions

void SettingsSetRequestPacket::addSettingsFlag(const SETTING_FLAG flagToAdd) { addFlag(flagToAdd); }

void SettingsSetRequestPacket::addSettingsFlag2(const SETTING_FLAG2 flag2ToAdd) { addFlag2(flag2ToAdd); }

SettingsSetRequestPacket &SettingsSetRequestPacket::setPower(const bool isOn) {
  pkt_.setPayloadByte(PLINDEX_POWER, isOn ? 0x01 : 0x00);
  addSettingsFlag(SF_POWER);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::setMode(const MODE_BYTE mode) {
  pkt_.setPayloadByte(PLINDEX_MODE, mode);
  addSettingsFlag(SF_MODE);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::setTargetTemperature(const float temperatureDegressC) {
  if (temperatureDegressC < 63.5 && temperatureDegressC > -64.0) {
    pkt_.setPayloadByte(PLINDEX_TARGET_TEMPERATURE, MUARTUtils::DegCToTempScaleA(temperatureDegressC));
    pkt_.setPayloadByte(PLINDEX_TARGET_TEMPERATURE_CODE, MUARTUtils::DegCToLegacyTargetTemp(temperatureDegressC));

    // TODO: while spawning a warning here is fine, we should (a) only actually send that warning if the system can't
    //       support this setpoint, and (b) clamp the setpoint to the known-acceptable values.
    // The utility class will already clamp this for us, so we only need to worry about the warning.
    if (temperatureDegressC < 16 || temperatureDegressC > 31.5) {
      ESP_LOGW(PTAG, "Target temp %f is out of range for the legacy temp scale. This may be a problem on older units.",
               temperatureDegressC);
    }

    addSettingsFlag(SF_TARGET_TEMPERATURE);
  } else {
    ESP_LOGW(PTAG, "Target temp %f is outside valid range - target temperature not set!", temperatureDegressC);
  }

  return *this;
}
SettingsSetRequestPacket &SettingsSetRequestPacket::setFan(const FAN_BYTE fan) {
  pkt_.setPayloadByte(PLINDEX_FAN, fan);
  addSettingsFlag(SF_FAN);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::setVane(const VANE_BYTE vane) {
  pkt_.setPayloadByte(PLINDEX_VANE, vane);
  addSettingsFlag(SF_VANE);
  return *this;
}

SettingsSetRequestPacket &SettingsSetRequestPacket::setHorizontalVane(const HORIZONTAL_VANE_BYTE horizontal_vane) {
  pkt_.setPayloadByte(PLINDEX_HORIZONTAL_VANE, horizontal_vane);
  addSettingsFlag2(SF2_HORIZONTAL_VANE);
  return *this;
}

// SettingsGetResponsePacket functions
float SettingsGetResponsePacket::getTargetTemp() const {
  uint8_t enhancedRawTemp = pkt_.getPayloadByte(PLINDEX_TARGETTEMP);

  if (enhancedRawTemp == 0x00) {
    uint8_t legacyRawTemp = pkt_.getPayloadByte(PLINDEX_TARGETTEMP_LEGACY);
    return MUARTUtils::LegacyTargetTempToDegC(legacyRawTemp);
  }

  return MUARTUtils::TempScaleAToDegC(enhancedRawTemp);
}

bool SettingsGetResponsePacket::isISeeEnabled() const {
  uint8_t mode = pkt_.getPayloadByte(PLINDEX_MODE);

  // so far only modes 0x09 to 0x11 are known to be i-see.
  // Mode 0x08 technically *can* be, but it's not a guarantee by itself.
  return (mode >= 0x09 && mode <= 0x11);
}

// RemoteTemperatureSetRequestPacket functions

float RemoteTemperatureSetRequestPacket::getRemoteTemperature() const {
  uint8_t rawTempA = pkt_.getPayloadByte(PLINDEX_REMOTE_TEMPERATURE);

  if (rawTempA == 0) {
    uint8_t rawTempLegacy = pkt_.getPayloadByte(PLINDEX_LEGACY_REMOTE_TEMPERATURE);
    return MUARTUtils::LegacyRoomTempToDegC(rawTempLegacy);
  }

  return MUARTUtils::TempScaleAToDegC(rawTempA);
}

RemoteTemperatureSetRequestPacket &RemoteTemperatureSetRequestPacket::setRemoteTemperature(float temperatureDegressC) {
  if (temperatureDegressC < 63.5 && temperatureDegressC > -64.0) {
    pkt_.setPayloadByte(PLINDEX_REMOTE_TEMPERATURE, MUARTUtils::DegCToTempScaleA(temperatureDegressC));
    pkt_.setPayloadByte(PLINDEX_LEGACY_REMOTE_TEMPERATURE, MUARTUtils::DegCToLegacyRoomTemp(temperatureDegressC));
    setFlags(0x01);  // Set flags to say we're providing the temperature
  } else {
    ESP_LOGW(PTAG, "Remote temp %f is outside valid range.", temperatureDegressC);
  }
  return *this;
}
RemoteTemperatureSetRequestPacket &RemoteTemperatureSetRequestPacket::useInternalTemperature() {
  setFlags(0x00);  // Set flags to say to use internal temperature
  return *this;
}

// CurrentTempGetResponsePacket functions
float CurrentTempGetResponsePacket::getCurrentTemp() const {
  uint8_t enhancedRawTemp = pkt_.getPayloadByte(PLINDEX_CURRENTTEMP);

  // TODO: Figure out how to handle "out of range" issues here.
  if (enhancedRawTemp == 0) {
    uint8_t legacyRawTemp = pkt_.getPayloadByte(PLINDEX_CURRENTTEMP_LEGACY);
    return MUARTUtils::LegacyRoomTempToDegC(legacyRawTemp);
  }

  return MUARTUtils::TempScaleAToDegC(enhancedRawTemp);
}

// ThermostatHelloRequestPacket functions
std::string ThermostatHelloRequestPacket::getThermostatModel() const {
  return MUARTUtils::DecodeNBitString((pkt_.getBytes() + 1), 3, 6);
}

std::string ThermostatHelloRequestPacket::getThermostatSerial() const {
  return MUARTUtils::DecodeNBitString((pkt_.getBytes() + 4), 8, 6);
}

std::string ThermostatHelloRequestPacket::getThermostatVersionString() const {
  char buf[16];
  sprintf(buf, "%02d.%02d.%02d", pkt_.getPayloadByte(13), pkt_.getPayloadByte(14), pkt_.getPayloadByte(15));

  return buf;
}

// ErrorStateGetResponsePacket functions
std::string ErrorStateGetResponsePacket::getShortCode() const {
  const char *upperAlphabet = "AbEFJLPU";
  const char *lowerAlphabet = "0123456789ABCDEFOHJLPU";
  const uint8_t errorCode = this->getRawShortCode();

  uint8_t lowBits = errorCode & 0x1F;
  if (lowBits > 0x15) {
    char buf[7];
    sprintf(buf, "ERR_%x", errorCode);
    return buf;
  }

  return {upperAlphabet[(errorCode & 0xE0) >> 5], lowerAlphabet[lowBits]};
}

// ExtendedConnectResponsePacket functions
uint8_t ExtendedConnectResponsePacket::getSupportedFanSpeeds() const {
  uint8_t raw_value = ((pkt_.getPayloadByte(7) & 0x10) >> 2) + ((pkt_.getPayloadByte(8) & 0x08) >> 2) +
                      ((pkt_.getPayloadByte(9) & 0x02) >> 1);

  switch (raw_value) {
    case 1:
    case 2:
    case 4:
      return raw_value;
    case 0:
      return 3;
    case 6:
      return 5;

    default:
      ESP_LOGW(PACKETS_TAG, "Unexpected supported fan speeds: %i", raw_value);
      return 0;  // TODO: Depending on how this is used, it might be more useful to just return 3 and hope for the best
  }
}

climate::ClimateTraits ExtendedConnectResponsePacket::asTraits() const {
  auto ct = climate::ClimateTraits();

  // always enabled
  ct.add_supported_mode(climate::CLIMATE_MODE_COOL);
  ct.add_supported_mode(climate::CLIMATE_MODE_OFF);

  if (!this->isHeatDisabled())
    ct.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  if (!this->isDryDisabled())
    ct.add_supported_mode(climate::CLIMATE_MODE_DRY);
  if (!this->isFanDisabled())
    ct.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);

  if (this->supportsVaneSwing()) {
    ct.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);

    if (this->supportsVane() && this->supportsHVane())
      ct.add_supported_swing_mode(climate::CLIMATE_SWING_BOTH);
    if (this->supportsVane())
      ct.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);
    if (this->supportsHVane())
      ct.add_supported_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
  }

  ct.set_visual_min_temperature(std::min(this->getMinCoolDrySetpoint(), this->getMinHeatingSetpoint()));
  ct.set_visual_max_temperature(std::max(this->getMaxCoolDrySetpoint(), this->getMaxHeatingSetpoint()));

  // TODO: Figure out what these states *actually* map to so we aren't sending bad data.
  // This is probably a dynamic map, so the setter will need to be aware of things.
  switch (this->getSupportedFanSpeeds()) {
    case 1:
      ct.set_supported_fan_modes({climate::CLIMATE_FAN_HIGH});
      break;
    case 2:
      ct.set_supported_fan_modes({climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_HIGH});
      break;
    case 3:
      ct.set_supported_fan_modes({climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});
      break;
    case 4:
      ct.set_supported_fan_modes({
          climate::CLIMATE_FAN_QUIET,
          climate::CLIMATE_FAN_LOW,
          climate::CLIMATE_FAN_MEDIUM,
          climate::CLIMATE_FAN_HIGH,
      });
      break;
    case 5:
      ct.set_supported_fan_modes({
          climate::CLIMATE_FAN_QUIET,
          climate::CLIMATE_FAN_LOW,
          climate::CLIMATE_FAN_MEDIUM,
          climate::CLIMATE_FAN_HIGH,
      });
      ct.add_supported_custom_fan_mode("Very High");
      break;
    default:
      // no-op, don't set a fan mode.
      break;
  }
  if (!this->autoFanSpeedDisabled())
    ct.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);

  return ct;
}

}  // namespace mitsubishi_uart
}  // namespace esphome
