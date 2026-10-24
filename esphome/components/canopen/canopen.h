#pragma once

#include <array>
#include "esphome/components/canbus/canbus.h"

namespace esphome::canopen {

enum class SDOResult : uint8_t {
  SDO_RESULT_SUCCESS,
  SDO_RESULT_ABORT,
  SDO_RESULT_TIMEOUT,
  SDO_RESULT_TRANSPORT_ERROR,
  SDO_RESULT_UNSUPPORTED,
  SDO_RESULT_REJECTED,
};

using SDOCallback = void (*)(void *, SDOResult, uint32_t, uint8_t);

enum class NMTCommand : uint8_t {
  NMT_COMMAND_START = 1,
  NMT_COMMAND_STOP = 2,
  NMT_COMMAND_PRE_OPERATIONAL = 128,
  NMT_COMMAND_RESET_NODE = 129,
  NMT_COMMAND_RESET_COMMUNICATION = 130,
};

struct Emergency {
  uint16_t error_code;
  uint8_t error_register;
  std::array<uint8_t, 5> manufacturer_data;
};

class CANopenComponent : public Component {
 public:
  explicit CANopenComponent(canbus::Canbus *bus) : bus_(bus) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_response_timeout(uint32_t timeout) { this->response_timeout_ms_ = timeout; }
  bool send_nmt(uint8_t node, NMTCommand command);
  template<typename F> void add_on_heartbeat_callback(F &&callback) {
    this->heartbeat_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_emcy_callback(F &&callback) {
    this->emcy_callback_.add(std::forward<F>(callback));
  }
  // Accepted requests call back exactly once. Rejected requests return false without calling back.
  bool read_sdo(uint8_t node, uint16_t index, uint8_t subindex, SDOCallback callback, void *context);
  bool write_sdo(uint8_t node, uint16_t index, uint8_t subindex, uint32_t value, uint8_t size,
                 SDOCallback callback = nullptr, void *context = nullptr);

 protected:
  struct Request {
    uint32_t value;
    SDOCallback callback;
    void *context;
    uint16_t index;
    uint8_t node;
    uint8_t subindex;
    uint8_t size;  // Zero means upload (read); otherwise download (write).
  };
  bool enqueue_(const Request &request);
  void on_frame_(uint32_t id, bool extended, bool rtr, const std::vector<uint8_t> &data);
  void finish_(SDOResult result, uint32_t value = 0, uint8_t size = 0);

  static constexpr size_t QUEUE_SIZE = 16;
  canbus::Canbus *bus_;
  LazyCallbackManager<void(uint8_t, uint8_t)> heartbeat_callback_;
  LazyCallbackManager<void(uint8_t, const Emergency &)> emcy_callback_;
  std::array<Request, QUEUE_SIZE> requests_{};
  size_t head_{0};
  size_t count_{0};
  bool active_{false};
  uint32_t sent_at_ms_{0};
  uint32_t response_timeout_ms_{500};
};

}  // namespace esphome::canopen
