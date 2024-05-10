#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_uart {

void MitsubishiUART::routePacket(const Packet &packet) {
  // If the packet is associated with the thermostat and just came from the thermostat, send it to the heatpump
  // If it came from the heatpump, send it back to the thermostat
  if (packet.getControllerAssociation() == ControllerAssociation::thermostat) {
    if (packet.getSourceBridge() == SourceBridge::thermostat) {
      hp_bridge.sendPacket(packet);
    } else if (packet.getSourceBridge() == SourceBridge::heatpump) {
      ts_bridge->sendPacket(packet);
    }
  }
}

// Packet Handlers
void MitsubishiUART::processPacket(const Packet &packet) {
  ESP_LOGI(TAG, "Generic unhandled packet type %x received.", packet.getPacketType());
  ESP_LOGD(TAG, "%s", packet.to_string().c_str());
  routePacket(packet);
};

void MitsubishiUART::processPacket(const ConnectRequestPacket &packet) {
  // Nothing to be done for these except forward them along from thermostat to heat pump.
  // This method defined so that these packets are not "unhandled"
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);
};
void MitsubishiUART::processPacket(const ConnectResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);
  // Not sure if there's any needed content in this response, so assume we're connected.
  hpConnected = true;
  ESP_LOGI(TAG, "Heatpump connected.");
};

void MitsubishiUART::processPacket(const ExtendedConnectRequestPacket &packet) {
  // Nothing to be done for these except forward them along from thermostat to heat pump.
  // This method defined so that these packets are not "unhandled"
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);
};
void MitsubishiUART::processPacket(const ExtendedConnectResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);
  // Not sure if there's any needed content in this response, so assume we're connected.
  // TODO: Is there more useful info in these?
  hpConnected = true;
  _capabilitiesCache = packet;
  ESP_LOGI(TAG, "Received heat pump identification packet.");
};

void MitsubishiUART::processPacket(const GetRequestPacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);
  // These are just requests for information from the thermostat.  For now, nothing to be done
  // except route them.  In the future, we could use this to inject information for the thermostat
  // or use a cached value.
}

