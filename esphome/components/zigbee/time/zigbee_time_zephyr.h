#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_TIME)
#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/zigbee/zigbee_zephyr.h"

extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

namespace esphome::zigbee {

class ZigbeeTime : public time::RealTimeClock, public ZigbeeEntity {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_cluster_attributes(zb_zcl_time_attrs_t &cluster_attributes) {
    this->cluster_attributes_ = &cluster_attributes;
  }

  void set_epoch_time(uint32_t epoch);

 protected:
  static void sync_time(zb_ret_t status, zb_uint32_t auth_level, zb_uint16_t short_addr, zb_uint8_t endpoint,
                        zb_uint32_t nw_time);
  void zcl_device_cb_(zb_bufid_t bufid);
  zb_zcl_time_attrs_t *cluster_attributes_{nullptr};

  bool has_time_{false};
};

}  // namespace esphome::zigbee

#endif
