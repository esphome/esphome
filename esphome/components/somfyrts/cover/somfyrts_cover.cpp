#include "somfyrts_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace somfyrts {

static const char *const TAG = "somfyrts.cover";

using namespace esphome::uart;

void SomfyRTSCover::dump_config() {
  LOG_COVER("", "somfyrts Cover", this);
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  ESP_LOGCONFIG(TAG, "  NodeID: 0x%02X 0x%02X 0x%02X", this->node_id_1_, this->node_id_2_, this->node_id_3_);
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  RTS Control Pin: %d", this->crtl_pin_);
}
void SomfyRTSCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
  }
  if (this->crtl_pin_ != nullptr)
    this->crtl_pin_->digital_write(false);
}
void SomfyRTSCover::loop() {
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
float SomfyRTSCover::get_setup_priority() const { return setup_priority::DATA; }
esphome::cover::CoverTraits SomfyRTSCover::get_traits() {
  auto traits = esphome::cover::CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  traits.set_supports_tilt(true);
  return traits;
}
void SomfyRTSCover::control(const esphome::cover::CoverCall &call) {
  if (call.get_stop()) {
    this->start_direction_(esphome::cover::COVER_OPERATION_IDLE);
    this->publish_state();
  }
  if (call.get_tilt()) {
    if (call.get_tilt() == 1) {
        uint8_t frame[14] = {0x7E, 0xF1, 0xFA, 0x01, 0x00, 0x00, ~this->node_id_3_, ~this->node_id_2_, ~this->node_id_1_, ~this->channel_, 0xFF, 0xFE, 0x00, 0x00};
        this->calc_chksum_(frame, 14);
        if (this->crtl_pin_ != nullptr)
          this->crtl_pin_->digital_write(true);
        this->write_array(frame, sizeof(frame));
        if (this->crtl_pin_ != nullptr) 
          this->crtl_pin_->digital_write(false);
      } else {
        uint8_t frame[14] = {0x7E, 0xF1, 0xFA, 0x01, 0x00, 0x00, ~this->node_id_3_, ~this->node_id_2_, ~this->node_id_1_, ~this->channel_, 0xFE, 0xFE, 0x00, 0x00};
        this->calc_chksum_(frame, 14);
        if (this->crtl_pin_ != nullptr)
          this->crtl_pin_->digital_write(true);
        this->write_array(frame, sizeof(frame));
        if (this->crtl_pin_ != nullptr) 
          this->crtl_pin_->digital_write(false);
      }
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
bool SomfyRTSCover::is_at_target_() const {
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
void SomfyRTSCover::start_direction_(esphome::cover::CoverOperation dir) {
  if (dir == this->current_operation && dir != esphome::cover::COVER_OPERATION_IDLE)
    return;

  this->recompute_position_();
  uint8_t frame[13] = {0x7F, 0xF2, 0xFA, 0x01, 0x00, 0x00, ~this->node_id_3_, ~this->node_id_2_, ~this->node_id_1_, ~this->channel_, 0x00, 0x00, 0x00};
  switch (dir) {
    case esphome::cover::COVER_OPERATION_IDLE:
      frame[10] = 0xFC;
      break;
    case esphome::cover::COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      frame[10] = 0xFE;
      if(this->isinverted) frame[10] = 0xFD;
      break;
    case esphome::cover::COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      frame[10] = 0xFD;
      if(this->isinverted) frame[10] = 0xFE;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  const uint32_t now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
  this->calc_chksum_(frame, 13);
  if (this->crtl_pin_ != nullptr)
    this->crtl_pin_->digital_write(true);
  this->write_array(frame, sizeof(frame));
  if (this->crtl_pin_ != nullptr) 
    this->crtl_pin_->digital_write(false);
}
void SomfyRTSCover::calc_chksum_(uint8_t *frame, uint8_t frame_size){
  uint16_t chksum = 0x00;
  for(uint8_t i = 0; i < frame_size-2; i++){
    chksum+=frame[i];
  }
  frame[frame_size-2] = chksum>>8;
  frame[frame_size-1] = (chksum<<24)>>24;
}

void SomfyRTSCover::set_channel(uint8_t channel) { this->channel_ = channel; }
void SomfyRTSCover::set_node_id(uint8_t nodeid_1, uint8_t nodeid_2, uint8_t nodeid_3){ this->node_id_1_ = nodeid_1; this->node_id_2_ = nodeid_2; this->node_id_3_ = nodeid_3;}

void SomfyRTSCover::recompute_position_() {
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

}  // namespace somfyrts
}  // namespace esphome
