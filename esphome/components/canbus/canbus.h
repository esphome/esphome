#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <vector>

namespace esphome::canbus {

enum Error : uint8_t {
  ERROR_OK = 0,
  ERROR_FAIL = 1,
  ERROR_ALLTXBUSY = 2,
  ERROR_FAILINIT = 3,
  ERROR_FAILTX = 4,
  ERROR_NOMSG = 5
};

enum CanSpeed : uint8_t {
  CAN_1KBPS,
  CAN_5KBPS,
  CAN_10KBPS,
  CAN_12K5BPS,
  CAN_16KBPS,
  CAN_20KBPS,
  CAN_25KBPS,
  CAN_31K25BPS,
  CAN_33KBPS,
  CAN_40KBPS,
  CAN_50KBPS,
  CAN_80KBPS,
  CAN_83K3BPS,
  CAN_95KBPS,
  CAN_100KBPS,
  CAN_125KBPS,
  CAN_200KBPS,
  CAN_250KBPS,
  CAN_500KBPS,
  CAN_800KBPS,
  CAN_1000KBPS,
  CAN_2000KBPS,
  CAN_3000KBPS,
  CAN_4000KBPS,
  CAN_5000KBPS,
  CAN_6000KBPS,
  CAN_7000KBPS,
  CAN_8000KBPS
};

template<typename... Ts> class CanbusSendAction;

/* CAN payload length definitions according to ISO 11898-1 */
static constexpr uint8_t CAN_MAX_DATA_LENGTH = 8;
static constexpr uint8_t CAN_FD_MAX_DATA_LENGTH = 64;

static constexpr uint8_t CAN_ERROR_PASSIVE_THRESHOLD = 128;

/* Returned by either length conversion when the input cannot be represented. */
static constexpr uint8_t CAN_INVALID_DLC = UINT8_MAX;

/* Decode using the CAN FD mapping. Classic CAN callers must cap valid results at eight bytes. */
constexpr uint8_t can_dlc_to_len(uint8_t dlc) {
  if (dlc <= CAN_MAX_DATA_LENGTH) {
    return dlc;
  }

  switch (dlc) {
    case 9:
      return 12;
    case 10:
      return 16;
    case 11:
      return 20;
    case 12:
      return 24;
    case 13:
      return 32;
    case 14:
      return 48;
    case 15:
      return 64;
    default:
      return CAN_INVALID_DLC;
  }
}

/* With pad, round up to the next CAN FD length. The caller must fill the extra payload bytes. */
constexpr uint8_t can_len_to_dlc(size_t len, bool pad) {
  if (len > CAN_FD_MAX_DATA_LENGTH) {
    return CAN_INVALID_DLC;
  }

  if (len <= CAN_MAX_DATA_LENGTH) {
    return static_cast<uint8_t>(len);
  }

  if (len <= 12)
    return (len == 12 || pad) ? 9 : CAN_INVALID_DLC;
  if (len <= 16)
    return (len == 16 || pad) ? 10 : CAN_INVALID_DLC;
  if (len <= 20)
    return (len == 20 || pad) ? 11 : CAN_INVALID_DLC;
  if (len <= 24)
    return (len == 24 || pad) ? 12 : CAN_INVALID_DLC;
  if (len <= 32)
    return (len == 32 || pad) ? 13 : CAN_INVALID_DLC;
  if (len <= 48)
    return (len == 48 || pad) ? 14 : CAN_INVALID_DLC;
  if (len <= 64)
    return (len == 64 || pad) ? 15 : CAN_INVALID_DLC;

  return CAN_INVALID_DLC;
}

/// Frame payload without heap; converts to and from std::vector<uint8_t> for older lambdas.
class CanPayload : public StaticVector<uint8_t, CAN_FD_MAX_DATA_LENGTH> {
 public:
  using StaticVector<uint8_t, CAN_FD_MAX_DATA_LENGTH>::StaticVector;
  // Not = default, so value-init does not zero the buffer
  CanPayload() {}
  CanPayload(const std::vector<uint8_t> &data)  // NOLINT(google-explicit-constructor)
      : StaticVector<uint8_t, CAN_FD_MAX_DATA_LENGTH>(data.begin(), data.end()) {}
  operator std::vector<uint8_t>() const { return {this->begin(), this->end()}; }  // NOLINT(google-explicit-constructor)
};

/*
Can Frame describes a normative CAN Frame
The RTR = Remote Transmission Request is implemented in every CAN controller but rarely used
So currently the flag is passed to and from the hardware but currently ignored to the user application.
*/
struct CanFrame {
  uint32_t can_id;              /* 29 or 11 bit CAN_ID; drivers clear bits 29-31 (on_frame relies on it) */
  uint8_t can_data_length_code; /* Wire DLC (0..15), not byte length. For RTR, the requested length. */

