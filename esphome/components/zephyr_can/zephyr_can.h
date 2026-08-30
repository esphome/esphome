#pragma once
#ifdef USE_ZEPHYR

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>

namespace esphome::zephyr_can {

class ZephyrCan : public canbus::Canbus {
 public:
  ZephyrCan(const struct device *can_dev, struct k_msgq *rx_queue, uint32_t bitrate, can_mode_t mode)
      : can_dev_(can_dev), rx_queue_(rx_queue), bitrate_(bitrate), mode_(mode) {}

  void dump_config() override;
  void loop() override;

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

  /// Add one accept-all receive filter feeding rx_queue_. Returns false on failure.
  bool add_rx_filter_(bool extended_id);
  /// Log controller state and error counters, but only when they change.
  void log_bus_state_();

  const struct device *can_dev_;
  struct k_msgq *rx_queue_;
  uint32_t bitrate_;
  can_mode_t mode_;
  uint32_t last_state_check_{0};
  can_state last_state_{CAN_STATE_STOPPED};
  can_bus_err_cnt last_err_cnt_{0, 0};
};

}  // namespace esphome::zephyr_can

#endif  // USE_ZEPHYR
