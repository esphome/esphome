#include "mitsubishi_itp.h"

namespace esphome {
namespace mitsubishi_itp {

void MitsubishiUART::route_packet_(const Packet &packet) {
  // If the packet is associated with the thermostat and just came from the thermostat, send it to the heatpump
  // If it came from the heatpump, send it back to the thermostat
  if (packet.get_controller_association() == ControllerAssociation::THERMOSTAT) {
    if (packet.get_source_bridge() == SourceBridge::THERMOSTAT) {
      hp_bridge_.send_packet(packet);
    } else if (packet.get_source_bridge() == SourceBridge::HEATPUMP) {
      ts_bridge_->send_packet(packet);
    }
  }
}

// Packet Handlers
void MitsubishiUART::process_packet(const Packet &packet) {
  ESP_LOGI(TAG, "Generic unhandled packet type %x received.", packet.get_packet_type());
  ESP_LOGD(TAG, "%s", packet.to_string().c_str());
  route_packet_(packet);
}

void MitsubishiUART::process_packet(const ConnectRequestPacket &packet) {
  // Nothing to be done for these except forward them along from thermostat to heat pump.
  // This method defined so that these packets are not "unhandled"
  ESP_LOGV(TAG, "Passing through inbound %s", packet.to_string().c_str());
  route_packet_(packet);
}
void MitsubishiUART::process_packet(const ConnectResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
  // Not sure if there's any needed content in this response, so assume we're connected.
  hp_connected_ = true;
  ESP_LOGI(TAG, "Heatpump connected.");
}

void MitsubishiUART::process_packet(const CapabilitiesRequestPacket &packet) {
  // Nothing to be done for these except forward them along from thermostat to heat pump.
  // This method defined so that these packets are not "unhandled"
  ESP_LOGV(TAG, "Passing through inbound %s", packet.to_string().c_str());
  route_packet_(packet);
}
void MitsubishiUART::process_packet(const CapabilitiesResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
  // Not sure if there's any needed content in this response, so assume we're connected.
  // TODO: Is there more useful info in these?
  hp_connected_ = true;
  capabilities_cache_ = packet;
  ESP_LOGI(TAG, "Received heat pump identification packet.");
}

void MitsubishiUART::process_packet(const GetRequestPacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());

