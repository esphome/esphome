#pragma once

#include <cstdint>

namespace esphome::fendt_caravan {

// Inline so each has one address: the link hands the pointer back and PanelControl::sends() compares it
inline constexpr char CMD_FLOOR_HEATER_TOGGLE[] = "cmd-tgl:FLOOR_HEATER_ON";
inline constexpr char CMD_HS_KEY[] = "net-HS_KEY";            // turns the unit on, or toggles all lights
inline constexpr char CMD_HS_KEY_LONG[] = "net-HS_KEY_LONG";  // turns the unit off

static constexpr uint32_t PRESS_SETTLE_MS = 3000;  // a sent press not reported back by then was ignored

/// One panel control (the HS key or the floor heater). States are the panel's own numbers; only one press is
/// out at a time, and the next is decided by the report that answers it. Methods returning a command want it
/// queued, and the result passed to on_queued().
class PanelControl {
 public:
  /// `toggle` is the command of a control that flips on every press; null for the HS key
  explicit PanelControl(const char *toggle) : toggle_(toggle) {}

  int8_t state() const { return this->state_; }
  /// What a new request builds on: the state asked for while a press is out, else the reported one
  int8_t current() const { return this->target_ >= 0 ? this->target_ : this->state_; }
  bool sends(const char *command) const {
    return this->toggle_ != nullptr ? command == this->toggle_ : command == CMD_HS_KEY || command == CMD_HS_KEY_LONG;
  }

  void reset() {
    this->state_ = -1;
    this->target_ = -1;
    this->press_ = Press::PRESS_NONE;
  }
  /// Forgets a sent press the panel has not reported back within PRESS_SETTLE_MS
  void expire(uint32_t now) {
    if (this->press_ == Press::PRESS_SENT && now - this->sent_ms_ >= PRESS_SETTLE_MS) {
      this->press_ = Press::PRESS_NONE;
      this->target_ = -1;
    }
  }
  const char *request(int8_t target) {
    if (target == this->current())
      return nullptr;
    this->target_ = target;
    return this->next_press_();
  }
  const char *on_report(int8_t state, uint32_t now) {
    this->state_ = state;
    // An expired press was ignored and is not sent again. A report while the press still waits in the queue
    // predates it, so only a sent press is answered here
    this->expire(now);
    if (this->press_ != Press::PRESS_SENT)
      return nullptr;
    this->press_ = Press::PRESS_NONE;
    return this->next_press_();
  }
  void on_queued(bool queued) {
    if (queued) {
      this->press_ = Press::PRESS_QUEUED;
    } else {
      this->target_ = -1;
    }
  }
  /// The link wrote the command, or gave up on it before the panel could have taken it
  void on_command_done(bool sent, uint32_t now) {
    if (this->press_ != Press::PRESS_QUEUED)
      return;
    if (!sent) {
      this->press_ = Press::PRESS_NONE;
      this->target_ = -1;
      return;
    }
    // Settling runs from here, not from the request, as earlier commands can hold the queue for seconds
    this->press_ = Press::PRESS_SENT;
    this->sent_ms_ = now;
  }

 protected:
  enum class Press : uint8_t { PRESS_NONE, PRESS_QUEUED, PRESS_SENT };

  const char *next_press_() {
    if (this->press_ != Press::PRESS_NONE)
      return nullptr;
    if (this->target_ < 0 || this->state_ < 0 || this->target_ == this->state_) {
      this->target_ = -1;
      return nullptr;
    }
    // A short key press takes the unit from off to on and toggles all lights while it runs; a long one turns
    // it off. From off to all lights takes two presses, the second sent once the first is reported.
    if (this->toggle_ != nullptr)
      return this->toggle_;
    return this->target_ == 0 ? CMD_HS_KEY_LONG : CMD_HS_KEY;
  }

  const char *toggle_;
  uint32_t sent_ms_{0};
  int8_t state_{-1};   // as reported, -1 until reported in this session
  int8_t target_{-1};  // the state asked for, -1 when nothing is asked; kept until the panel reports it
  Press press_{Press::PRESS_NONE};
};

}  // namespace esphome::fendt_caravan
