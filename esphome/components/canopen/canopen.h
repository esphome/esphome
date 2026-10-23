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
};

using SDOCallback = void (*)(void *, SDOResult, uint32_t, uint8_t);

class CANopenComponent : public Component {
 public:
  explicit CANopenComponent(canbus::Canbus *bus) : bus_(bus) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_response_timeout(uint32_t timeout) { this->response_timeout_ms_ = timeout; }
  bool read_sdo(uint8_t node, uint16_t index, uint8_t subindex, SDOCallback callback, void *context);
  bool write_sdo(uint8_t node, uint16_t index, uint8_t subindex, uint32_t value, uint8_t size);

 protected:
  struct Request {
    uint8_t node;
    uint16_t index;
    uint8_t subindex;
    uint32_t value;
    uint8_t size;  // Zero means upload (read); otherwise download (write).
    SDOCallback callback;
    void *context;
  };
  bool enqueue_(const Request &request);
  void on_frame_(uint32_t id, bool extended, bool rtr, const std::vector<uint8_t> &data);
  void finish_(SDOResult result, uint32_t value = 0, uint8_t size = 0);

  static constexpr size_t QUEUE_SIZE = 16;
  canbus::Canbus *bus_;
  std::array<Request, QUEUE_SIZE> requests_{};
  size_t head_{0};
  size_t count_{0};
  bool active_{false};
  uint32_t sent_at_ms_{0};
  uint32_t response_timeout_ms_{500};
};

}  // namespace esphome::canopen
