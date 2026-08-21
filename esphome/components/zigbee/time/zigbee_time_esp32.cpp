#include "zigbee_time_esp32.h"
#if defined(USE_ZIGBEE) && defined(USE_ESP32) && defined(USE_TIME)
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::zigbee {

static const char *const TAG = "zigbee.time";

// This time standard is the number of
// seconds since 0 hrs 0 mins 0 sec on 1st January 2000 UTC (Universal Coordinated Time).
constexpr time_t EPOCH_2000 = 946684800;

ZigbeeTime *global_time = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void ZigbeeTime::setup() {
  global_time = this;
  this->parent_->add_on_start_callback([this]() { this->register_zb_time(); });
}

void ZigbeeTime::register_zb_time() {
  ezb_zcl_time_interface_t time_interface = {
      .get_utc_time = this->get_utc_time,
      .set_utc_time = this->set_utc_time,
  };
  ezb_err_t ret;
  ret = ezb_zcl_time_server_interface_register(this->endpoint_, time_interface);
  if (ret != EZB_ERR_NONE) {
    ESP_LOGW(TAG, "Setup failed: %u", ret);
    this->mark_failed();
    return;
  }
  this->registered_ = true;
  this->parent_->add_on_join_callback([this](bool x) { this->update(); });
  if (this->parent_->is_joined()) {
    this->update();
  }
}

void ZigbeeTime::cb(ezb_err_t status) {
  if (status == EZB_ERR_NONE) {
    ESP_LOGV(TAG, "Time synchronization successful");
  } else if (status == EZB_ERR_TIMEOUT) {
    ESP_LOGW(TAG, "Time synchronization timed out");
  } else {
    ESP_LOGW(TAG, "Time synchronization failed with error: %d", status);
  }
}

void ZigbeeTime::update() {
  if (this->parent_->is_joined() && this->registered_) {
    if (esp_zigbee_lock_acquire(20 / portTICK_PERIOD_MS)) {
      ESP_LOGV(TAG, "Updating time sync from Zigbee network...");
      ezb_zcl_time_server_synchronize_time(this->endpoint_, 10, this->cb, EZB_ZCL_TIME_SERVER_RANK_MASTER);
      esp_zigbee_lock_release();
    } else {
      ESP_LOGW(TAG, "Could not acquire Zigbee lock to synchronize time, will retry...");
      this->defer([this]() { this->update(); });
    }
  } else {
    ESP_LOGD(TAG, "Not connected to Zigbee network, cannot synchronize time");
  }
}

uint32_t ZigbeeTime::get_utc_time() { return (uint32_t) (global_time->timestamp_now() - EPOCH_2000); }

void ZigbeeTime::set_utc_time(uint32_t utc) { global_time->set_epoch_time(utc + EPOCH_2000); }

void ZigbeeTime::set_epoch_time(uint32_t utc) {
  // called from zigbee task, defer to main loop
  this->defer([this, utc]() {
    ESP_LOGV(TAG, "Setting device time to UTC: %u", utc);
    this->synchronize_epoch_(utc);
  });
  App.wake_loop_threadsafe();
}

void ZigbeeTime::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee Time\n"
                "  Endpoint: %d",
                this->endpoint_);
  RealTimeClock::dump_config();
}

}  // namespace esphome::zigbee

#endif
