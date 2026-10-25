#pragma once
#ifdef USE_ZEPHYR

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

namespace esphome::zephyr_can {

class ZephyrCan : public canbus::Canbus {
 public:
  ZephyrCan(const struct device *can_dev, struct k_msgq *rx_queue, uint32_t bitrate, can_mode_t mode)
      : can_dev_(can_dev), rx_queue_(rx_queue), bitrate_(bitrate), mode_(mode) {}

#ifdef USE_ZEPHYR_CAN_TX_QUEUE
  void set_tx_queue(struct k_msgq *tx_queue) { this->tx_queue_ = tx_queue; }
#endif

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
#ifdef USE_ZEPHYR_CAN_TX_QUEUE
  /// Start sending queued frames unless a frame is already with the controller.
  void kick_tx_();
  /// Hand the next queued frame to the controller. Only called by the owner of tx_busy_.
  void send_next_();
  static void tx_work_handler_(struct k_work *work);
#endif
  static void tx_done_(const struct device *dev, int error, void *user_data);

  const struct device *can_dev_;
  struct k_msgq *rx_queue_;
#ifdef USE_ZEPHYR_CAN_TX_QUEUE
  struct k_msgq *tx_queue_{nullptr};
  /// Runs send_next_() in thread context: the driver's send path can't be used from the TX done ISR.
  /// Wrapped so CONTAINER_OF works on a standard-layout type (ZephyrCan has virtual methods).
  struct TxWork {
    struct k_work work;
    ZephyrCan *parent;
  } tx_work_{};
  /// Set while a frame is with the controller or a refill is pending.
  atomic_t tx_busy_{ATOMIC_INIT(0)};
#endif
  /// Frames dropped after send_message() returned (can_send or transmission failed), reported from loop().
  atomic_t tx_errors_{ATOMIC_INIT(0)};
  atomic_t tx_last_error_{ATOMIC_INIT(0)};
  atomic_val_t reported_tx_errors_{0};
  uint32_t bitrate_;
  can_mode_t mode_;
  uint32_t last_state_check_{0};
  can_state last_state_{CAN_STATE_STOPPED};
  can_bus_err_cnt last_err_cnt_{0, 0};
};

}  // namespace esphome::zephyr_can

#endif  // USE_ZEPHYR
