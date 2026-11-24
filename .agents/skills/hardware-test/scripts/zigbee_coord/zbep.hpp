#pragma once
// nRF Zigbee checks without a coordinator (esphome: includes:). zbdump() logs every registered endpoint with its
// clusters; inject(ep, cluster, value) delivers a present value write the way the stack does for a coordinator write
// (on the ZBOSS thread, status logged; -31 means no handler); readpv(ep) logs the present value the stack holds.
#include <zboss_api.h>
#include "esphome/core/log.h"
#include <cstring>
inline void zbdump() {
  int n = 0;
  for (int ep = 1; ep <= 240; ep++) {
    zb_af_endpoint_desc_t *d = zb_af_get_endpoint_desc(ep);
    if (d == nullptr || d->simple_desc == nullptr)
      continue;
    n++;
    auto *sd = d->simple_desc;
    char buf[160];
    int pos = 0;
    int total = sd->app_input_cluster_count + sd->app_output_cluster_count;
    for (int i = 0; i < total && pos < (int) sizeof(buf) - 8; i++)
      pos += snprintf(buf + pos, sizeof(buf) - pos, "%s%04X", i == sd->app_input_cluster_count ? " | out " : " ",
                      sd->app_cluster_list[i]);
    ESP_LOGI("zbdump", "ep %3d (desc ep %3d) profile 0x%04X device 0x%04X in %u out %u clusters:%s", ep, sd->endpoint,
             sd->app_profile_id, sd->app_device_id, sd->app_input_cluster_count, sd->app_output_cluster_count, buf);
  }
  ESP_LOGI("zbdump", "%d endpoints registered", n);
}
extern "C" zb_ret_t zigbee_schedule_callback(zb_callback_t func, zb_uint8_t param);
static uint8_t inj_ep;
static uint16_t inj_cluster;
static uint32_t inj_val;
static void inj_buf_cb(zb_bufid_t bufid) {
  auto *p = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  memset(p, 0, sizeof(*p));
  p->device_cb_id = ZB_ZCL_SET_ATTR_VALUE_CB_ID;
  p->endpoint = inj_ep;
  p->cb_param.set_attr_value_param.cluster_id = inj_cluster;
  p->cb_param.set_attr_value_param.attr_id = 0x0055;  // present value
  if (inj_cluster == ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT)
    p->cb_param.set_attr_value_param.values.data8 = (uint8_t) inj_val;
  else
    p->cb_param.set_attr_value_param.values.data32 = inj_val;
  ZCL_CTX().device_cb(bufid);
  ESP_LOGI("inject", "write ep %u cluster 0x%04X -> status %d", inj_ep, inj_cluster, p->status);
  zb_buf_free(bufid);
}
static void inj_sched(zb_uint8_t) { zb_buf_get_out_delayed(inj_buf_cb); }
inline void inject(uint8_t ep, uint16_t cluster, uint32_t val) {
  inj_ep = ep;
  inj_cluster = cluster;
  inj_val = val;
  zigbee_schedule_callback(inj_sched, 0);
}
inline uint32_t fbits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}
// present value of an input or output cluster on an endpoint, as the stack holds it
inline void readpv(uint8_t ep) {
  static constexpr struct {
    uint16_t cluster;
    bool is_float;
    const char *name;
  } PV[] = {{ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, true, "analog input"},
            {ZB_ZCL_CLUSTER_ID_BINARY_INPUT, false, "binary input"},
            {ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, false, "binary output"},
            {ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, true, "analog output"}};
  for (const auto &c : PV) {
    zb_zcl_attr_t *a = zb_zcl_get_attr_desc_a(ep, c.cluster, ZB_ZCL_CLUSTER_SERVER_ROLE, 0x0055);
    if (a == nullptr)
      continue;
    if (c.is_float) {
      float f;
      memcpy(&f, a->data_p, 4);
      ESP_LOGI("readpv", "ep %u %s present value %.1f", ep, c.name, f);
    } else {
      ESP_LOGI("readpv", "ep %u %s present value %u", ep, c.name, *(uint8_t *) a->data_p);
    }
    return;
  }
}
