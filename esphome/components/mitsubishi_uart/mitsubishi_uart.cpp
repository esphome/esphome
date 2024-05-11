#include "mitsubishi_uart.h"

namespace esphome {
namespace mitsubishi_uart {

////
// MitsubishiUART
////

MitsubishiUART::MitsubishiUART(uart::UARTComponent *hp_uart_comp)
    : hp_uart{*hp_uart_comp}, hp_bridge{HeatpumpBridge(hp_uart_comp, this)} {
  /**
   * Climate pushes all its data to Home Assistant immediately when the API connects, this causes
   * the default 0 to be sent as temperatures, but since this is a valid value (0 deg C), it
   * can cause confusion and mess with graphs when looking at the state in HA.  Setting this to
   * NAN gets HA to treat this value as "unavailable" until we have a real value to publish.
   */
  target_temperature = NAN;
  current_temperature = NAN;
}

// Used to restore state of previous MUART-specific settings (like temperature source or pass-thru mode)
// Most other climate-state is preserved by the heatpump itself and will be retrieved after connection
void MitsubishiUART::setup() {
  // Populate select map
  for (size_t index = 0; index < temperature_source_select->traits.get_options().size(); index++) {
    temp_select_map[temperature_source_select->traits.get_options()[index]] = index;
  }

  // Using App.get_compilation_time() means these will get reset each time the firmware is updated, but this
  // is an easy way to prevent wierd conflicts if e.g. select options change.
  preferences_ = global_preferences->make_preference<MUARTPreferences>(get_object_id_hash() ^
                                                                       fnv1_hash(App.get_compilation_time()));
  restore_preferences();
}

void MitsubishiUART::save_preferences() {
  MUARTPreferences prefs{};

  // currentTemperatureSource
  // Save the index of the value stored in currentTemperatureSource just in case we're temporarily using Internal
  auto index = temp_select_map.find(currentTemperatureSource);
  if (index != temp_select_map.end()) {
    prefs.currentTemperatureSourceIndex = index->second;
  }

  preferences_.save(&prefs);
}

// Restores previously set values, or sets sane defaults
void MitsubishiUART::restore_preferences() {
  MUARTPreferences prefs;
  if (preferences_.load(&prefs)) {
    // currentTemperatureSource
    if (prefs.currentTemperatureSourceIndex.has_value() &&
        temperature_source_select->has_index(prefs.currentTemperatureSourceIndex.value()) &&
        temperature_source_select->at(prefs.currentTemperatureSourceIndex.value()).has_value()) {
      currentTemperatureSource = temperature_source_select->at(prefs.currentTemperatureSourceIndex.value()).value();
      temperature_source_select->publish_state(currentTemperatureSource);
      ESP_LOGCONFIG(TAG, "Preferences loaded.");
    } else {
      ESP_LOGCONFIG(TAG, "Preferences loaded, but unsuitable values.");
      currentTemperatureSource = TEMPERATURE_SOURCE_INTERNAL;
      temperature_source_select->publish_state(TEMPERATURE_SOURCE_INTERNAL);
    }
  } else {
    // TODO: Shouldn't need to define setting all these defaults twice
    ESP_LOGCONFIG(TAG, "Preferences not loaded.");
    currentTemperatureSource = TEMPERATURE_SOURCE_INTERNAL;
    temperature_source_select->publish_state(TEMPERATURE_SOURCE_INTERNAL);
  }
}

void MitsubishiUART::sendIfActive(const Packet &packet) {
  if (active_mode)
    hp_bridge.sendPacket(packet);
}

#define IFACTIVE(dothis) \
  if (active_mode) { \
    dothis \
  }
#define IFNOTACTIVE(dothis) \
  if (!active_mode) { \
    dothis \
  }

/* Used for receiving and acting on incoming packets as soon as they're available.
  Because packet processing happens as part of the receiving process, packet processing
  should not block for very long (e.g. no publishing inside the packet processing)
*/
void MitsubishiUART::loop() {
  // Loop bridge to handle sending and receiving packets
  hp_bridge.loop();
  if (ts_bridge)
    ts_bridge->loop();

  // If it's been too long since we received a temperature update (and we're not set to Internal)
  if (((millis() - lastReceivedTemperature) > TEMPERATURE_SOURCE_TIMEOUT_MS) &&
      (temperature_source_select->state != TEMPERATURE_SOURCE_INTERNAL)) {
    ESP_LOGW(TAG, "No temperature received from %s for %i milliseconds, reverting to Internal source",
             currentTemperatureSource.c_str(), TEMPERATURE_SOURCE_TIMEOUT_MS);
    // Set the select to show Internal (but do not change currentTemperatureSource)
    temperature_source_select->publish_state(TEMPERATURE_SOURCE_INTERNAL);
    // Send a packet to the heat pump to tell it to switch to internal temperature sensing
    IFACTIVE(hp_bridge.sendPacket(RemoteTemperatureSetRequestPacket().useInternalTemperature());)
  }
  //
  // Send packet to HP to tell it to use internal temp sensor
}

void MitsubishiUART::dump_config() {
  if (_capabilitiesCache.has_value()) {
    ESP_LOGCONFIG(TAG, "Discovered Capabilities: %s", _capabilitiesCache.value().to_string().c_str());
  }
}

/* Called periodically as PollingComponent; used to send packets to connect or request updates.

Possible TODO: If we only publish during updates, since data is received during loop, updates will always
be about `update_interval` late from their actual time.  Generally the update interval should be low enough
(default is 5seconds) this won't pose a practical problem.
*/
void MitsubishiUART::update() {
  // TODO: Temporarily wait 5 seconds on startup to help with viewing logs
  if (millis() < 5000) {
    return;
  }

  // If we're not yet connected, send off a connection request (we'll check again next update)
  if (!hpConnected) {
    IFACTIVE(hp_bridge.sendPacket(ConnectRequestPacket::instance());)
    return;
  }

  // Attempt to read capabilities on the next loop after connect.
  // TODO: This should likely be done immediately after connect, and will likely need to block setup for proper
  // autoconf.
  //       For now, just requesting it as part of our "init loops" is a good first step.
  if (!this->_capabilitiesRequested) {
    IFACTIVE(hp_bridge.sendPacket(ExtendedConnectRequestPacket::instance()); this->_capabilitiesRequested = true;)
  }

  // Before requesting additional updates, publish any changes waiting from packets received
  if (publishOnUpdate) {
    doPublish();

    publishOnUpdate = false;
  }

  IFACTIVE(
      // Request an update from the heatpump
      // TODO: This isn't a problem *yet*, but sending all these packets every loop might start to cause some issues in
      //       certain configurations or setups. We may want to consider only asking for certain packets on a rarer
      //       cadence, depending on their utility (e.g. we dont need to check for errors every loop).
      hp_bridge.sendPacket(
          GetRequestPacket::getSettingsInstance());  // Needs to be done before status packet for mode logic to work
      hp_bridge.sendPacket(GetRequestPacket::getStandbyInstance());
      hp_bridge.sendPacket(GetRequestPacket::getStatusInstance());
      hp_bridge.sendPacket(GetRequestPacket::getCurrentTempInstance());
      hp_bridge.sendPacket(GetRequestPacket::getErrorInfoInstance());)
}

void MitsubishiUART::doPublish() {
  publish_state();
  vane_position_select->publish_state(vane_position_select->state);
  horizontal_vane_position_select->publish_state(horizontal_vane_position_select->state);
  save_preferences();  // We can save this every time we publish as writes to flash are by default collected and delayed

  // Check sensors and publish if needed.
  // This is a bit of a hack to avoid needing to publish sensor data immediately as packets arrive.
  // Instead, packet data is written directly to `raw_state` (which doesn't update `state`).  If they
  // differ, calling `publish_state` will update `state` so that it won't be published later
  if (thermostat_temperature_sensor &&
      (thermostat_temperature_sensor->raw_state != thermostat_temperature_sensor->state)) {
    ESP_LOGI(TAG, "Thermostat temp differs, do publish");
    thermostat_temperature_sensor->publish_state(thermostat_temperature_sensor->raw_state);
  }
  if (compressor_frequency_sensor && (compressor_frequency_sensor->raw_state != compressor_frequency_sensor->state)) {
    ESP_LOGI(TAG, "Compressor frequency differs, do publish");
    compressor_frequency_sensor->publish_state(compressor_frequency_sensor->raw_state);
  }
  if (actual_fan_sensor && (actual_fan_sensor->raw_state != actual_fan_sensor->state)) {
    ESP_LOGI(TAG, "Actual fan speed differs, do publish");
    actual_fan_sensor->publish_state(actual_fan_sensor->raw_state);
  }
  if (error_code_sensor && (error_code_sensor->raw_state != error_code_sensor->state)) {
    ESP_LOGI(TAG, "Error code state differs, do publish");
    error_code_sensor->publish_state(error_code_sensor->raw_state);
  }

  // Binary sensors automatically dedup publishes (I think) and so will only actually publish on change
  service_filter_sensor->publish_state(service_filter_sensor->state);
  defrost_sensor->publish_state(defrost_sensor->state);
  hot_adjust_sensor->publish_state(hot_adjust_sensor->state);
  standby_sensor->publish_state(standby_sensor->state);
  isee_status_sensor->publish_state(isee_status_sensor->state);
}

bool MitsubishiUART::select_temperature_source(const std::string &state) {
  // TODO: Possibly check to see if state is available from the select options?  (Might be a bit redundant)

  currentTemperatureSource = state;
  // Reset the timeout for received temperature (without this, the menu dropdown will switch back to Internal
  // temporarily)
  lastReceivedTemperature = millis();

  // If we've switched to internal, let the HP know right away
  if (TEMPERATURE_SOURCE_INTERNAL == state) {
    IFACTIVE(hp_bridge.sendPacket(RemoteTemperatureSetRequestPacket().useInternalTemperature());)
  }

  return true;
}

bool MitsubishiUART::select_vane_position(const std::string &state) {
  IFNOTACTIVE(return false;)  // Skip this if we're not in active mode
  SettingsSetRequestPacket::VANE_BYTE positionByte = SettingsSetRequestPacket::VANE_AUTO;

  // NOTE: Annoyed that C++ doesn't have switches for strings, but since this is going to be called
  // infrequently, this is probably a better solution than over-optimizing via maps or something

  if (state == "Auto") {
    positionByte = SettingsSetRequestPacket::VANE_AUTO;
  } else if (state == "1") {
    positionByte = SettingsSetRequestPacket::VANE_1;
  } else if (state == "2") {
    positionByte = SettingsSetRequestPacket::VANE_2;
  } else if (state == "3") {
    positionByte = SettingsSetRequestPacket::VANE_3;
  } else if (state == "4") {
    positionByte = SettingsSetRequestPacket::VANE_4;
  } else if (state == "5") {
    positionByte = SettingsSetRequestPacket::VANE_5;
  } else if (state == "Swing") {
    positionByte = SettingsSetRequestPacket::VANE_SWING;
  } else {
    ESP_LOGW(TAG, "Unknown vane position %s", state.c_str());
    return false;
  }

  hp_bridge.sendPacket(SettingsSetRequestPacket().setVane(positionByte));
  return true;
}

bool MitsubishiUART::select_horizontal_vane_position(const std::string &state) {
  IFNOTACTIVE(return false;)  // Skip this if we're not in active mode
  SettingsSetRequestPacket::HORIZONTAL_VANE_BYTE positionByte = SettingsSetRequestPacket::HV_CENTER;

  // NOTE: Annoyed that C++ doesn't have switches for strings, but since this is going to be called
  // infrequently, this is probably a better solution than over-optimizing via maps or something

  if (state == "Auto") {
    positionByte = SettingsSetRequestPacket::HV_AUTO;
  } else if (state == "<<") {
    positionByte = SettingsSetRequestPacket::HV_LEFT_FULL;
  } else if (state == "<") {
    positionByte = SettingsSetRequestPacket::HV_LEFT;
  } else if (state == "|") {
    positionByte = SettingsSetRequestPacket::HV_CENTER;
  } else if (state == ">") {
    positionByte = SettingsSetRequestPacket::HV_RIGHT;
  } else if (state == ">>") {
    positionByte = SettingsSetRequestPacket::HV_RIGHT_FULL;
  } else if (state == "<>") {
    positionByte = SettingsSetRequestPacket::HV_SPLIT;
  } else if (state == "Swing") {
    positionByte = SettingsSetRequestPacket::HV_SWING;
  } else {
    ESP_LOGW(TAG, "Unknown horizontal vane position %s", state.c_str());
    return false;
  }

  hp_bridge.sendPacket(SettingsSetRequestPacket().setHorizontalVane(positionByte));
  return true;
}

// Called by temperature_source sensors to report values.  Will only take action if the currentTemperatureSource
// matches the incoming source.  Specifically this means that we are not storing any values
// for sensors other than the current source, and selecting a different source won't have any
// effect until that source reports a temperature.
// TODO: ? Maybe store all temperatures (and report on them using internal sensors??) so that selecting a new
// source takes effect immediately?  Only really needed if source sensors are configured with very slow update times.
void MitsubishiUART::temperature_source_report(const std::string &temperature_source, const float &v) {
  ESP_LOGI(TAG, "Received temperature from %s of %f. (Current source: %s)", temperature_source.c_str(), v,
           currentTemperatureSource.c_str());

  // Only proceed if the incomming source matches our chosen source.
  if (currentTemperatureSource == temperature_source) {
    // Reset the timeout for received temperature
    lastReceivedTemperature = millis();

    // Tell the heat pump about the temperature asap, but don't worry about setting it locally, the next update() will
    // get it
    IFACTIVE(RemoteTemperatureSetRequestPacket pkt = RemoteTemperatureSetRequestPacket(); pkt.setRemoteTemperature(v);
             hp_bridge.sendPacket(pkt);)

    // If we've changed the select to reflect a temporary reversion to a different source, change it back.
    if (temperature_source_select->state != temperature_source) {
      ESP_LOGI(TAG, "Temperature received, switching back to %s as source.", temperature_source.c_str());
      temperature_source_select->publish_state(temperature_source);
    }
  }
}

}  // namespace mitsubishi_uart
}  // namespace esphome
