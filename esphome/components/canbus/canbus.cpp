#include "canbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace canbus {

static const char *TAG = "canbus";

void Canbus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Canbus...");
  if (!this->setup_internal_()) {
    ESP_LOGE(TAG, "Canbus setup error!");
    this->mark_failed();
  }
}

void Canbus::dump_config() { ESP_LOGCONFIG(TAG, "Canbus: sender_id=%d", this->sender_id_); }

void Canbus::send(int can_id, uint8_t *data) {
  int size = (sizeof data/ sizeof *data);
  ESP_LOGD(TAG, "send: sender_id=%d, can_id=%d,  data=%d data_size=%d", this->sender_id_, can_id, data[0],size);
  //this->send_internal_(can_id, data);
};

void Canbus::loop() {
  // check harware inputbuffer and process to esphome outputs
}

CanCall &CanCall::set_data(optional<float> data) {
  this->float_data_ = data;
  return *this;
}
CanCall &CanCall::set_data(float data) {
  this->float_data_ = data;
  return *this;
}

void CanCall::perform() {
  ESP_LOGD(TAG,"parent_id=%d can_id= %d data=%f",this->parent_->sender_id_,this->can_id_,this->float_data_);
  uint8_t *p = reinterpret_cast<uint8_t *>(&this->float_data_);
  //here we start the canbus->send
  this->parent_->send(this->can_id_,p);
}

}  // namespace canbus
}  // namespace esphome