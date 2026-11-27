#include "pmsx003.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome::pmsx003 {

ESPHOME_LOG_TAG(TAG, "pmsx003");

static const uint16_t STABILISING_MS = 30000;  // time taken for the sensor to become stable after power on in ms

static const uint16_t CMD_MEASUREMENT_MODE_PASSIVE =
    0x0000;  // use `Command::MANUAL_MEASUREMENT` to trigger a measurement
static const uint16_t CMD_MEASUREMENT_MODE_ACTIVE = 0x0001;  // automatically perform measurements
static const uint16_t CMD_SLEEP_MODE_SLEEP = 0x0000;         // go to sleep mode
static const uint16_t CMD_SLEEP_MODE_WAKEUP = 0x0001;        // wake up from sleep mode

void PMSX003Component::setup() {}

void PMSX003Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PMSX003:");
  LOG_SENSOR("  ", "PM1.0STD", this->pm_1_0_std_sensor_);
  LOG_SENSOR("  ", "PM2.5STD", this->pm_2_5_std_sensor_);
  LOG_SENSOR("  ", "PM10.0STD", this->pm_10_0_std_sensor_);

  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);

  LOG_SENSOR("  ", "PM0.3um", this->pm_particles_03um_sensor_);
  LOG_SENSOR("  ", "PM0.5um", this->pm_particles_05um_sensor_);
  LOG_SENSOR("  ", "PM1.0um", this->pm_particles_10um_sensor_);
  LOG_SENSOR("  ", "PM2.5um", this->pm_particles_25um_sensor_);
  LOG_SENSOR("  ", "PM5.0um", this->pm_particles_50um_sensor_);
  LOG_SENSOR("  ", "PM10.0um", this->pm_particles_100um_sensor_);

  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);

  if (this->update_interval_ <= STABILISING_MS) {
    ESP_LOGCONFIG(TAG, "  Mode: active continuous (sensor default)");
  } else {
    ESP_LOGCONFIG(TAG, "  Mode: passive with sleep/wake cycles");
  }
}

void PMSX003Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // Initialize sensor mode on first loop
  if (!this->initialised_) {
    if (this->update_interval_ > STABILISING_MS) {
      // Long update interval: use passive mode with sleep/wake cycles
      this->send_command_(Command::MEASUREMENT_MODE, CMD_MEASUREMENT_MODE_PASSIVE);
      this->send_command_(Command::SLEEP_MODE, CMD_SLEEP_MODE_WAKEUP);
    } else {
      // Short/zero update interval: use active continuous mode
      this->send_command_(Command::MEASUREMENT_MODE, CMD_MEASUREMENT_MODE_ACTIVE);
    }
    this->initialised_ = true;
  }

  // If we update less often than it takes the device to stabilise, spin the fan down
  // rather than running it constantly. It does take some time to stabilise, so we
  // need to keep track of what state we're in.
  if (this->update_interval_ > STABILISING_MS) {
    switch (this->state_) {
      case State::IDLE:
        // Power on the sensor now so it'll be ready when we hit the update time
        if (now - this->last_update_ < (this->update_interval_ - STABILISING_MS))
          return;

        this->state_ = State::STABILISING;
        this->send_command_(Command::SLEEP_MODE, CMD_SLEEP_MODE_WAKEUP);
        this->fan_on_time_ = now;
        return;
      case State::STABILISING:
        // wait for the sensor to be stable
        if (now - this->fan_on_time_ < STABILISING_MS)
          return;
        // consume any command responses that are in the serial buffer, and any partial frame from before the sleep
        this->discard_available();
        this->reader_.reset();
        // Trigger a new read
        this->send_command_(Command::MANUAL_MEASUREMENT, 0);
        this->state_ = State::WAITING;
        break;
      case State::WAITING:
        // Just go ahead and read stuff
        break;
    }
  }

  auto handle = [this, now](const uint8_t *frame, size_t len) {
    if (!this->check_frame_(frame, len))
      return false;
    if (this->update_interval_ > STABILISING_MS || now - this->last_update_ >= this->update_interval_) {
      this->parse_data_(frame);
      this->last_update_ = now;
    }
    return true;
  };
  this->read_frames(this->reader_, handle, now, this->last_transmission_);
}

void warn_too_long(size_t len) {
  ESP_LOGW(TAG, "Payload length %u doesn't match. Are you using the correct PMSX003 type?",
           static_cast<unsigned>(len - PayloadLength::PAYLOAD_AT));
}

