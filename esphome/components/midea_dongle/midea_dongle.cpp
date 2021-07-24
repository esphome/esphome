#include "midea_dongle.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midea_dongle {

static const char *const TAG = "midea_dongle";
static const std::string SEND_TIMEOUT = "midea_dongle: send_timeout";
static const std::string RESPONSE_TIMEOUT = "midea_dongle: response_timeout";

ResponseStatus MideaRequest::call_handler(const Frame &frame) {
  if (!frame.has_type(this->request.get_type()))
    return ResponseStatus::RESPONSE_WRONG;
  if (this->handler == nullptr)
    return RESPONSE_OK;
  return this->handler(frame);
}

void MideaDongle::setup() {
  this->get_electronic_id_();
  this->setup_network_notify_task_();
  this->set_timeout(SEND_TIMEOUT, 5000, [this](){
    this->is_ready_ = true;
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

void MideaDongle::get_electronic_id_() {
  uint8_t data[] = {0xAA, 0x0B, 0xFF, 0xF4, 0x00, 0x00, 0x01, 0x00, 0x00, GET_ELECTRONIC_ID, 0x00, 0xFA};
  this->queue_request(data, 5, 2000, [this](const Frame &frame) -> ResponseStatus {
    ESP_LOGI(TAG, "Detected appliance type: 0x%.2X", frame.app_type());
    return RESPONSE_OK;
  });
}

void MideaDongle::setup_network_notify_task_() {
  static const std::string NETWORK_NOTIFY_TASK = "midea_dongle: network_notify_task";
  this->set_interval(NETWORK_NOTIFY_TASK, 2*60*1000, [this](){
    this->send_network_notify_();
  });
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
  auto connected = WiFi.isConnected();
  auto strength = get_signal_strength();
  auto ip = WiFi.localIP();
  notify.set_type(msg_type);
  notify.set_connected(connected);
  notify.set_signal_strength(connected ? strength : 0);
  notify.set_ip(ip);
  notify.update_cs();
  ESP_LOGD(TAG, "Send network notify: WiFi STA is %s, signal strength: %d, ip: %s",
    connected ? "connected" : "not connected", strength,
    ip.toString().c_str()
  );
  if (msg_type == NETWORK_NOTIFY)
    this->queue_request(notify, 5, 2000);
  else
    this->send_frame(notify);
}

void MideaDongle::queue_request(const Frame &request, uint32_t attempts, uint32_t timeout, ResponseHandler handler) {
  ESP_LOGD(TAG, "Request push_back to queue. Attempts: %d, response timeout: %dms", attempts, timeout);
  this->queue_.push_back(new MideaRequest{request, attempts, timeout, handler});
}

void MideaDongle::queue_request_priority(const Frame &request, uint32_t attempts, uint32_t timeout, ResponseHandler handler) {
  ESP_LOGD(TAG, "Request push_front to queue. Attempts: %d, response timeout: %dms", attempts, timeout);
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

  // other requests send to appliance
  if (this->appliance_ != nullptr)
    this->appliance_->on_frame(frame);
}

void MideaDongle::send_frame(const Frame &frame) {
  ESP_LOGD(TAG, "TX: %s", frame.to_string().c_str());
  this->write_array(frame.data(), frame.size());
  this->is_ready_ = false;
  this->set_timeout(SEND_TIMEOUT, 1000, [this](){
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

}  // namespace midea_dongle
}  // namespace esphome
