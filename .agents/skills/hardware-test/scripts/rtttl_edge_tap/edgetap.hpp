#pragma once
// Pass-through speaker that measures each note's onset and release level. A note is a run of samples between
// runs of at least 8 zero samples; rtttl writes zeros for rests.
#include "esphome/components/speaker/speaker.h"
#include "esphome/core/log.h"
#include <cstdlib>

class EdgeTap : public esphome::speaker::Speaker {
 public:
  explicit EdgeTap(esphome::speaker::Speaker *out) : out_(out) {}
  size_t play(const uint8_t *data, size_t length) override { return this->tap_(data, this->out_->play(data, length)); }
  size_t play(const uint8_t *data, size_t length, TickType_t ticks) override {
    return this->tap_(data, this->out_->play(data, length, ticks));
  }
  void start() override {
    this->out_->set_audio_stream_info(this->audio_stream_info_);
    this->out_->start();
  }
  void stop() override { this->out_->stop(); }
  void finish() override { this->out_->finish(); }
  bool has_buffered_data() const override { return this->out_->has_buffered_data(); }
  void set_volume(float volume) override {
    this->volume_ = volume;
    this->out_->set_volume(volume);
  }
  void sync() {
    this->state_ = this->out_->is_running()   ? esphome::speaker::STATE_RUNNING
                   : this->out_->is_stopped() ? esphome::speaker::STATE_STOPPED
                                              : esphome::speaker::STATE_STARTING;
  }
  void report() {
    if (this->notes_ == 0)
      return;
    ESP_LOGI("edge", "notes=%u onset: mean %.1f%% max %.1f%% | release: mean %.1f%% max %.1f%% | biggest jump %d",
             this->notes_, 100.0f * this->on_sum_ / this->notes_, 100.0f * this->on_max_,
             100.0f * this->off_sum_ / this->notes_, 100.0f * this->off_max_, this->max_jump_);
    this->notes_ = 0;
    this->on_sum_ = this->off_sum_ = this->on_max_ = this->off_max_ = 0;
    this->max_jump_ = 0;
  }

 protected:
  static constexpr int EDGE = 4;
  size_t tap_(const uint8_t *data, size_t sent) {
    const int16_t *s = reinterpret_cast<const int16_t *>(data);
    for (size_t i = 0; i < sent / 2; i++)
      this->sample_(s[i]);
    return sent;
  }
  void sample_(int16_t v) {
    int jump = std::abs(v - this->prev_);
    this->prev_ = v;
    if (v == 0) {
      if (++this->zeros_ == 8 && this->in_note_)
        this->end_note_();
      return;
    }
    if (!this->in_note_ && this->zeros_ >= 8) {
      this->in_note_ = true;
      this->count_ = 0;
      this->peak_ = 0;
      this->onset_ = 0;
    }
    this->zeros_ = 0;
    if (!this->in_note_)
      return;
    if (jump > this->max_jump_)
      this->max_jump_ = jump;
    int a = std::abs(v);
    if (this->count_ < EDGE && a > this->onset_)
      this->onset_ = a;
    if (a > this->peak_)
      this->peak_ = a;
    this->tail_[this->count_ % EDGE] = a;
    this->count_++;
  }
  void end_note_() {
    this->in_note_ = false;
    if (this->count_ < 64 || this->peak_ == 0)
      return;
    int release = 0;
    for (int t : this->tail_)
      release = std::max(release, t);
    float on = float(this->onset_) / this->peak_, off = float(release) / this->peak_;
    this->on_sum_ += on;
    this->off_sum_ += off;
    this->on_max_ = std::max(this->on_max_, on);
    this->off_max_ = std::max(this->off_max_, off);
    this->notes_++;
  }
  esphome::speaker::Speaker *out_;
  int16_t prev_{0};
  uint32_t zeros_{8}, count_{0}, notes_{0};
  bool in_note_{false};
  int peak_{0}, onset_{0}, tail_[EDGE]{}, max_jump_{0};
  float on_sum_{0}, off_sum_{0}, on_max_{0}, off_max_{0};
};

inline EdgeTap *&edge_tap() {
  static EdgeTap *tap = nullptr;
  return tap;
}
