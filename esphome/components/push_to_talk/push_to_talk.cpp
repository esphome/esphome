#include "push_to_talk.h"

#include "esphome/core/log.h"

namespace esphome {
namespace push_to_talk {

static const char *const TAG = "push_to_talk";

PushToTalk::PushToTalk() { voice_assistant::global_voice_assistant = this; }

void PushToTalk::setup() {
  if (!VoiceAssistant::setup_udp_socket_()) {
    ESP_LOGW(TAG, "Could not set up UDP socket.");
    this->mark_failed();
    return;
  }

  this->binary_sensor_->add_on_state_callback([this](bool state) {
    if (state) {
      this->request_start_();
    } else {
      this->signal_stop_();
    }
  });
}

float PushToTalk::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

}  // namespace push_to_talk
}  // namespace esphome
