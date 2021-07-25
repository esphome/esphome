#include "midea_dongle.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midea_dongle {

static const char *const TAG = "midea_dongle";
static const std::string RESPONSE_TIMEOUT = "midea_dongle: response_timeout";

ResponseStatus MideaRequest::call_handler(const Frame &frame) {
  if (!frame.has_type(this->request.get_type()))
    return ResponseStatus::RESPONSE_WRONG;
  if (this->handler == nullptr)
    return RESPONSE_OK;
  return this->handler(frame);
}

void MideaDongle::setup() {
  this->set_interval(2*60*1000, [this](){
    this->send_network_notify_();
  });
}

void MideaDongle::loop() {
  while (this->receiver_.read(this)) {
    ESP_LOGD(TAG, "RX: %s", this->receiver_.to_string().c_str());
    this->handler_(this->receiver_);
  }
  if (!this->is_ready_ || this->is_wait_for_response_())
    return;
  if (this->queue_.empty()) {
    if (this->appliance_ != nullptr)
      this->appliance_->on_idle();
    return;
  }
  this->request_ = this->queue_.front();
  this->queue_.pop_front();
  ESP_LOGD(TAG, "Getting and sending a request from the queue...");
  this->send_frame(this->request_->request);
  this->update_timeout_();
}

static uint8_t get_signal_strength() {
  const int32_t dbm = WiFi.RSSI();
  if (dbm > -63)
    return 4;
  if (dbm > -75)
    return 3;
  if (dbm > -88)
    return 2;
  return 1;
}

void MideaDongle::send_network_notify_(uint8_t msg_type) {
  NotifyFrame notify;
  notify.set_type(msg_type);
  notify.set_connected(WiFi.isConnected());
  notify.set_signal_strength(get_signal_strength());
  notify.set_ip(WiFi.localIP());
  notify.update_cs();
  if (msg_type == NETWORK_NOTIFY) {
    ESP_LOGD(TAG, "Enqueuing a DEVICE_NETWORK(0x0D) notification...");
    this->queue_request(notify, 5, 2000);
  } else {
    ESP_LOGD(TAG, "Answer to QUERY_NETWORK(0x63) request...");
    this->send_frame(notify);
  }
}

void MideaDongle::queue_request(const Frame &request, uint32_t attempts, uint32_t timeout, ResponseHandler handler) {
  ESP_LOGD(TAG, "Enqueuing the request. Attempts: %d, response timeout: %dms", attempts, timeout);
  this->queue_.push_back(new MideaRequest{request, attempts, timeout, handler});
}

void MideaDongle::queue_request_priority(const Frame &request, uint32_t attempts, uint32_t timeout, ResponseHandler handler) {
  ESP_LOGD(TAG, "Priority request queuing. Attempts: %d, response timeout: %dms", attempts, timeout);
  this->queue_.push_front(new MideaRequest{request, attempts, timeout, handler});
}

void MideaDongle::handler_(const Frame &frame) {
  if (this->is_wait_for_response_()) {
    auto result = this->request_->call_handler(frame);
    if (result != RESPONSE_WRONG) {
      if (result == RESPONSE_OK)
        this->destroy_request_();
      else
        this->update_timeout_();
      return;
    }
  }
  /* HANDLE REQUESTS */
  if (frame.has_type(QUERY_NETWORK)) {
    this->send_network_notify_(QUERY_NETWORK);
    return;
  }
  // Other requests send to appliance
  if (this->appliance_ != nullptr)
    this->appliance_->on_frame(frame);
}

void MideaDongle::send_frame(const Frame &frame) {
  ESP_LOGD(TAG, "TX: %s", frame.to_string().c_str());
  this->write_array(frame.data(), frame.size());
  this->is_ready_ = false;
  this->set_timeout(this->period_, [this](){
    this->is_ready_ = true;
  });
}

void MideaDongle::update_timeout_() {
  this->set_timeout(RESPONSE_TIMEOUT, this->request_->timeout, [this](){
    ESP_LOGD(TAG, "Response timeout. Attempts left: %d", this->request_->attempts - 1);
    if (!--this->request_->attempts) {
      this->destroy_request_();
      return;
    }
    ESP_LOGD(TAG, "Trying to send the request again...");
    this->send_frame(this->request_->request);
    this->update_timeout_();
  });
}

void MideaDongle::destroy_request_() {
  ESP_LOGD(TAG, "Destroying the request...");
  delete this->request_;
  this->request_ = nullptr;
  this->cancel_timeout(RESPONSE_TIMEOUT);
}

void MideaDongle::dump_config() {
  ESP_LOGCONFIG(TAG, "Using period: %dms", this->period_);
}

#ifdef USE_REMOTE_TRANSMITTER
void MideaDongle::transmit_ir(remote_base::MideaData &data) {
  data.finalize();
  ESP_LOGD(TAG, "Sending Midea IR data: %s", data.to_string().c_str());
  auto transmit = this->transmitter_->transmit();
  remote_base::MideaProtocol().encode(transmit.get_data(), data);
  transmit.perform();
}
#endif

}  // namespace midea_dongle
}  // namespace esphome