  bool use_extended_id : 1 = false;
  bool remote_transmission_request : 1 = false;
  bool fd : 1 = false; /* FD frames cannot use RTR. BRS and ESI are only meaningful for FD frames. */
  bool bit_rate_switch : 1 = false;
  bool error_state_indicator : 1 =
      false; /* Received transmitter error state. Transmit handling is controller-specific. */

  uint8_t data[CAN_FD_MAX_DATA_LENGTH] __attribute__((aligned(8)));

  /// Payload bytes for the DLC, at most 8 for a classic frame; 0 for an invalid DLC.
  uint8_t payload_length() const {
    const uint8_t len = can_dlc_to_len(this->can_data_length_code);
    if (len == CAN_INVALID_DLC)
      return 0;
    return this->fd ? len : std::min(len, CAN_MAX_DATA_LENGTH);
  }
  CanPayload payload() const {
    CanPayload out;
    const uint8_t len = this->payload_length();
    std::memcpy(out.data(), this->data, len);
    out.resize(len);
    return out;
  }
};

enum CanEventFlags : uint8_t {
  CAN_EVENT_PASSIVE = 1 << 0,
  CAN_EVENT_ACTIVE = 1 << 1,
  CAN_EVENT_BUS_OFF = 1 << 2,
  CAN_EVENT_RX_QUEUE_FULL = 1 << 3,
};

static constexpr uint32_t CAN_STATUS_UNSET = UINT32_MAX;

struct CanStatus {
  bool bus_off{false};
  uint8_t rx_error_counter{0};
  uint8_t tx_error_counter{0};
  uint32_t tx_failed_count{CAN_STATUS_UNSET};
  uint32_t rx_missed_count{CAN_STATUS_UNSET};
  uint32_t rx_overrun_count{CAN_STATUS_UNSET};
  uint32_t arb_lost_count{CAN_STATUS_UNSET};
  uint32_t bus_error_count{CAN_STATUS_UNSET};
};

class Canbus : public Component {
 public:
  Canbus() {};
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  canbus::Error send_data(CanFrame &frame);

