#ifdef USE_ZEPHYR

#include "zephyr_can.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace esphome::zephyr_can {

static const char *const TAG = "zephyr_can";

// Controller state and error counters are only read this often -- they are polled from
// loop(), which runs far more frequently than a bus error can meaningfully change.
static const uint32_t STATE_POLL_INTERVAL_MS = 1000;

static const char *can_state_str(can_state state) {
  switch (state) {
    case CAN_STATE_ERROR_ACTIVE:
      return "error-active";
    case CAN_STATE_ERROR_WARNING:
      return "error-warning";
    case CAN_STATE_ERROR_PASSIVE:
      return "error-passive";
    case CAN_STATE_BUS_OFF:
      return "bus-off";
    case CAN_STATE_STOPPED:
      return "stopped";
    default:
      return "unknown";
  }
}

static const char *can_mode_str(can_mode_t mode) {
  if ((mode & CAN_MODE_LOOPBACK) != 0) {
    return "loopback";
  }
  if ((mode & CAN_MODE_LISTENONLY) != 0) {
    return "listen only";
  }
  return "normal";
}

// Passed to can_send() so it returns as soon as the frame is queued: without a callback
// the call blocks until the frame is actually put on the wire, which never happens while
// the bus is off.
static void tx_callback(const struct device *dev, int error, void *user_data) {
  if (error != 0) {
    ESP_LOGW(TAG, "sending frame failed [%d]", error);
  }
}

bool ZephyrCan::setup_internal() {
  if (!device_is_ready(this->can_dev_)) {
    ESP_LOGE(TAG, "CAN device is not ready");
    return false;
  }

  // Bitrate and mode can only be changed while the controller is stopped. A freshly
  // initialized controller already is, hence -EALREADY is not an error here.
  int err = can_stop(this->can_dev_);
  if (err != 0 && err != -EALREADY) {
    ESP_LOGE(TAG, "can_stop failed [%d]", err);
    return false;
  }

  err = can_set_bitrate(this->can_dev_, this->bitrate_);
  if (err != 0) {
    ESP_LOGE(TAG, "can't set bitrate to %" PRIu32 " bit/s [%d]", this->bitrate_, err);
    return false;
  }

  err = can_set_mode(this->can_dev_, this->mode_);
  if (err != 0) {
    ESP_LOGE(TAG, "can't set mode [%d]", err);
    return false;
  }

  // Standard and extended identifiers need one filter each -- a filter without
  // CAN_FILTER_IDE only ever matches standard frames.
  if (!this->add_rx_filter_(false) || !this->add_rx_filter_(true)) {
    return false;
  }

  err = can_start(this->can_dev_);
  if (err != 0) {
    ESP_LOGE(TAG, "can_start failed [%d]", err);
    return false;
  }

  return true;
}

bool ZephyrCan::add_rx_filter_(bool extended_id) {
  // Accept everything (mask 0) -- filtering by identifier is done by canbus triggers.
  const struct can_filter filter = {
      .id = 0,
      .mask = 0,
      .flags = static_cast<uint8_t>(extended_id ? CAN_FILTER_IDE : 0),
  };
  int filter_id = can_add_rx_filter_msgq(this->can_dev_, this->rx_queue_, &filter);
  if (filter_id < 0) {
    ESP_LOGE(TAG, "unable to add %s id receive filter [%d]", extended_id ? "extended" : "standard", filter_id);
    return false;
  }
  return true;
}

void ZephyrCan::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zephyr CAN:\n"
                "  Device: %s\n"
                "  Bit rate: %" PRIu32 " bit/s\n"
                "  Mode: %s",
                this->can_dev_->name, this->bitrate_, can_mode_str(this->mode_));
  canbus::Canbus::dump_config();
}

void ZephyrCan::loop() {
  canbus::Canbus::loop();
  this->log_bus_state_();
}

void ZephyrCan::log_bus_state_() {
  const uint32_t now = millis();
  if (now - this->last_state_check_ < STATE_POLL_INTERVAL_MS) {
    return;
  }
  this->last_state_check_ = now;

  enum can_state state;
  struct can_bus_err_cnt err_cnt;
  if (can_get_state(this->can_dev_, &state, &err_cnt) != 0) {
    ESP_LOGW(TAG, "can't read controller state");
    return;
  }
  if (state == this->last_state_ && err_cnt.rx_err_cnt == this->last_err_cnt_.rx_err_cnt &&
      err_cnt.tx_err_cnt == this->last_err_cnt_.tx_err_cnt) {
    return;
  }
  this->last_state_ = state;
  this->last_err_cnt_ = err_cnt;

  // The controller recovers from bus-off on its own, so this is a report, not an error.
  if (state == CAN_STATE_ERROR_ACTIVE) {
    ESP_LOGD(TAG, "state: %s, rx errors: %u, tx errors: %u", can_state_str(state), err_cnt.rx_err_cnt,
             err_cnt.tx_err_cnt);
  } else {
    ESP_LOGW(TAG, "state: %s, rx errors: %u, tx errors: %u", can_state_str(state), err_cnt.rx_err_cnt,
             err_cnt.tx_err_cnt);
  }
}

canbus::Error ZephyrCan::send_message(struct canbus::CanFrame *frame) {
  struct can_frame tx_frame = {
      .id = frame->can_id,
      .dlc = frame->can_data_length_code,
      .flags = static_cast<uint8_t>((frame->use_extended_id ? CAN_FRAME_IDE : 0) |
                                    (frame->remote_transmission_request ? CAN_FRAME_RTR : 0)),
  };
  if (frame->can_data_length_code != 0) {
    memcpy(tx_frame.data, frame->data, frame->can_data_length_code);
  }
  // K_NO_WAIT: waiting here for a free transmit mailbox would stall the whole loop.
  int err = can_send(this->can_dev_, &tx_frame, K_NO_WAIT, tx_callback, nullptr);
  if (err != 0) {
    // Canbus::send_data() already warns about the failure; this adds the reason.
    ESP_LOGD(TAG, "can_send failed [%d]", err);
    return err == -EAGAIN ? canbus::ERROR_ALLTXBUSY : canbus::ERROR_FAILTX;
  }
  return canbus::ERROR_OK;
}

canbus::Error ZephyrCan::read_message(struct canbus::CanFrame *frame) {
  struct can_frame rx_frame;
  if (k_msgq_get(this->rx_queue_, &rx_frame, K_NO_WAIT) != 0) {
    return canbus::ERROR_NOMSG;
  }
  const uint8_t length = std::min<uint8_t>(rx_frame.dlc, canbus::CAN_MAX_DATA_LENGTH);
  frame->can_id = rx_frame.id;
  frame->can_data_length_code = length;
  frame->use_extended_id = (rx_frame.flags & CAN_FRAME_IDE) != 0;
  frame->remote_transmission_request = (rx_frame.flags & CAN_FRAME_RTR) != 0;
  if (length != 0) {
    memcpy(frame->data, rx_frame.data, length);
  }
  return canbus::ERROR_OK;
}

}  // namespace esphome::zephyr_can

#endif  // USE_ZEPHYR
