#include "mitp_text-sensor.h"

namespace esphome {
namespace mitsubishi_itp {

void ErrorCodeSensor::process_packet(const ErrorStateGetResponsePacket &packet) {
  // TODO: Include friendly text from JSON, somehow.
  if (!packet.error_present()) {
    mitp_text_sensor_state_ = std::string("No Error Reported");
  } else if (auto raw_code = packet.get_raw_short_code() != 0x00) {
    // Not that it matters, but good for validation I guess.
    if ((raw_code & 0x1F) > 0x15) {
      ESP_LOGW(LISTENER_TAG, "Error short code %x had invalid low bits. This is an IT protocol violation!", raw_code);
    }

    mitp_text_sensor_state_ = "Error " + packet.get_short_code();
  } else {
    mitp_text_sensor_state_ = "Error " + to_string(packet.get_error_code());
  }
}

void ThermostatBatterySensor::process_packet(const ThermostatSensorStatusPacket &packet) {
  if (packet.get_flags() & 0x08) {
    mitp_text_sensor_state_ = THERMOSTAT_BATTERY_STATE_NAMES[packet.get_thermostat_battery_state()];
  }
}

}  // namespace mitsubishi_itp
}  // namespace esphome
