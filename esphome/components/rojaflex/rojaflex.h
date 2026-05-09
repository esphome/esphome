#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/cc1101/cc1101.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "rojaflex_protocol.h"

namespace esphome::rojaflex {

class RojaflexDevice;
class RojaflexCover;

class RojaflexComponent : public Component, public cc1101::CC1101Listener {
 public:
  void setup() override;
  void dump_config() override;

  void on_packet(const std::vector<uint8_t> &packet, float freq_offset, float rssi, uint8_t lqi) override;

  void set_transceiver(cc1101::CC1101Component *transceiver) { this->transceiver_ = transceiver; }
  void set_housecode(const std::string &housecode) { this->housecode_ = housecode; }
  void set_tx_repetitions(uint8_t repetitions) { this->tx_repetitions_ = repetitions; }

  void register_device(RojaflexDevice *device);
  void register_cover(RojaflexCover *cover, uint8_t channel);

  bool send_command(uint8_t channel_id, uint8_t cmd_code, bool optimistic_motor_pct_update = true);
  void set_position(uint8_t channel_id, int target_pct);
  bool set_housecode_manually(const std::string &housecode);

  int get_motor_pct(uint8_t channel_id) const;
  int get_cal_time_open_s(uint8_t channel_id) const;
  int get_cal_time_close_s(uint8_t channel_id) const;
  std::string get_channel_status(uint8_t channel_id) const;
  std::string get_housecode() const { return this->housecode_; }
  std::string get_last_rx_raw() const { return this->last_rx_raw_; }
  std::string get_last_rx_info() const { return this->last_rx_info_; }
  bool get_last_tx_ok() const { return this->cc1101_last_tx_ok_; }
  int get_last_tx_error() const { return this->cc1101_last_error_; }
  uint8_t get_tx_repetitions() const { return this->tx_repetitions_; }
  void set_tx_repetitions_runtime(uint8_t repetitions) {
    if (repetitions >= 1 && repetitions <= 9) {
      this->tx_repetitions_ = repetitions;
    }
  }
  std::string get_device_status() const;

 protected:
  static constexpr uint32_t SELF_TX_ECHO_GUARD_MS = 200;
  static constexpr uint32_t CALIBRATION_CAPTURE_WINDOW_MS = 60000;
  static constexpr int CALIBRATION_MIN_TIME_S = 5;

  std::string mid_position_timer_name_(uint8_t channel_id) const;
  void tick_position_interpolation_();
  void cancel_position_interpolation_(uint8_t channel_id);
  void arm_position_interpolation_(uint8_t channel_id, int start_pct, int end_pct, uint32_t duration_ms);
  void cancel_mid_position_timers_(uint8_t channel_id, bool all_channels);
  void apply_pct_to_motor_(uint8_t channel_id, int pct, bool all_channels);
  bool try_resolve_calibration_(uint8_t channel_id, int reported_pct);
  void cancel_calibration_capture_(uint8_t channel_id);
  void arm_calibration_capture_(uint8_t channel_id, int target_pct);
  void refresh_covers_();

  cc1101::CC1101Component *transceiver_{nullptr};
  std::vector<RojaflexDevice *> devices_;
  std::vector<RojaflexCover *> covers_ = std::vector<RojaflexCover *>(16, nullptr);

  std::string housecode_{"0000000"};
  std::string auto_learn_housecode_;
  uint32_t auto_learn_count_{0};
  uint8_t tx_repetitions_{2};

  std::vector<int> motor_pct_ = std::vector<int>(16, -1);
  std::string last_rx_raw_{"-"};
  std::string last_rx_info_{"-"};
  bool cc1101_last_tx_ok_{false};
  int cc1101_last_error_{0};

  uint32_t last_self_tx_ms_{0};

  std::vector<int> cal_time_open_s_ = std::vector<int>(16, -1);
  std::vector<int> cal_time_close_s_ = std::vector<int>(16, -1);
  std::vector<uint32_t> cal_pending_tx_ms_ = std::vector<uint32_t>(16, 0);
  std::vector<int> cal_pending_target_pct_ = std::vector<int>(16, -1);

  std::vector<uint32_t> interp_start_ms_ = std::vector<uint32_t>(16, 0);
  std::vector<int> interp_start_pct_ = std::vector<int>(16, -1);
  std::vector<int> interp_end_pct_ = std::vector<int>(16, -1);
  std::vector<uint32_t> interp_duration_ms_ = std::vector<uint32_t>(16, 0);
};

class RojaflexDevice {
 public:
  void set_parent(RojaflexComponent *parent) { this->parent_ = parent; }

 protected:
  RojaflexComponent *parent_{nullptr};
};

template<typename... Ts> class SetHousecodeAction : public Action<Ts...>, public Parented<RojaflexComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, housecode)
  void play(const Ts &...x) override { this->parent_->set_housecode_manually(this->housecode_.value(x...)); }
};

template<typename... Ts> class SendCommandAction : public Action<Ts...>, public Parented<RojaflexComponent> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(uint8_t, command)
  void play(const Ts &...x) override { this->parent_->send_command(this->channel_.value(x...), this->command_.value(x...)); }
};

template<typename... Ts> class SetPositionAction : public Action<Ts...>, public Parented<RojaflexComponent> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(int, target)
  void play(const Ts &...x) override { this->parent_->set_position(this->channel_.value(x...), this->target_.value(x...)); }
};

}  // namespace esphome::rojaflex
