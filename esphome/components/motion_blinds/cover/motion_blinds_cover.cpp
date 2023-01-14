#include "motion_blinds_cover.h"
#include "esphome/core/log.h"

#include <sstream>

#ifdef USE_ESP32

namespace esphome {
namespace motion_blinds {

static const char *const TAG = "motion_blinds_cover";
static const char* const COMMAND_SET_POSITION = "05020440";
static const char* const COMMAND_STOP = "03020303";
static const char* const NOTIFICATION_POSITION = "07040402";

using namespace esphome::cover;

void MotionBlindsComponent::dump_config() {
  LOG_COVER("", "Motion Blinds Cover", this);
  ESP_LOGCONFIG(TAG, "  Invert Position: %d", (int) this->invert_position_);
}

void MotionBlindsComponent::setup() { this->position = COVER_OPEN; }

void MotionBlindsComponent::loop() {
  // Nothing to do
}

CoverTraits MotionBlindsComponent::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  traits.set_is_assumed_state(false);
  return traits;
}

void MotionBlindsComponent::control(const CoverCall &call) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot send cover control, not connected", this->get_name().c_str());
    return;
  }
  if (call.get_stop()) {
    this->send_command(COMMAND_STOP);
  }
  if (call.get_position().has_value()) {
    auto int_value = std::min(100, std::max(0, static_cast<int>(this->invert_position_value_(call.get_position().value()) * 100)));
    std::stringstream buffer;
    buffer << COMMAND_SET_POSITION;
    buffer << MotionBlindsCommunication::format_hex_num(int_value, false);
    buffer << "00";
    this->send_command(buffer.str());
  }
}

std::string MotionBlindsComponent::get_logging_device_name() { return this->get_name(); }

void MotionBlindsComponent::on_disconnected() {
  // Nothing to do
  this->
}

void MotionBlindsComponent::on_notify(const std::string &data) {
  if (data.find(NOTIFICATION_POSITION) == 0 && data.length() > 13) {
    auto percentage = data.substr(12, 2);
    int temp;
    std::stringstream buffer;
    buffer << std::hex << percentage;
    buffer >> temp;
    auto value = this->invert_position_value_(std::max(0, std::min(100, static_cast<int>(temp))) / 100.0f);
    this->position = value;  // 0.0 = closed, 1.0 = open
    this->publish_state();
  }
}

}  // namespace motion_blinds
}  // namespace esphome

#endif