  switch (packet.get_requested_command()) {
    case GetCommand::THERMOSTAT_STATE_DOWNLOAD:
      this->handle_thermostat_state_download_request(packet);
      break;
    case GetCommand::THERMOSTAT_GET_AB:
      this->handle_thermostat_ab_get_request(packet);
      break;
    default:
      route_packet_(packet);
  }
}

void MitsubishiUART::process_packet(const SettingsGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
  alert_listeners_(packet);

  // Mode

  const climate::ClimateMode old_mode = mode;
  if (packet.get_power()) {
    switch (packet.get_mode()) {
      case 0x01:
      case 0x09:  // i-see
        mode = climate::CLIMATE_MODE_HEAT;
        break;
      case 0x02:
      case 0x0A:  // i-see
        mode = climate::CLIMATE_MODE_DRY;
        break;
      case 0x03:
      case 0x0B:  // i-see
        mode = climate::CLIMATE_MODE_COOL;
        break;
      case 0x07:
        mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case 0x08:
      // unsure when 0x21 or 0x23 would ever be sent, as they seem to be Kumo exclusive, but let's handle them anyways.
      case 0x21:
      case 0x23:
        mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      default:
        mode = climate::CLIMATE_MODE_OFF;
    }
  } else {
    mode = climate::CLIMATE_MODE_OFF;
  }

  publish_on_update_ |= (old_mode != mode);

  // Temperature
  const float old_target_temperature = target_temperature;
  target_temperature = packet.get_target_temp();
  publish_on_update_ |= (old_target_temperature != target_temperature);
  if (mode <= MAX_RECALL_MODE_INDEX) {
    mode_recall_setpoints_[mode] = target_temperature;
  }

  switch (mode) {
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_DRY:
      this->mhk_state_.cool_setpoint_ = target_temperature;
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->mhk_state_.heat_setpoint_ = target_temperature;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      this->mhk_state_.cool_setpoint_ = target_temperature + 2;
      this->mhk_state_.heat_setpoint_ = target_temperature - 2;
    default:
      break;
  }

  // Fan
  static bool fan_changed = false;
  switch (packet.get_fan()) {
    case 0x00:
      fan_changed = set_fan_mode_(climate::CLIMATE_FAN_AUTO);
      break;
    case 0x01:
      fan_changed = set_fan_mode_(climate::CLIMATE_FAN_QUIET);
      break;
    case 0x02:
      fan_changed = set_fan_mode_(climate::CLIMATE_FAN_LOW);
      break;
    case 0x03:
      fan_changed = set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
      break;
    case 0x05:
      fan_changed = set_fan_mode_(climate::CLIMATE_FAN_HIGH);
      break;
    case 0x06:
      fan_changed = set_custom_fan_mode_(FAN_MODE_VERYHIGH);
      break;
  }

  publish_on_update_ |= fan_changed;
}

void MitsubishiUART::process_packet(const CurrentTempGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
  alert_listeners_(packet);
  // This will be the same as the remote temperature if we're using a remote sensor, otherwise the internal temp
  const float old_current_temperature = current_temperature;
  current_temperature = packet.get_current_temp();

  publish_on_update_ |= (old_current_temperature != current_temperature);
}

void MitsubishiUART::process_packet(const StatusGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
  alert_listeners_(packet);

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
  else if (packet.get_operating()) {
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

  publish_on_update_ |= (old_action != action);
}
void MitsubishiUART::process_packet(const RunStateGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
  alert_listeners_(packet);

  run_state_received_ = true;  // Set this since we received one

  // TODO: Not sure what AutoMode does yet
}

void MitsubishiUART::process_packet(const ErrorStateGetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
  alert_listeners_(packet);
}

void MitsubishiUART::process_packet(const Functions1GetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
}

void MitsubishiUART::process_packet(const Functions2GetResponsePacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());
  route_packet_(packet);
}

void MitsubishiUART::process_packet(const SettingsSetRequestPacket &packet) {
  ESP_LOGV(TAG, "Passing through inbound %s", packet.to_string().c_str());

  // forward this packet as-is; we're just intercepting to log.
  alert_listeners_(packet);
}

void MitsubishiUART::process_packet(const RemoteTemperatureSetRequestPacket &packet) {
  ESP_LOGV(TAG, "Processing %s", packet.to_string().c_str());

  // Only send this temperature packet to the heatpump if Thermostat is the selected source,
  // or we're in passive mode (since in passive mode we're not generating any packets to
  // set the temperature) otherwise just respond to the thermostat to keep it happy.
  if (current_temperature_source_ == TEMPERATURE_SOURCE_THERMOSTAT || !active_mode_) {
    route_packet_(packet);
  } else {
    ts_bridge_->send_packet(SetResponsePacket());
  }
  alert_listeners_(packet);

  float t = packet.get_remote_temperature();
  temperature_source_report(TEMPERATURE_SOURCE_THERMOSTAT, t);
}

void MitsubishiUART::process_packet(const ThermostatSensorStatusPacket &packet) {
  if (!enhanced_mhk_support_) {
    ESP_LOGV(TAG, "Passing through inbound %s", packet.to_string().c_str());

    route_packet_(packet);
    return;
  }

  ESP_LOGV(TAG, "Processing inbound %s", packet.to_string().c_str());

  alert_listeners_(packet);

  ts_bridge_->send_packet(SetResponsePacket());
}

void MitsubishiUART::process_packet(const ThermostatHelloPacket &packet) {
  if (!enhanced_mhk_support_) {
    ESP_LOGV(TAG, "Passing through inbound %s", packet.to_string().c_str());

    route_packet_(packet);
    return;
  }

  ESP_LOGV(TAG, "Processing inbound %s", packet.to_string().c_str());
  ts_bridge_->send_packet(SetResponsePacket());
}

void MitsubishiUART::process_packet(const ThermostatStateUploadPacket &packet) {
  if (!enhanced_mhk_support_) {
    ESP_LOGV(TAG, "Passing through inbound %s", packet.to_string().c_str());

    route_packet_(packet);
    return;
  }

  ESP_LOGV(TAG, "Processing inbound %s", packet.to_string().c_str());

  if (packet.get_flags() & 0x08)
    this->mhk_state_.heat_setpoint_ = packet.get_heat_setpoint();
  if (packet.get_flags() & 0x10)
    this->mhk_state_.cool_setpoint_ = packet.get_cool_setpoint();

  ts_bridge_->send_packet(SetResponsePacket());
}

void MitsubishiUART::process_packet(const ThermostatAASetRequestPacket &packet) {
  if (!enhanced_mhk_support_) {
    ESP_LOGV(TAG, "Passing through inbound %s", packet.to_string().c_str());

    route_packet_(packet);
    return;
  }

  ESP_LOGV(TAG, "Processing inbound %s", packet.to_string().c_str());

  ts_bridge_->send_packet(SetResponsePacket());
}

void MitsubishiUART::process_packet(const SetResponsePacket &packet) {
  ESP_LOGV(TAG, "Got Set Response packet, success = %s (code = %x)", packet.is_successful() ? "true" : "false",
           packet.get_result_code());
  route_packet_(packet);
}

// Process incoming data requests from an MHK probing for/running in enhanced mode
void MitsubishiUART::handle_thermostat_state_download_request(const GetRequestPacket &packet) {
  if (!enhanced_mhk_support_) {
    route_packet_(packet);
    return;
  }

  auto response = ThermostatStateDownloadResponsePacket();

  response.set_auto_mode((mode == climate::CLIMATE_MODE_HEAT_COOL || mode == climate::CLIMATE_MODE_AUTO));
  response.set_heat_setpoint(this->mhk_state_.heat_setpoint_);
  response.set_cool_setpoint(this->mhk_state_.cool_setpoint_);

#ifdef USE_TIME
  if (this->time_source_ != nullptr) {
    response.set_timestamp(this->time_source_->now());
  } else {
    ESP_LOGW(TAG, "No time source specified. Cannot provide accurate time!");
    response.set_timestamp(ESPTime::from_epoch_utc(1704067200));  // 2024-01-01 00:00:00Z
  }
#else
  ESP_LOGW(TAG, "No time source specified. Cannot provide accurate time!");
  response.set_timestamp(ESPTime::from_epoch_utc(1704067200));  // 2024-01-01 00:00:00Z
#endif

  ts_bridge_->send_packet(response);
}

void MitsubishiUART::handle_thermostat_ab_get_request(const GetRequestPacket &packet) {
  if (!enhanced_mhk_support_) {
    route_packet_(packet);
    return;
  }

  auto response = ThermostatABGetResponsePacket();

  ts_bridge_->send_packet(response);
}

}  // namespace mitsubishi_itp
}  // namespace esphome