  // Explicit frame format. pad_data permits zero-padding to the next representable FD length.
  canbus::Error send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request, const uint8_t *data,
                          size_t len, bool fd, bool bit_rate_switch, bool pad_data = false);
  canbus::Error send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request, const uint8_t *data,
                          size_t len) {
    return this->send_data(can_id, use_extended_id, remote_transmission_request, data, len, false, false);
  }
  canbus::Error send_data(uint32_t can_id, bool use_extended_id, bool remote_transmission_request,
                          const std::vector<uint8_t> &data) {
    return this->send_data(can_id, use_extended_id, remote_transmission_request, data.data(), data.size());
  }
  canbus::Error send_data(uint32_t can_id, bool use_extended_id, const std::vector<uint8_t> &data) {
    // for backwards compatibility only
    return this->send_data(can_id, use_extended_id, false, data);
  }
  void set_can_id(uint32_t can_id) { this->can_id_ = can_id; }
  void set_use_extended_id(bool use_extended_id) { this->use_extended_id_ = use_extended_id; }
  void set_bitrate(CanSpeed bit_rate) { this->bit_rate_ = bit_rate; }
  void set_max_frames_per_loop(uint32_t max_frames_per_loop) { this->max_frames_per_loop_ = max_frames_per_loop; }
  // Defaults apply to canbus.send actions, not the classic-only send_data() overloads or explicit CanFrames.
  void set_default_fd(bool default_fd) { this->default_fd_ = default_fd; }
  void set_default_bit_rate_switch(bool default_bit_rate_switch) {
    this->default_bit_rate_switch_ = default_bit_rate_switch;
  }
  void set_default_pad_data(bool default_pad_data) { this->default_pad_data_ = default_pad_data; }
  virtual bool supports_fd() const { return false; }
  virtual CanStatus get_status() { return {}; }

  /// The frame is only valid during the call.
  template<typename F> void add_frame_callback(F &&callback) { this->frame_callbacks_.add(std::forward<F>(callback)); }

  /**
   * Add a callback to be called when a CAN message is received. All received messages
   * are passed to the callback without filtering.
   *
   * The callback function receives:
   * - can_id of the received data
   * - extended_id True if the can_id is an extended id
   * - rtr If this is a remote transmission request
   * - data The message data
   */
  template<typename F>
  ESPDEPRECATED("Use add_frame_callback(), which does not copy the payload. Removed in 2027.5.0.", "2026.11.0")
  void add_callback(F &&callback) {
    this->frame_callbacks_.add([cb = std::forward<F>(callback)](const CanFrame &frame) mutable {
      cb(frame.can_id, frame.use_extended_id, frame.remote_transmission_request,
         std::vector<uint8_t>(frame.data, frame.data + frame.payload_length()));
    });
  }

 protected:
  template<typename... Ts> friend class CanbusSendAction;
  uint32_t can_id_;
  uint32_t max_frames_per_loop_{50};  // Keep in sync with DEFAULT_MAX_FRAMES_PER_LOOP in __init__.py
  CanSpeed bit_rate_{CAN_125KBPS};    // Keep in sync with DEFAULT_BIT_RATE in __init__.py
  bool use_extended_id_{false};
  bool default_fd_{false};
  bool default_bit_rate_switch_{false};
  bool default_pad_data_{false};
  uint8_t events_to_log_{0};
  bool bus_off_{false};
  CallbackManager<void(const CanFrame &frame)> frame_callbacks_{};

  // interval in which to check for new can bus events (bus-off, passive, active)
  static constexpr uint32_t EVENT_CHECK_INTERVAL_MS = 100;
  uint32_t last_event_check_time_{0};

  // time to wait after logging an event before logging the next event(s)
  static constexpr uint32_t EVENT_LOG_THROTTLE_MS = 1000;
  uint32_t last_event_log_time_{0};

  // time span without bus-off events before the bus is declared recovered
  static constexpr uint32_t EVENT_LOG_BUS_OFF_HOLDOFF_MS = 1000;
  uint32_t last_bus_off_time_{0};

#ifdef ESPHOME_LOG_HAS_VERBOSE
  static constexpr uint32_t STATE_LOG_INTERVAL_MS = 1000;
  uint32_t last_state_log_time_{0};
#endif

  void log_events_(CanEventFlags events, uint32_t now);
  virtual bool setup_internal() = 0;
  virtual Error send_message(struct CanFrame *frame) = 0;
  virtual Error read_message(struct CanFrame *frame) = 0;
  virtual CanEventFlags get_events() { return CanEventFlags{}; }
};

template<typename... Ts> class CanbusSendAction final : public Action<Ts...>, public Parented<Canbus> {
  TEMPLATABLE_BUFFER(data, CanPayload)
  TEMPLATABLE_VALUE(bool, fd)
  TEMPLATABLE_VALUE(bool, bit_rate_switch)
  TEMPLATABLE_VALUE(bool, pad_data)

  void set_can_id(uint32_t can_id) { this->can_id_ = can_id; }

  void set_use_extended_id(bool use_extended_id) { this->use_extended_id_ = use_extended_id; }

  void set_remote_transmission_request(bool remote_transmission_request) {
    this->remote_transmission_request_ = remote_transmission_request;
  }

  void play(const Ts &...x) override {
    auto can_id = this->can_id_.has_value() ? *this->can_id_ : this->parent_->can_id_;
    auto use_extended_id =
        this->use_extended_id_.has_value() ? *this->use_extended_id_ : this->parent_->use_extended_id_;
    const bool fd = this->fd_.value_or(x..., this->parent_->default_fd_);
    const bool bit_rate_switch = this->bit_rate_switch_.value_or(x..., fd && this->parent_->default_bit_rate_switch_);
    const bool pad_data = this->pad_data_.value_or(x..., this->parent_->default_pad_data_);
    this->data_.template visit<CAN_FD_MAX_DATA_LENGTH>(
        [&](const uint8_t *data, size_t len) {
          this->parent_->send_data(can_id, use_extended_id, this->remote_transmission_request_, data, len, fd,
                                   bit_rate_switch, pad_data);
        },
        x...);
  }

 protected:
  optional<uint32_t> can_id_{};
  optional<bool> use_extended_id_{};
  bool remote_transmission_request_{false};
};

}  // namespace esphome::canbus
