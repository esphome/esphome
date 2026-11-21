#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/components/cover/cover.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "tormatic_protocol.h"

namespace esphome::tormatic {

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

#ifdef USE_SWITCH
  void set_light_switch(switch_::Switch *light_switch) { this->light_switch_ = light_switch; }
#endif

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

  uint16_t seq_tx_{0};
  optional<MessageHeader> pending_hdr_{};
  optional<uint16_t> pending_gate_seq_{};
  optional<uint16_t> pending_light_seq_{};

  GateStatus current_status_{PAUSED};

  uint32_t open_duration_{0};
  uint32_t close_duration_{0};
  uint32_t last_publish_time_{0};
  uint32_t last_recompute_time_{0};
  uint32_t last_light_poll_time_{0};
  uint32_t direction_start_time_{0};
  GateStatus next_command_{OPENED};
  optional<float> target_position_{};
  bool position_known_{true};
#ifdef USE_SWITCH
  switch_::Switch *light_switch_{nullptr};
#endif
};

template<typename... Ts> class VentilateAction : public Action<Ts...> {
 public:
  explicit VentilateAction(Tormatic *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->ventilate(); }

 protected:
  Tormatic *parent_;
};

}  // namespace esphome::tormatic