void MitsubishiUART::processPacket(const SettingsGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);

  // Mode

  const climate::ClimateMode old_mode = mode;
  if (packet.getPower()) {
    switch (packet.getMode()) {
      case 0x01:
        mode = climate::CLIMATE_MODE_HEAT;
        break;
      case 0x02:
        mode = climate::CLIMATE_MODE_DRY;
        break;
      case 0x03:
        mode = climate::CLIMATE_MODE_COOL;
        break;
      case 0x07:
        mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case 0x08:
        mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      default:
        mode = climate::CLIMATE_MODE_OFF;
    }
  } else {
    mode = climate::CLIMATE_MODE_OFF;
  }

  publishOnUpdate |= (old_mode != mode);

  // Temperature
  const float old_target_temperature = target_temperature;
  target_temperature = packet.getTargetTemp();
  publishOnUpdate |= (old_target_temperature != target_temperature);

  // Fan
  static bool fanChanged = false;
  switch (packet.getFan()) {
    case 0x00:
      fanChanged = set_fan_mode_(climate::CLIMATE_FAN_AUTO);
      break;
    case 0x01:
      fanChanged = set_fan_mode_(climate::CLIMATE_FAN_QUIET);
      break;
    case 0x02:
      fanChanged = set_fan_mode_(climate::CLIMATE_FAN_LOW);
      break;
    case 0x03:
      fanChanged = set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
      break;
    case 0x05:
      fanChanged = set_fan_mode_(climate::CLIMATE_FAN_HIGH);
      break;
    case 0x06:
      fanChanged = set_custom_fan_mode_(FAN_MODE_VERYHIGH);
      break;
  }

  publishOnUpdate |= fanChanged;

  // TODO: It would probably be nice to have the enum->string mapping defined somewhere to avoid typos/errors
  const std::string old_vane_position = vane_position_select->state;
  switch (packet.getVane()) {
    case SettingsSetRequestPacket::VANE_AUTO:
      vane_position_select->state = "Auto";
      break;
    case SettingsSetRequestPacket::VANE_1:
      vane_position_select->state = "1";
      break;
    case SettingsSetRequestPacket::VANE_2:
      vane_position_select->state = "2";
      break;
    case SettingsSetRequestPacket::VANE_3:
      vane_position_select->state = "3";
      break;
    case SettingsSetRequestPacket::VANE_4:
      vane_position_select->state = "4";
      break;
    case SettingsSetRequestPacket::VANE_5:
      vane_position_select->state = "5";
      break;
    case SettingsSetRequestPacket::VANE_SWING:
      vane_position_select->state = "Swing";
      break;
    default:
      ESP_LOGW(TAG, "Vane in unknown position %x", packet.getVane());
  }
  publishOnUpdate |= (old_vane_position != vane_position_select->state);

  const std::string old_horizontal_vane_position = horizontal_vane_position_select->state;
  switch (packet.getHorizontalVane()) {
    case SettingsSetRequestPacket::HV_AUTO:
      horizontal_vane_position_select->state = "Auto";
      break;
    case SettingsSetRequestPacket::HV_LEFT_FULL:
      horizontal_vane_position_select->state = "<<";
      break;
    case SettingsSetRequestPacket::HV_LEFT:
      horizontal_vane_position_select->state = "<";
      break;
    case SettingsSetRequestPacket::HV_CENTER:
      horizontal_vane_position_select->state = "|";
      break;
    case SettingsSetRequestPacket::HV_RIGHT:
      horizontal_vane_position_select->state = ">";
      break;
    case SettingsSetRequestPacket::HV_RIGHT_FULL:
      horizontal_vane_position_select->state = ">>";
      break;
    case SettingsSetRequestPacket::HV_SPLIT:
      horizontal_vane_position_select->state = "<>";
      break;
    case SettingsSetRequestPacket::HV_SWING:
      horizontal_vane_position_select->state = "Swing";
      break;
    default:
      ESP_LOGW(TAG, "Vane in unknown horizontal position %x", packet.getHorizontalVane());
  }
  publishOnUpdate |= (old_horizontal_vane_position != horizontal_vane_position_select->state);
};

void MitsubishiUART::processPacket(const CurrentTempGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);
  // This will be the same as the remote temperature if we're using a remote sensor, otherwise the internal temp
  const float old_current_temperature = current_temperature;
  current_temperature = packet.getCurrentTemp();

  publishOnUpdate |= (old_current_temperature != current_temperature);
};

