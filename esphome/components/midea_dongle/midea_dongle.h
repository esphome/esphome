#pragma once
#include <deque>
#include "esphome/core/component.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/uart/uart.h"
#include "midea_frame.h"

namespace esphome {
namespace midea_dongle {

enum MideaApplianceType : uint8_t { DEHUMIDIFIER = 0xA1, AIR_CONDITIONER = 0xAC, BROADCAST = 0xFF };
enum MideaMessageType : uint8_t {
  DEVICE_CONTROL = 0x02,
  DEVICE_QUERY = 0x03,
  GET_ELECTRONIC_ID = 0x07,
  NETWORK_NOTIFY = 0x0D,
  QUERY_NETWORK = 0x63,
};

struct MideaAppliance {
  /// Calling on requests.
  virtual void on_frame(const Frame &frame) = 0;
  /// Called when the queue is free and the message can be sent. Should be used to send very frequent messages.
  virtual void on_idle() = 0;
};

enum ResponseStatus : uint8_t {
  RESPONSE_OK,
  RESPONSE_PARTIAL,
  RESPONSE_WRONG,
};

using ResponseHandler = std::function<ResponseStatus(const Frame &)>;

struct MideaRequest {
  StaticFrame<Frame, 36> request;
  uint32_t attempts;
  uint32_t timeout;
  ResponseHandler handler;
  ResponseStatus call_handler(const Frame &frame);
};

class MideaDongle : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::BEFORE_API; }
  void setup() override;
  void loop() override;
  void set_appliance(MideaAppliance *app) { this->appliance_ = app; }
  void send_frame(const Frame &frame);
  void queue_request(const Frame &frame, uint32_t attempts, uint32_t timeout, ResponseHandler handler = nullptr);
  void queue_request_priority(const Frame &frame, uint32_t attempts, uint32_t timeout, ResponseHandler handler = nullptr);

 protected:
  MideaAppliance *appliance_{nullptr};
  std::deque<MideaRequest *> queue_;
  MideaRequest *request_{nullptr};
  FrameReceiver<256> receiver_;
  bool is_ready_{false};
  void handler_(const Frame &frame);
  void get_electronic_id_();
  void report_network_status_(uint8_t type);
  void setup_network_notify_task_();
  void send_network_notify_(uint8_t msg_type = NETWORK_NOTIFY);
  void destroy_request_();
  void update_timeout_();
  bool is_wait_for_response_() const { return this->request_ != nullptr; }
};

}  // namespace midea_dongle
}  // namespace esphome