bool PMSX003Component::check_frame_(const uint8_t *frame, size_t total_size) {
  const uint16_t payload_length = total_size - PayloadLength::PAYLOAD_AT;
  if (!this->check_payload_length_(payload_length)) {
    ESP_LOGW(TAG, "Payload length %u doesn't match. Are you using the correct PMSX003 type?", payload_length);
    return false;
  }

  // checksum is without checksum bytes
  const uint16_t checksum = sum16_bytes(frame, total_size - 2);
  const uint16_t check = encode_uint16(frame[total_size - 2], frame[total_size - 1]);
  if (checksum != check) {
    ESP_LOGW(TAG, "PMSX003 checksum mismatch! 0x%02X != 0x%02X", checksum, check);
    return false;
  }
  return true;
}

bool PMSX003Component::check_payload_length_(uint16_t payload_length) {
  // https://avaldebe.github.io/PyPMS/sensors/Plantower/
  switch (this->type_) {
    case Type::PMS1003:
      return payload_length == 28;  // 2*13+2
    case Type::PMS3003:             // Data 7/8/9 not set/reserved
      return payload_length == 20;  // 2*9+2
    case Type::PMSX003:             // Data 13 not set/reserved
      // Deprecated: Length 20 is for PMS3003 backwards compatibility
      return payload_length == 28 || payload_length == 20;  // 2*13+2
    case Type::PMS5003S:
    case Type::PMS5003T:            // Data 13 not set/reserved
      return payload_length == 28;  // 2*13+2
    case Type::PMS5003ST:           // Data 16 not set/reserved
      return payload_length == 36;  // 2*17+2
    case Type::PMS9003M:
    case Type::LUFTMY:
      return payload_length == 28;  // 2*13+2
  }
  return false;
}

void PMSX003Component::send_command_(Command cmd, uint16_t data) {
  // Kept bytes of an earlier reply must not merge into this command's
  this->reader_.drop_kept();
  uint8_t send_data[7] = {
      START_CHARACTER_1,            // Start Byte 1
      START_CHARACTER_2,            // Start Byte 2
      static_cast<uint8_t>(cmd),    // Command
      uint8_t((data >> 8) & 0xFF),  // Data 1
      uint8_t((data >> 0) & 0xFF),  // Data 2
      0,                            // Verify Byte 1
      0,                            // Verify Byte 2
  };

  // Calculate checksum
  put_be16(send_data + 5, sum16_bytes(send_data, 5));

  for (auto send_byte : send_data) {
    this->write_byte(send_byte);
  }
}

