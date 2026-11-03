#pragma once

namespace esphome::sx126x {

enum class SX126xError {
  NONE = 0,
  TIMEOUT,
  INVALID_PARAMS,
  SX126X_ERROR_NOT_READY,
  SX126X_ERROR_BUSY_TIMEOUT,
  SX126X_ERROR_BUSY
};

// Owns readiness across whole radio operations, not individual SPI exchanges.
// Callbacks run synchronously and are never stored. No hardware I/O occurs in destructors.
class RadioOperation {
 public:
  RadioOperation() = default;
  RadioOperation(const RadioOperation &) = delete;
  RadioOperation &operator=(const RadioOperation &) = delete;

  bool is_ready() const { return this->state_ == State::STATE_IDLE; }
  bool is_active() const {
    return this->state_ == State::STATE_CONFIGURING || this->state_ == State::STATE_CHANGING_MODE ||
           this->is_transmitting();
  }
  bool is_transmitting() const {
    return this->state_ == State::STATE_PREPARING_TX || this->state_ == State::STATE_TRANSMITTING ||
           this->state_ == State::STATE_RESTORING;
  }
  SX126xError transmit_admission() const {
    if (this->is_ready()) {
      return SX126xError::NONE;
    }
    return this->is_active() ? SX126xError::SX126X_ERROR_BUSY : SX126xError::SX126X_ERROR_NOT_READY;
  }

  template<typename Apply> [[nodiscard]] bool configure(Apply &&apply) {
    if (this->is_active()) {
      return false;
    }
    Attempt attempt(*this, State::STATE_CONFIGURING, State::STATE_UNCONFIGURED);
    const bool success = apply();
    attempt.finish(success ? State::STATE_IDLE : State::STATE_UNCONFIGURED);
    return success;
  }

  template<typename Command> [[nodiscard]] bool change_mode(Command &&command, bool retains_configuration = true) {
    if (!this->is_ready()) {
      return false;
    }
    Attempt attempt(*this, State::STATE_CHANGING_MODE);
    const bool success = command();
    attempt.finish(success ? (retains_configuration ? State::STATE_IDLE : State::STATE_UNCONFIGURED)
                           : State::STATE_RECOVERY_REQUIRED);
    return success;
  }

  // Every return from prepare/wait comes back here before readiness can be restored.
  // A failed preparation still requires restoration; a failed restoration overrides the TX result.
  template<typename Prepare, typename Wait, typename Restore>
  [[nodiscard]] SX126xError transmit(Prepare &&prepare, Wait &&wait, Restore &&restore) {
    const auto admission = this->transmit_admission();
    if (admission != SX126xError::NONE) {
      return admission;
    }
    Attempt attempt(*this, State::STATE_PREPARING_TX);
    auto result = SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
    if (prepare()) {
      this->state_ = State::STATE_TRANSMITTING;
      result = wait();
    }
    this->state_ = State::STATE_RESTORING;
    const bool restored = restore();
    attempt.finish(restored ? State::STATE_IDLE : State::STATE_RECOVERY_REQUIRED);
    return restored ? result : SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
  }

 private:
  enum class State {
    STATE_UNCONFIGURED,
    STATE_CONFIGURING,
    STATE_IDLE,
    STATE_CHANGING_MODE,
    STATE_PREPARING_TX,
    STATE_TRANSMITTING,
    STATE_RESTORING,
    STATE_RECOVERY_REQUIRED
  };

  // If the owner itself gains an unexpected early exit, it fails closed.
  // Fallible hardware restoration belongs in transmit(), where its result is reported.
  class Attempt {
   public:
    Attempt(RadioOperation &owner, State phase, State failure = State::STATE_RECOVERY_REQUIRED)
        : owner_(owner), failure_(failure) {
      this->owner_.state_ = phase;
    }
    ~Attempt() {
      if (!this->finished_) {
        this->owner_.state_ = this->failure_;
      }
    }
    Attempt(const Attempt &) = delete;
    Attempt &operator=(const Attempt &) = delete;
    void finish(State state) {
      this->owner_.state_ = state;
      this->finished_ = true;
    }

   private:
    RadioOperation &owner_;
    State failure_;
    bool finished_{false};
  };
  State state_{State::STATE_UNCONFIGURED};
};

}  // namespace esphome::sx126x
