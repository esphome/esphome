#include "canbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace canbus {

static const char *TAG = "canbus";

void Canbus::setup() { ESP_LOGCONFIG(TAG, "Setting up Canbus..."); }

void Canbus::dump_config() { ESP_LOGCONFIG(TAG, "Canbus: sender_id=%d", this->sender_id_); }

void Canbus::send(int can_id, uint8_t *data) {
  ESP_LOGD(TAG, "send: sender_id=%d, id=%d,  data=%d", this->sender_id_, can_id, data[0]);
  this->send_internal_(can_id, data);
};

void Canbus::loop() {
    //check harware inputbuffer and process to esphome outputs
}

}  // namespace canbus
}  // namespace esphome