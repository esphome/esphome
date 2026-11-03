#pragma once

namespace esphome::sx126x {

enum class SX126xError {
  NONE = 0,
  TIMEOUT,
  INVALID_PARAMS,
  SX126X_ERROR_NOT_READY,
  SX126X_ERROR_BUSY_TIMEOUT,
  SX126X_ERROR_BUSY,
  SX126X_ERROR_CANCELLED
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
           this->is_transmitting() || this->has_pending_receive();
  }
  bool is_transmitting() const {
    return this->state_ == State::STATE_PREPARING_TX || this->state_ == State::STATE_TRANSMITTING ||
           this->state_ == State::STATE_RESTORING;
  }
  bool has_pending_receive() const { return this->state_ == State::STATE_RECEIVE_PENDING; }
  // Explicit reconfiguration may discard held RX, including after repeated read failures.
  bool can_configure() const { return !this->is_active() || this->has_pending_receive(); }
  SX126xError transmit_admission() const {
    if (this->is_ready()) {
      return SX126xError::NONE;
    }
    return this->is_active() ? SX126xError::SX126X_ERROR_BUSY : SX126xError::SX126X_ERROR_NOT_READY;
  }

  template<typename Apply> [[nodiscard]] bool configure(Apply &&apply) {
    if (!this->can_configure()) {
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

  // The blocking runner and asynchronous continuation share admission and restoration.
  template<typename Prepare, typename Wait, typename Restore>
  [[nodiscard]] SX126xError transmit(Prepare &&prepare, Wait &&wait, Restore &&restore) {
    const auto admission = this->start_transmit(prepare, restore);
    if (admission != SX126xError::NONE) {
      return admission;
    }
    Attempt waiting(*this, State::STATE_TRANSMITTING);
    const auto result = this->finish_transmit(wait(), restore);
    waiting.release();
    return result;
  }

  // Preparation returns NONE, BUSY for RX held in standby, or a command failure.
  // Pending RX retains ownership until finish_receive(); failures restore immediately.
  template<typename Prepare, typename Restore>
  [[nodiscard]] SX126xError start_transmit(Prepare &&prepare, Restore &&restore) {
    const auto admission = this->transmit_admission();
    if (admission != SX126xError::NONE) {
      return admission;
    }
    Attempt attempt(*this, State::STATE_PREPARING_TX);
    const SX126xError prepared = prepare();
    if (prepared == SX126xError::SX126X_ERROR_BUSY) {
      attempt.finish(State::STATE_RECEIVE_PENDING);
      return prepared;
    }
    if (prepared != SX126xError::NONE) {
      return this->restore_(prepared, restore, attempt);
    }
    attempt.finish(State::STATE_TRANSMITTING);
    return SX126xError::NONE;
  }

  bool is_waiting_for_transmit() const { return this->state_ == State::STATE_TRANSMITTING; }

  // The caller must consume held RX before restoration can clear IRQs or resume reception.
  template<typename Restore> [[nodiscard]] SX126xError finish_receive(Restore &&restore) {
    if (!this->has_pending_receive()) {
      return SX126xError::SX126X_ERROR_NOT_READY;
    }
    Attempt attempt(*this, State::STATE_RESTORING);
    return this->restore_(SX126xError::NONE, restore, attempt);
  }

  template<typename Restore> [[nodiscard]] SX126xError finish_transmit(SX126xError result, Restore &&restore) {
    if (!this->is_waiting_for_transmit()) {
      return SX126xError::SX126X_ERROR_NOT_READY;
    }
    Attempt attempt(*this, State::STATE_RESTORING);
    return this->restore_(result, restore, attempt);
  }

 private:
  enum class State {
    STATE_UNCONFIGURED,
    STATE_CONFIGURING,
    STATE_IDLE,
    STATE_CHANGING_MODE,
    STATE_PREPARING_TX,
    STATE_RECEIVE_PENDING,
    STATE_TRANSMITTING,
    STATE_RESTORING,
    STATE_RECOVERY_REQUIRED
  };

  // If the owner itself gains an unexpected early exit, it fails closed.
  // Fallible hardware restoration belongs in restore_(), where its result is reported.
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
      this->release();
    }
    void release() { this->finished_ = true; }

   private:
    RadioOperation &owner_;
    State failure_;
    bool finished_{false};
  };
  template<typename Restore> SX126xError restore_(SX126xError result, Restore &&restore, Attempt &attempt) {
    this->state_ = State::STATE_RESTORING;
    const bool restored = restore();
    attempt.finish(restored ? State::STATE_IDLE : State::STATE_RECOVERY_REQUIRED);
    return restored ? result : SX126xError::SX126X_ERROR_BUSY_TIMEOUT;
  }
  State state_{State::STATE_UNCONFIGURED};
};

}  // namespace esphome::sx126x
