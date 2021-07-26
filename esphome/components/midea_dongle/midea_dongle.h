#pragma once
#include <deque>
#include "esphome/core/component.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/uart/uart.h"
#include "midea_frame.h"
#ifdef USE_REMOTE_TRANSMITTER
#include "esphome/components/remote_base/midea_protocol.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#endif

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
using ErrorHandler = std::function<void()>;

class MideaDongle : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_appliance(MideaAppliance *app) { this->appliance_ = app; }
  void send_frame(const Frame &frame);
  void queue_request(const Frame &frame, ResponseHandler handler = nullptr, ErrorHandler error_cb = nullptr);
  void queue_request_priority(const Frame &frame, ResponseHandler handler = nullptr, ErrorHandler error_cb = nullptr);
  void set_period(uint32_t ms) { this->period_ = ms; }
  void set_response_timeout(uint32_t ms) { this->response_timeout_ = ms; }
  void set_request_attempts(uint32_t attempts) { this->request_attempts_ = attempts; }
#ifdef USE_REMOTE_TRANSMITTER
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void transmit_ir(remote_base::MideaData &data);
#endif

 protected:
  struct Request {
    StaticFrame<Frame, 36> request;
    ResponseHandler handler;
    ErrorHandler error_cb;
    ResponseStatus call_handler(const Frame &frame);
  };
  void handler_(const Frame &frame);
  void send_network_notify_(uint8_t msg_type = NETWORK_NOTIFY);
  void destroy_request_();
  void reset_timeout_();
  void reset_attempts_() { this->remain_attempts_ = this->request_attempts_; }
  bool is_wait_for_response_() const { return this->request_ != nullptr; }

  std::deque<Request *> queue_;
  MideaAppliance *appliance_{nullptr};
  Request *request_{nullptr};
  uint32_t remain_attempts_{};
#ifdef USE_REMOTE_TRANSMITTER
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
#endif
  FrameReceiver<128> receiver_{};
  uint32_t period_{1000};
  uint32_t request_attempts_{5};
  uint32_t response_timeout_{2000};
  bool is_busy_{false};
};

}  // namespace midea_dongle
}  // namespace esphome
