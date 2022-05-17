#include "somfycover_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SomfyRemote.h>

#define CC1101_FREQUENCY 433.42
#define EMITTER_GPIO 4

namespace esphome {
namespace somfycover {
  

SomfyRemote *somfyremote_;
static const char *const TAG = "SomfyCover.cover";


void SomfyCover::dump_config() {
  LOG_COVER("", "ArduinoSomfy Cover", this);
  ESP_LOGCONFIG(TAG, "  RemoteID: 0x%06x", this->remote_id_);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
}

void SomfyCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
  }
  pinMode(EMITTER_GPIO, OUTPUT);
  digitalWrite(EMITTER_GPIO, LOW);
  //ELECHOUSE_cc1101.setSpiPin(14, 12, 13, 15);
  ELECHOUSE_cc1101.setSpiPin(14, 12, 2, 15);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(CC1101_FREQUENCY);
  SomfyRemote* somfyremote_ = new SomfyRemote(EMITTER_GPIO, this->remote_id_);
  ELECHOUSE_cc1101.SetTx();
}
void SomfyCover::loop() {
  if (this->current_operation == esphome::cover::COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->is_at_target_()) {
    if (this->target_position_ == esphome::cover::COVER_OPEN || this->target_position_ == esphome::cover::COVER_CLOSED) {
      // Don't trigger stop, let the cover stop by itself.
      this->current_operation = esphome::cover::COVER_OPERATION_IDLE;
    } else {
      this->start_direction_(esphome::cover::COVER_OPERATION_IDLE);
    }
    this->publish_state();
  }

  // Send current position every second
  if (now - this->last_publish_time_ > 1000) {
    this->publish_state(false);
    this->last_publish_time_ = now;
  }
}
float SomfyCover::get_setup_priority() const { return setup_priority::DATA; }
esphome::cover::CoverTraits SomfyCover::get_traits() {
  auto traits = esphome::cover::CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  traits.set_supports_tilt(false);
  return traits;
}
void SomfyCover::control(const esphome::cover::CoverCall &call) {
  if (call.get_stop()) {
    this->start_direction_(esphome::cover::COVER_OPERATION_IDLE);
    this->publish_state();
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != esphome::cover::COVER_OPERATION_IDLE) {
      this->start_direction_(esphome::cover::COVER_OPERATION_IDLE);
      this->publish_state();
    } else {
      if (this->position == esphome::cover::COVER_CLOSED || this->last_operation_ == esphome::cover::COVER_OPERATION_CLOSING) {
        this->target_position_ = esphome::cover::COVER_OPEN;
        this->start_direction_(esphome::cover::COVER_OPERATION_OPENING);
      } else {
        this->target_position_ = esphome::cover::COVER_CLOSED;
        this->start_direction_(esphome::cover::COVER_OPERATION_CLOSING);
      }
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (pos == this->position) {
      // already at target
      // for covers with built in end stop, we should send the command again
      if (pos == esphome::cover::COVER_OPEN || pos == esphome::cover::COVER_CLOSED) {
        auto op = pos == esphome::cover::COVER_CLOSED ? esphome::cover::COVER_OPERATION_CLOSING : esphome::cover::COVER_OPERATION_OPENING;
        this->target_position_ = pos;
        this->start_direction_(op);
      }
    } else {
      auto op = pos < this->position ? esphome::cover::COVER_OPERATION_CLOSING : esphome::cover::COVER_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}
bool SomfyCover::is_at_target_() const {
  switch (this->current_operation) {
    case esphome::cover::COVER_OPERATION_OPENING:
      return this->position >= this->target_position_;
    case esphome::cover::COVER_OPERATION_CLOSING:
      return this->position <= this->target_position_;
    case esphome::cover::COVER_OPERATION_IDLE:
    default:
      return true;
  }
}
void SomfyCover::start_direction_(esphome::cover::CoverOperation dir) {
  if (dir == this->current_operation && dir != esphome::cover::COVER_OPERATION_IDLE)
    return;

  this->recompute_position_();
  Command command = Command::My;
  switch (dir) {
    case esphome::cover::COVER_OPERATION_IDLE:
      command = Command::My;
      break;
    case esphome::cover::COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      command = Command::Up;
      break;
    case esphome::cover::COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      command = Command::Down;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
  Serial.print("Rolling code: ");
  uint16_t newcode = id(this->rollingCodeStorage_);
  // Serial.printf("%u\n", (unsigned int)newcode);
  somfyremote_->sendCommand(command, this->remote_id_, newcode);
  id(this->rollingCodeStorage_)+=1;
  //this->send_command_(command);
}

void SomfyCover::set_remote_id(uint32_t remote_id) { this->remote_id_ = remote_id; }

// void SomfyCover::send_command_(Command command){
//       //ESP_LOGD(TAG, "Enviando comando: *2*%d*%s##", command, canal.c_str());
//       SomfyRemote somfyRemote(EMITTER_GPIO, this->remote_id_, rollingCodeStorage_);
//       ELECHOUSE_cc1101.SetTx();
//       somfyRemote.sendCommand(command);
//       ELECHOUSE_cc1101.setSidle();
//     }



void SomfyCover::recompute_position_() {
  if (this->current_operation == esphome::cover::COVER_OPERATION_IDLE)
    return;

  float dir;
  float action_dur;
  switch (this->current_operation) {
    case esphome::cover::COVER_OPERATION_OPENING:
      dir = 1.0f;
      action_dur = this->open_duration_;
      break;
    case esphome::cover::COVER_OPERATION_CLOSING:
      dir = -1.0f;
      action_dur = this->close_duration_;
      break;
    default:
      return;
  }

  const uint32_t now = millis();
  this->position += dir * (now - this->last_recompute_time_) / action_dur;
  this->position = clamp(this->position, 0.0f, 1.0f);

  this->last_recompute_time_ = now;
}

}  // namespace somfycover
}  // namespace esphome