void PMSX003Component::parse_data_(const uint8_t *frame) {
  // PM mass concentrations
  const uint16_t pm_1_0_std_concentration = encode_value<uint16_t>(frame + 4);
  const uint16_t pm_2_5_std_concentration = encode_value<uint16_t>(frame + 6);
  const uint16_t pm_10_0_std_concentration = encode_value<uint16_t>(frame + 8);

  PUBLISH_STATE(this->pm_1_0_std_sensor_, pm_1_0_std_concentration);
  PUBLISH_STATE(this->pm_2_5_std_sensor_, pm_2_5_std_concentration);
  PUBLISH_STATE(this->pm_10_0_std_sensor_, pm_10_0_std_concentration);
  ESP_LOGD(TAG,
           "Got PM1.0 Standard Concentration: %u µg/m³, PM2.5 Standard Concentration %u µg/m³, PM10.0 Standard "
           "Concentration: %u µg/m³",
           pm_1_0_std_concentration, pm_2_5_std_concentration, pm_10_0_std_concentration);

  // The Luftmy LD10, LD11, LD13, LD15 and LD16 report no atmospheric PM, so their particle counts start at byte 10.
  // All PMS types have atmospheric PM at bytes 10-15, so their particle counts start at byte 16.
  const bool is_luftmy = this->type_ == Type::LUFTMY;
  const uint8_t pc_offset = is_luftmy ? 10 : 16;

  if (!is_luftmy) {
    const uint16_t pm_1_0_concentration = encode_value<uint16_t>(frame + 10);
    const uint16_t pm_2_5_concentration = encode_value<uint16_t>(frame + 12);
    const uint16_t pm_10_0_concentration = encode_value<uint16_t>(frame + 14);

    ESP_LOGD(TAG, "Got PM1.0 Concentration: %u µg/m³, PM2.5 Concentration %u µg/m³, PM10.0 Concentration: %u µg/m³",
             pm_1_0_concentration, pm_2_5_concentration, pm_10_0_concentration);

    PUBLISH_STATE(this->pm_1_0_sensor_, pm_1_0_concentration);
    PUBLISH_STATE(this->pm_2_5_sensor_, pm_2_5_concentration);
    PUBLISH_STATE(this->pm_10_0_sensor_, pm_10_0_concentration);
  }

  // Particle counts at pc_offset: >0.3, >0.5, >1.0, >2.5µm
  const uint16_t pm_particles_03um = encode_value<uint16_t>(frame + pc_offset);
  const uint16_t pm_particles_05um = encode_value<uint16_t>(frame + pc_offset + 2);
  const uint16_t pm_particles_10um = encode_value<uint16_t>(frame + pc_offset + 4);
  const uint16_t pm_particles_25um = encode_value<uint16_t>(frame + pc_offset + 6);

  PUBLISH_STATE(this->pm_particles_03um_sensor_, pm_particles_03um);
  PUBLISH_STATE(this->pm_particles_05um_sensor_, pm_particles_05um);
  PUBLISH_STATE(this->pm_particles_10um_sensor_, pm_particles_10um);
  PUBLISH_STATE(this->pm_particles_25um_sensor_, pm_particles_25um);
  ESP_LOGD(TAG,
           "Got PM0.3 Particles: %u Count/0.1L, PM0.5 Particles: %u Count/0.1L, PM1.0 Particles: %u Count/0.1L, "
           "PM2.5 Particles: %u Count/0.1L",
           pm_particles_03um, pm_particles_05um, pm_particles_10um, pm_particles_25um);

  // PMS5003T uses pc_offset+8 and pc_offset+10 for temperature/humidity, not particle counts
  if (this->type_ != Type::PMS5003T) {
    const uint16_t pm_particles_50um = encode_value<uint16_t>(frame + pc_offset + 8);
    const uint16_t pm_particles_100um = encode_value<uint16_t>(frame + pc_offset + 10);

    ESP_LOGD(TAG, "Got PM5.0 Particles: %u Count/0.1L, PM10.0 Particles: %u Count/0.1L", pm_particles_50um,
             pm_particles_100um);

    PUBLISH_STATE(this->pm_particles_50um_sensor_, pm_particles_50um);
    PUBLISH_STATE(this->pm_particles_100um_sensor_, pm_particles_100um);
  }

  // Formaldehyde
  if (this->type_ == Type::PMS5003S || this->type_ == Type::PMS5003ST) {
    const uint16_t formaldehyde = encode_value<uint16_t>(frame + 28);

    ESP_LOGD(TAG, "Got Formaldehyde: %u µg/m^3", formaldehyde);

    PUBLISH_STATE(this->formaldehyde_sensor_, formaldehyde);
  }

  // Temperature and Humidity
  if (this->type_ == Type::PMS5003T || this->type_ == Type::PMS5003ST) {
    const uint8_t temperature_offset = (this->type_ == Type::PMS5003T) ? 24 : 30;

    const float temperature = encode_value<int16_t>(frame + temperature_offset) / 10.0f;
    const float humidity = encode_value<uint16_t>(frame + temperature_offset + 2) / 10.0f;

    ESP_LOGD(TAG, "Got Temperature: %.1f°C, Humidity: %.1f%%", temperature, humidity);

    PUBLISH_STATE(this->temperature_sensor_, temperature);
    PUBLISH_STATE(this->humidity_sensor_, humidity);
  }

  // Firmware Version and Error Code
  if (this->type_ == Type::PMS1003 || this->type_ == Type::PMS5003ST || this->type_ == Type::PMS9003M) {
    const uint8_t firmware_error_code_offset = (this->type_ == Type::PMS5003ST) ? 36 : 28;
    const uint8_t firmware_version = frame[firmware_error_code_offset];
    const uint8_t error_code = frame[firmware_error_code_offset + 1];

    ESP_LOGD(TAG, "Got Firmware Version: 0x%02X, Error Code: 0x%02X", firmware_version, error_code);
  }

  // Spin down the sensor again if we aren't going to need it until more time has
  // passed than it takes to stabilise
  if (this->update_interval_ > STABILISING_MS) {
    this->send_command_(Command::SLEEP_MODE, CMD_SLEEP_MODE_SLEEP);
    this->state_ = State::IDLE;
  }

  this->status_clear_warning();
}

}  // namespace esphome::pmsx003
