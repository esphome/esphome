#pragma once

#ifdef USE_ESP32

#include <array>
#include <atomic>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "pd.h"

namespace esphome::fusb302b {

/// FUSB302B USB Type-C / Power Delivery sink controller.
///
/// A dedicated task owns all I2C traffic to the chip so that PD replies meet the protocol timing
/// even while the main loop is busy. The main loop only publishes state changes.
class FUSB302B : public PowerDelivery, public Component, public i2c::I2CDevice {
 public:
  explicit FUSB302B(InternalGPIOPin *interrupt_pin) : interrupt_pin_(interrupt_pin) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  /// Negotiate a new contract with the highest source voltage that does not exceed `voltage`.
  /// Values outside 5-20 V are clamped with a warning.
  void request_voltage(int voltage);

  /// Callbacks receive each new state in order, with the state before it.
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }
  /// Called when a voltage request was rejected, got no answer, or was dropped because the component failed.
  /// A contract already in place stays valid.
  template<typename F> void add_on_request_failed_callback(F &&callback) {
    this->request_failed_callback_.add(std::forward<F>(callback));
  }

#ifdef USE_SENSOR
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sensor_ = sensor; }
  void set_current_sensor(sensor::Sensor *sensor) { this->current_sensor_ = sensor; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_contract_text_sensor(text_sensor::TextSensor *sensor) { this->contract_text_sensor_ = sensor; }
#endif

 protected:
  enum class SourceCapStep : uint8_t {
    SOURCE_CAP_STEP_WAIT,
    SOURCE_CAP_STEP_GET_SENT,
    SOURCE_CAP_STEP_SOFT_RESET_SENT,
  };

  static void gpio_intr(FUSB302B *arg);
  static void task_func(void *arg);

  bool send_message(const PdMsg &msg) override;
  void on_state_changed() override;
  void on_soft_reset_received() override { this->start_source_cap_wait_(SourceCapStep::SOURCE_CAP_STEP_WAIT); }
  void on_source_caps_received() override { this->waiting_for_source_caps_ = false; }
  void on_transition_timeout() override { this->send_soft_reset_(); }
  void on_request_failed() override;

  // PHY task only
  [[noreturn]] void run_task_();
  bool init_chip_();
  bool reset_pd_();
  bool enable_auto_crc_();
  void handle_interrupt_();
  bool read_message_(PdMsg &msg, bool &is_sop);
  void try_attach_();
  bool measure_cc_(uint8_t meas_switch, uint8_t &level);
  void detach_();
  void handle_hard_reset_();
  void start_source_cap_wait_(SourceCapStep step);
  void start_negotiation_();
  void send_soft_reset_();
  void check_source_caps_();
  void enter_error_();

  void publish_contract_(PackedContract contract);
  void publish_state_(PdState state);
  void queue_event_(uint8_t event);

  InternalGPIOPin *interrupt_pin_;
  StaticTask task_;

  // PHY task only
  uint32_t source_cap_step_start_{0};
  SourceCapStep source_cap_step_{SourceCapStep::SOURCE_CAP_STEP_WAIT};
  bool attached_{false};
  bool waiting_for_source_caps_{false};
  bool cc_ambiguous_logged_{false};
  bool request_pending_{false};
  // Last state put in the queue
  PdState queued_state_{PdState::PD_STATE_DISCONNECTED};

  // State changes and request failures from the PHY task, replayed in order by loop() so no event is lost while the
  // main loop is blocked. An entry is a PdState value or EVENT_REQUEST_FAILED. The PHY task only writes head, the
  // main loop only writes tail.
  static constexpr uint8_t EVENT_QUEUE_SIZE = 8;
  static constexpr uint8_t EVENT_REQUEST_FAILED = 0xFF;
  std::array<uint8_t, EVENT_QUEUE_SIZE> event_queue_{};
  std::atomic<uint8_t> event_queue_head_{0};
  std::atomic<uint8_t> event_queue_tail_{0};
  std::atomic<bool> event_queue_overflow_{false};
  // Request failures that did not fit in the queue
  std::atomic<uint8_t> dropped_request_failures_{0};

  // Main loop only
  PdState published_state_{PdState::PD_STATE_DISCONNECTED};
  uint32_t published_contract_{UINT32_MAX};

  LazyCallbackManager<void(PdState, PdState)> state_callback_;
  LazyCallbackManager<void()> request_failed_callback_;
#ifdef USE_SENSOR
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *contract_text_sensor_{nullptr};
#endif
};

}  // namespace esphome::fusb302b

#endif  // USE_ESP32
