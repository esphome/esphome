#include "hon_water_heater.h"

#include <cmath>

#include "esphome/core/application.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"

namespace esphome::haier {

static const char *const TAG = "haier.water_heater";
using water_heater_protocol::HonMode;

// TextSensor accepts ordinary strings rather than flash-resident LogString values.
static const char *hon_mode_label(HonMode mode) {
  switch (mode) {
    case HonMode::HON_MODE_NORMAL:
      return "Normal";
    case HonMode::HON_MODE_ECO:
      return "ECO";
    case HonMode::HON_MODE_BPS:
      return "BPS";
    case HonMode::HON_MODE_UNKNOWN:
      return "Unknown";
  }
  return "Unknown";
}

void HonWaterHeater::update_network_status_() {
  bool ready = false;
#if defined(USE_WIFI) || defined(USE_ETHERNET)
  const auto connected_with_ip = [](auto *interface) {
    if (interface == nullptr || !interface->is_connected())
      return false;
    for (const auto &ip : interface->get_ip_addresses()) {
      if (ip.is_set())
        return true;
    }
    return false;
  };
#ifdef USE_ETHERNET
  ready |= connected_with_ip(ethernet::global_eth_component);
#endif
#ifdef USE_WIFI
  ready |= connected_with_ip(wifi::global_wifi_component);
#endif
#endif
  const uint8_t status = ready ? 0x07 : 0x05;
  if (status != this->network_status_) {
    this->network_status_ = status;
    this->network_pending_ = true;
    ESP_LOGI(TAG, "Local network status changed: %02X", status);
  }
}

void HonWaterHeater::setup() {
  this->status_set_warning("Waiting for a valid appliance status frame");
  if (this->communication_sensor_ != nullptr)
    this->communication_sensor_->publish_state(false);
  this->update_network_status_();
  this->set_interval("network_status", 500, [this]() { this->update_network_status_(); });
  this->set_interval("status_poll", 5000, [this]() { this->poll_status_(); });
}

void HonWaterHeater::send_frame_(uint8_t type, const uint8_t *payload, size_t size) {
  uint8_t wire[28];
  const size_t length = water_heater_protocol::encode_frame(type, payload, size, wire);
  if (length == 0)
    return;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex[format_hex_pretty_size(sizeof(wire))];
  ESP_LOGV(TAG, "TX type=%02X flags=00: %s", type, format_hex_pretty_to(hex, wire, length));
#endif
  this->write_array(wire, length);
}

void HonWaterHeater::poll_status_() {
  const uint32_t now = App.get_loop_component_start_time();
  this->expire_poll_(now);
  if (this->waiting_network_ || this->waiting_poll_)
    return;
  if (this->command_count_ != 0 || this->pending_off_) {
    return;
  }
  if (this->network_pending_ || now - this->network_sent_ >= 30000)
    return;
  // Try each read-only request even if initialization is unanswered. Revisit version/ID every minute.
  const uint8_t version[] = {0x00, 0x07};
  const uint8_t status[] = {0x4D, 0x01};
  if (this->poll_stage_ == 0) {
    this->expected_type_ = 0x62;
    this->send_frame_(0x61, version, sizeof(version));
  } else if (this->poll_stage_ == 1) {
    this->expected_type_ = 0x71;
    this->send_frame_(0x70);
  } else {
    this->expected_type_ = 0x02;
    this->send_frame_(0x01, status, sizeof(status));
  }
  this->poll_sent_ = now;
  this->waiting_poll_ = true;
  this->poll_stage_ = (this->poll_stage_ + 1) % 12;
}

void HonWaterHeater::expire_poll_(uint32_t now) {
  if (this->waiting_poll_ && now - this->poll_sent_ >= 5000) {
    ESP_LOGW(TAG, "No matching response type=%02X in 5 seconds", this->expected_type_);
    this->waiting_poll_ = false;
  }
}

void HonWaterHeater::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  this->expire_poll_(now);
  if (this->have_status_ && now - this->last_status_ >= this->communication_timeout_) {
    this->have_status_ = false;
    this->status_set_warning("Heater communication timed out");
    ESP_LOGW(TAG, "Heater communication lost: no valid status for %u ms",
             static_cast<unsigned>(this->communication_timeout_));
    if (this->communication_sensor_ != nullptr)
      this->communication_sensor_->publish_state(false);
  }
  if (this->waiting_control_ && now - this->control_sent_ > 5000) {
    ESP_LOGW(TAG, "Control response timed out; discarding remaining commands");
    this->waiting_control_ = false;
    this->command_count_ = 0;
  }
  if (this->waiting_network_ && now - this->network_sent_ > 2000) {
    ESP_LOGW(TAG, "Network status: no CONFIRM within 2 seconds");
    this->waiting_network_ = false;
  }
  if (this->pending_off_ && !this->waiting_control_ && !this->waiting_poll_ && !this->waiting_network_) {
    this->commands_[0] = {0x5D, 0x01, 0x00, 0x00};
    this->command_index_ = 0;
    this->command_count_ = 1;
    this->pending_off_ = false;
  }
  if (!this->pending_off_ && !this->waiting_network_ && !this->waiting_poll_ && this->command_count_ == 0 &&
      (this->network_pending_ || now - this->network_sent_ >= 30000)) {
    // Signal remains a placeholder; status represents local IP readiness, not cloud access.
    const uint8_t payload[] = {0x00, this->network_status_, 0x00, 100};
    this->send_frame_(0xF7, payload, sizeof(payload));
    this->network_sent_ = now;
    this->network_pending_ = false;
    this->waiting_network_ = true;
  }
  if (this->command_count_ != 0 && !this->waiting_control_ && !this->waiting_poll_ && !this->waiting_network_)
    this->send_next_control_();
  if (now - this->last_byte_ > 300)
    this->parser_.reset();
  for (size_t i = 0; i < 256 && this->available() > 0; i++) {
    uint8_t value;
    if (!this->read_byte(&value))
      break;
    this->last_byte_ = now;
    if (this->parser_.feed(value))
      this->process_frame_();
    else if (this->parser_.complete()) {
      if ((this->parser_.data()[1] & 0x80) != 0)
        ESP_LOGW(TAG, "RX: XOR-encrypted frames (flag 0x80) are not supported and will be dropped");
      else
        ESP_LOGW(TAG, "RX: unsupported flags or invalid checksum/CRC");
    }
  }
}

