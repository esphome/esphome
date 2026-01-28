#include "zigbee_time_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_TIME)
#include "esphome/core/log.h"

namespace esphome::zigbee {

static const char *const TAG = "zigbee.time";

// This time standard is the number of
// seconds since 0 hrs 0 mins 0 sec on 1st January 2000 UTC (Universal Coordinated Time).
constexpr time_t EPOCH_2000 = 946684800;

ZigbeeTime *global_time = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void ZigbeeTime::sync_time(zb_ret_t status, zb_uint32_t auth_level, zb_uint16_t short_addr, zb_uint8_t endpoint,
                           zb_uint32_t nw_time) {
  if (status == RET_OK && auth_level >= ZB_ZCL_TIME_HAS_SYNCHRONIZED_BIT) {
    global_time->set_epoch_time(nw_time + EPOCH_2000);
  } else if (status != RET_TIMEOUT || !global_time->has_time_) {
    ESP_LOGE(TAG, "Status: %d, auth_level: %u, short_addr: %d, endpoint: %d, nw_time: %u", status, auth_level,
             short_addr, endpoint, nw_time);
  }
}

void ZigbeeTime::setup() {
  global_time = this;
  this->parent_->add_callback(this->endpoint_, [this](zb_bufid_t bufid) { this->zcl_device_cb_(bufid); });
  synchronize_epoch_(EPOCH_2000);
  this->parent_->add_join_callback([this]() { zb_zcl_time_server_synchronize(this->endpoint_, sync_time); });
}

void ZigbeeTime::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee Time\n"
                "  Endpoint: %d",
                this->endpoint_);
  RealTimeClock::dump_config();
}

void ZigbeeTime::update() {
  time_t time = timestamp_now();
  this->cluster_attributes_->time = time - EPOCH_2000;
}

void ZigbeeTime::set_epoch_time(uint32_t epoch) {
  this->defer([this, epoch]() {
    this->synchronize_epoch_(epoch);
    this->has_time_ = true;
  });
}

void ZigbeeTime::zcl_device_cb_(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *p_device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  zb_zcl_device_callback_id_t device_cb_id = p_device_cb_param->device_cb_id;
  zb_uint16_t cluster_id = p_device_cb_param->cb_param.set_attr_value_param.cluster_id;
  zb_uint16_t attr_id = p_device_cb_param->cb_param.set_attr_value_param.attr_id;

  switch (device_cb_id) {
    /* ZCL set attribute value */
    case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
      if (cluster_id == ZB_ZCL_CLUSTER_ID_TIME) {
        if (attr_id == ZB_ZCL_ATTR_TIME_TIME_ID) {
          zb_uint32_t value = p_device_cb_param->cb_param.set_attr_value_param.values.data32;
          ESP_LOGI(TAG, "Synchronize time to %u", value);
          this->defer([this, value]() { synchronize_epoch_(value + EPOCH_2000); });
        } else if (attr_id == ZB_ZCL_ATTR_TIME_TIME_STATUS_ID) {
          zb_uint8_t value = p_device_cb_param->cb_param.set_attr_value_param.values.data8;
          ESP_LOGI(TAG, "Time status %hd", value);
          this->defer([this, value]() { this->has_time_ = ZB_ZCL_TIME_TIME_STATUS_SYNCHRONIZED_BIT_IS_SET(value); });
        }
      } else {
        /* other clusters attribute handled here */
        ESP_LOGI(TAG, "Unhandled cluster attribute id: %d", cluster_id);
        p_device_cb_param->status = RET_NOT_IMPLEMENTED;
      }
      break;
    default:
      p_device_cb_param->status = RET_NOT_IMPLEMENTED;
      break;
  }

  ESP_LOGD(TAG, "Zcl_device_cb_ status: %hd", p_device_cb_param->status);
}

}  // namespace esphome::zigbee

#endif
