#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/components/cover/cover.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "tormatic_protocol.h"

namespace esphome::tormatic {

static constexpr float COVER_VENTILATION = 2.0f;

using namespace esphome::cover;

class Tormatic final : public cover::Cover, public uart::UARTDevice, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_open_duration(uint32_t duration) { this->open_duration_ = duration; }
  void set_close_duration(uint32_t duration) { this->close_duration_ = duration; }

  void ventilate() { this->send_gate_command_(VENTILATING); }

  void send_light_command(bool state);

  template<typename F> void add_on_light_state_callback(F &&callback) {
    this->light_state_callback_.add(std::forward<F>(callback));
  }

  void publish_state(bool save = true, uint32_t ratelimit = 0);

  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;

  void recalibrate_duration_(GateStatus s);
  void recompute_position_();
  void control_position_(float target);
  void stop_at_target_();

  template<typename T> void send_message_(MessageType t, T r);
  template<typename T> optional<T> read_data_();
  void drain_rx_(uint16_t n = 0);

  void request_gate_status_();
  void request_light_status_();

  optional<GateStatus> read_status_response_();

  void send_gate_command_(GateStatus s);
  void handle_gate_status_(GateStatus s);

  uint32_t seq_tx_{0};
  optional<MessageHeader> pending_hdr_{};
  optional<StatusType> pending_status_type_{};

  GateStatus current_status_{PAUSED};

  uint32_t open_duration_{0};
  uint32_t close_duration_{0};
  uint32_t last_publish_time_{0};
  uint32_t last_recompute_time_{0};
  uint32_t last_light_poll_time_{0};
  uint32_t direction_start_time_{0};
  GateStatus next_command_{OPENED};
  optional<float> target_position_{};

  LazyCallbackManager<void(bool)> light_state_callback_;
};

template<typename... Ts> class VentilateAction : public Action<Ts...> {
 public:
  explicit VentilateAction(Tormatic *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->ventilate(); }

 protected:
  Tormatic *parent_;
};

}  // namespace esphome::tormatic