void HonWaterHeater::process_frame_() {
  const uint8_t *frame = this->parser_.data();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex[format_hex_pretty_size(244)];
  ESP_LOGV(TAG, "RX decoded type=%02X flags=%02X: %s", frame[7], frame[1],
           format_hex_pretty_to(hex, frame, this->parser_.size()));
#endif
  if (this->waiting_poll_ && frame[7] == this->expected_type_) {
    ESP_LOGV(TAG, "Response matched type=%02X", frame[7]);
    this->waiting_poll_ = false;
  }
  if (frame[7] == 0xF0)
    this->network_pending_ = true;
  if (frame[7] == 0x05 && this->waiting_network_) {
    ESP_LOGD(TAG, "Network status acknowledged");
    this->waiting_network_ = false;
  }
  if (frame[7] == 0x03) {
    this->waiting_network_ = false;
    ESP_LOGW(TAG, "Appliance rejected request; discarding pending controls");
    this->waiting_control_ = false;
    this->command_count_ = 0;
  }
  if (frame[7] == 0x06)
    this->send_frame_(0x05);

  water_heater_protocol::Status status{};
  if (!water_heater_protocol::decode_status(this->parser_.data(), this->parser_.size(), status))
    return;
  if (!this->have_status_) {
    ESP_LOGI(TAG, "Heater communication established");
    if (this->communication_sensor_ != nullptr)
      this->communication_sensor_->publish_state(true);
  }
  if (status.mode == HonMode::HON_MODE_UNKNOWN &&
      (!this->have_status_ || this->reported_mode_ != HonMode::HON_MODE_UNKNOWN)) {
    const uint8_t *payload = frame + 8;
    ESP_LOGW(TAG,
             "Unknown heater mode: payload[38..41]=%02X %02X %02X %02X; using last recognized active mode, or Electric "
             "if none is known",
             payload[38], payload[39], payload[40], payload[41]);
  }
  this->have_status_ = true;
  this->last_status_ = App.get_loop_component_start_time();
  this->reported_mode_ = status.mode;
  if (this->waiting_control_ && frame[7] == 0x02) {
    const auto &command = this->commands_[this->command_index_];
    const bool matched =
        (command[1] == 0x01 && status.powered == (command[3] != 0)) ||
        (command[1] == 0x02 && status.target_temperature == command[3] + 30) ||
        (command[1] == 0x05 && status.mode == (command[3] ? HonMode::HON_MODE_ECO : HonMode::HON_MODE_NORMAL)) ||
        (command[1] == 0x17 && status.mode == HonMode::HON_MODE_BPS);
    if (matched) {
      ESP_LOGD(TAG, "Control confirmed: parameter=%02X value=%02X", command[1], command[3]);
      this->waiting_control_ = false;
      if (++this->command_index_ == this->command_count_)
        this->command_count_ = 0;
    }
  }
  this->status_clear_warning();
  this->current_temperature_ = status.current_temperature;
  this->target_temperature_ = status.target_temperature;
  this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, status.powered);
  switch (status.mode) {
    case HonMode::HON_MODE_NORMAL:
      this->last_active_mode_ = water_heater::WATER_HEATER_MODE_ELECTRIC;
      break;
    case HonMode::HON_MODE_ECO:
      this->last_active_mode_ = water_heater::WATER_HEATER_MODE_ECO;
      break;
    case HonMode::HON_MODE_BPS:
      this->last_active_mode_ = water_heater::WATER_HEATER_MODE_PERFORMANCE;
      break;
    case HonMode::HON_MODE_UNKNOWN:
      break;
  }
  // The base entity has no Unknown mode; the text sensor preserves the decoded result.
  this->mode_ = status.powered ? this->last_active_mode_ : water_heater::WATER_HEATER_MODE_OFF;
  if (this->primary_sensor_ != nullptr)
    this->primary_sensor_->publish_state(status.current_temperature);
  if (this->secondary_sensor_ != nullptr)
    this->secondary_sensor_->publish_state(status.secondary_temperature <= 100 ? status.secondary_temperature : NAN);
  if (this->target_sensor_ != nullptr)
    this->target_sensor_->publish_state(status.target_temperature);
  if (this->heating_time_sensor_ != nullptr)
    this->heating_time_sensor_->publish_state(status.estimated_heating_time);
  if (this->water_sensor_ != nullptr) {
    static constexpr uint8_t percentages[] = {0, 33, 66, 100};
    this->water_sensor_->publish_state(status.available_water <= 3 ? percentages[status.available_water] : NAN);
  }
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(status.powered);
  if (this->heating_sensor_ != nullptr)
    this->heating_sensor_->publish_state(status.heating);
  if (this->mode_sensor_ != nullptr)
    this->mode_sensor_->publish_state(hon_mode_label(status.mode));
  this->publish_state();
}