void MitsubishiUART::processPacket(const StatusGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);
  const climate::ClimateAction old_action = action;

  // If mode is off, action is off
  if (mode == climate::CLIMATE_MODE_OFF) {
    action = climate::CLIMATE_ACTION_OFF;
  }
  // If mode is fan only, packet.getOperating() may be false, but the fan is running
  else if (mode == climate::CLIMATE_MODE_FAN_ONLY) {
    action = climate::CLIMATE_ACTION_FAN;
  }
  // If mode is anything other than off or fan, and the unit is operating, determine the action
  else if (packet.getOperating()) {
    switch (mode) {
      case climate::CLIMATE_MODE_HEAT:
        action = climate::CLIMATE_ACTION_HEATING;
        break;
      case climate::CLIMATE_MODE_COOL:
        action = climate::CLIMATE_ACTION_COOLING;
        break;
      case climate::CLIMATE_MODE_DRY:
        action = climate::CLIMATE_ACTION_DRYING;
        break;
      // TODO: This only works if we get an update while the temps are in this configuration
      // Surely there's some info from the heat pump about which of these modes it's in?
      case climate::CLIMATE_MODE_HEAT_COOL:
        if (current_temperature > target_temperature) {
          action = climate::CLIMATE_ACTION_COOLING;
        } else if (current_temperature < target_temperature) {
          action = climate::CLIMATE_ACTION_HEATING;
        }
        // When the heat pump *changes* to a new action, these temperature comparisons should be accurate.
        // If the mode hasn't changed, but the temps are equal, we can assume the same action and make no change.
        // If the unit overshoots, this still doesn't work.
        break;
      default:
        ESP_LOGW(TAG, "Unhandled mode %i.", mode);
        break;
    }
  }
  // If we're not operating (but not off or in fan mode), we're idle
  // Should be relatively safe to fall through any unknown modes into showing IDLE
  else {
    action = climate::CLIMATE_ACTION_IDLE;
  }

  publishOnUpdate |= (old_action != action);

  if (compressor_frequency_sensor) {
    const float old_compressor_frequency = compressor_frequency_sensor->raw_state;

    compressor_frequency_sensor->raw_state = packet.getCompressorFrequency();

    publishOnUpdate |= (old_compressor_frequency != compressor_frequency_sensor->raw_state);
  }
};
void MitsubishiUART::processPacket(const StandbyGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);

  if (service_filter_sensor) {
    const bool old_service_filter = service_filter_sensor->state;
    service_filter_sensor->state = packet.serviceFilter();
    publishOnUpdate |= (old_service_filter != service_filter_sensor->state);
  }

  if (defrost_sensor) {
    const bool old_defrost = defrost_sensor->state;
    defrost_sensor->state = packet.inDefrost();
    publishOnUpdate |= (old_defrost != defrost_sensor->state);
  }

  if (hot_adjust_sensor) {
    const bool old_hot_adjust = hot_adjust_sensor->state;
    hot_adjust_sensor->state = packet.inHotAdjust();
    publishOnUpdate |= (old_hot_adjust != hot_adjust_sensor->state);
  }

  if (standby_sensor) {
    const bool old_standby = standby_sensor->state;
    standby_sensor->state = packet.inStandby();
    publishOnUpdate |= (old_standby != standby_sensor->state);
  }

  if (actual_fan_sensor) {
    const auto old_actual_fan = actual_fan_sensor->raw_state;
    actual_fan_sensor->raw_state = ACTUAL_FAN_SPEED_NAMES[packet.getActualFanSpeed()];
    publishOnUpdate |= (old_actual_fan != actual_fan_sensor->raw_state);
  }

  // TODO: Not sure what AutoMode does yet
}

void MitsubishiUART::processPacket(const ErrorStateGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  routePacket(packet);

  std::string oldErrorCode = error_code_sensor->raw_state;

  // TODO: Include friendly text from JSON, somehow.
  if (!packet.errorPresent()) {
    error_code_sensor->raw_state = "No Error Reported";
  } else if (auto rawCode = packet.getRawShortCode() != 0x00) {
    // Not that it matters, but good for validation I guess.
    if ((rawCode & 0x1F) > 0x15) {
      ESP_LOGW(TAG, "Error short code %x had invalid low bits. This is an IT protocol violation!", rawCode);
    }

    error_code_sensor->raw_state = "Error " + packet.getShortCode();
  } else {
    error_code_sensor->raw_state = "Error " + to_string(packet.getErrorCode());
  }

  publishOnUpdate |= (oldErrorCode != error_code_sensor->raw_state);
}

void MitsubishiUART::processPacket(const RemoteTemperatureSetRequestPacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());

  // Only send this temperature packet to the heatpump if Thermostat is the selected source,
  // or we're in passive mode (since in passive mode we're not generating any packets to
  // set the temperature) otherwise just respond to the thermostat to keep it happy.
  if (currentTemperatureSource == TEMPERATURE_SOURCE_THERMOSTAT || !active_mode) {
    routePacket(packet);
  } else {
    ts_bridge->sendPacket(SetResponsePacket());
  }

  float t = packet.getRemoteTemperature();
  temperature_source_report(TEMPERATURE_SOURCE_THERMOSTAT, t);

  if (thermostat_temperature_sensor) {
    const float old_thermostat_temp = thermostat_temperature_sensor->raw_state;

    thermostat_temperature_sensor->raw_state = t;

    publishOnUpdate |= (old_thermostat_temp != thermostat_temperature_sensor->raw_state);
  }
};
void MitsubishiUART::processPacket(const SetResponsePacket &packet) {
  ESP_LOGV(TAG, "Got Set Response packet, success = %s (code = %x)", packet.isSuccessful() ? "true" : "false",
           packet.getResultCode());
  routePacket(packet);
}

}  // namespace mitsubishi_uart
}  // namespace esphome