void HonWaterHeater::dump_config() {
  LOG_WATER_HEATER("", "Haier hOn", this);
  ESP_LOGCONFIG(TAG, "  Local hOn controller; polling interval: 5s");
}

water_heater::WaterHeaterCallInternal HonWaterHeater::make_call() {
  return water_heater::WaterHeaterCallInternal(this);
}

void HonWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  using namespace water_heater;
  const auto mode = call.get_mode();
  const auto on = call.get_on();
  const bool off = (on.has_value() && !*on) || (mode.has_value() && *mode == WATER_HEATER_MODE_OFF);
  if (off) {
    // Preserve only the in-flight command until its reply/timeout; discard unsent writes.
    if (this->waiting_control_) {
      this->commands_[0] = this->commands_[this->command_index_];
      this->command_index_ = 0;
      this->command_count_ = 1;
    } else {
      this->command_count_ = 0;
    }
    this->pending_off_ = true;
    return;
  }
  if (!this->have_status_ || App.get_loop_component_start_time() - this->last_status_ > 15000) {
    ESP_LOGW(TAG, "Control rejected: no recent appliance status");
    return;
  }
  if (this->command_count_ != 0 || this->pending_off_) {
    ESP_LOGW(TAG, "Control busy; wait for the previous request to finish");
    return;
  }
  const float temperature = call.get_target_temperature();
  if (!std::isnan(temperature) &&
      (!std::isfinite(temperature) || temperature < 35 || temperature > 75 || std::round(temperature) != temperature ||
       (mode.has_value() ? *mode != WATER_HEATER_MODE_ELECTRIC : this->reported_mode_ != HonMode::HON_MODE_NORMAL))) {
    ESP_LOGW(TAG, "Target requires Normal mode and a whole temperature from 35 to 75 C");
    return;
  }
  this->command_index_ = 0;
  auto append = [this](uint8_t parameter, uint8_t value) {
    this->commands_[this->command_count_++] = {0x5D, parameter, 0x00, value};
  };
  if ((on.has_value() && *on) || (mode.has_value() && !this->is_on()))
    append(0x01, 0x01);
  if (mode.has_value()) {
    // Captures establish transitions through Normal, not direct ECO/BPS transitions.
    if (this->reported_mode_ != HonMode::HON_MODE_NORMAL && *mode != WATER_HEATER_MODE_ELECTRIC)
      append(0x05, 0x00);
    if (*mode == WATER_HEATER_MODE_ELECTRIC)
      append(0x05, 0x00);
    else if (*mode == WATER_HEATER_MODE_ECO)
      append(0x05, 0x01);
    else if (*mode == WATER_HEATER_MODE_PERFORMANCE)
      append(0x17, 0x01);
  }
  if (!std::isnan(temperature))
    append(0x02, static_cast<uint8_t>(temperature - 30));
}

void HonWaterHeater::send_next_control_() {
  this->send_frame_(0x01, this->commands_[this->command_index_].data(), 4);
  this->waiting_control_ = true;
  this->control_sent_ = App.get_loop_component_start_time();
}

water_heater::WaterHeaterTraits HonWaterHeater::traits() {
  water_heater::WaterHeaterTraits traits;
  traits.set_supports_current_temperature(true);
  using namespace water_heater;
  traits.add_feature_flags(WATER_HEATER_SUPPORTS_TARGET_TEMPERATURE | WATER_HEATER_SUPPORTS_OPERATION_MODE |
                           WATER_HEATER_SUPPORTS_ON_OFF);
  traits.set_supported_modes(
      {WATER_HEATER_MODE_OFF, WATER_HEATER_MODE_ELECTRIC, WATER_HEATER_MODE_ECO, WATER_HEATER_MODE_PERFORMANCE});
  traits.set_min_temperature(35);
  traits.set_max_temperature(75);
  traits.set_target_temperature_step(1);

  return traits;
}

}  // namespace esphome::haier
