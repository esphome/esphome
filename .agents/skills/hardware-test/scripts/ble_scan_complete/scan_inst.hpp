#pragma once
// esphome#22770 instrumentation: follows one scan's duration timer through Bluedroid.
// Link with -Wl,--wrap= for BTM_BleScan, btu_start_timer, btm_ble_timeout, osi_alarm_new, btc_transfer_context.
#include <cstdint>
#include <esp_gap_ble_api.h>
#include <esp_timer.h>

extern "C" {
struct scan_inst_t {
  volatile uint32_t ble_scan_start;      // BTM_BleScan(TRUE) calls
  volatile uint32_t ble_scan_start_bad;  // ... that did not return BTM_CMD_STARTED
  volatile uint32_t ble_scan_stop;       // BTM_BleScan(FALSE) calls
  volatile uint32_t timer_start;         // btu_start_timer(BTU_TTYPE_BLE_SCAN)
  volatile uint32_t alarm_fire;          // esp_timer fired for the scan timer entry (btc thread)
  volatile uint32_t ble_timeout;         // btm_ble_timeout for the scan timer entry (btu thread)
  volatile uint32_t inq_cmpl_post;       // INQ_CMPL handed to btc_transfer_context
  volatile uint32_t inq_cmpl_post_fail;  // ... that failed
  volatile uint8_t pid;
  void *volatile scan_tle;
  volatile int64_t last_timer_start_us;
  volatile int64_t last_fire_us;
  volatile int64_t last_timeout_us;
  volatile int64_t last_inq_cmpl_us;
};
scan_inst_t g_scan_inst{};

typedef void (*scan_inst_cb_t)(void *);
// the scan timer entry's original alarm callback (btu_general_alarm_cb)
static scan_inst_cb_t s_orig_cb = nullptr;

static void scan_inst_shim(void *data) {
  g_scan_inst.alarm_fire++;
  g_scan_inst.last_fire_us = esp_timer_get_time();
  s_orig_cb(data);
}

uint8_t __real_BTM_BleScan(uint8_t start, uint32_t duration, void *a, void *b, void *c);
uint8_t __wrap_BTM_BleScan(uint8_t start, uint32_t duration, void *a, void *b, void *c) {
  uint8_t r = __real_BTM_BleScan(start, duration, a, b, c);
  if (start) {
    g_scan_inst.ble_scan_start++;
    if (r != 1 /* BTM_CMD_STARTED */)
      g_scan_inst.ble_scan_start_bad++;
  } else {
    g_scan_inst.ble_scan_stop++;
  }
  return r;
}

void __real_btu_start_timer(void *p_tle, uint16_t type, uint32_t timeout_sec);
void __wrap_btu_start_timer(void *p_tle, uint16_t type, uint32_t timeout_sec) {
  if (type == 109 /* BTU_TTYPE_BLE_SCAN */) {
    g_scan_inst.scan_tle = p_tle;
    g_scan_inst.timer_start++;
    g_scan_inst.last_timer_start_us = esp_timer_get_time();
  }
  __real_btu_start_timer(p_tle, type, timeout_sec);
}

void __real_btm_ble_timeout(void *p_tle);
void __wrap_btm_ble_timeout(void *p_tle) {
  if (p_tle == g_scan_inst.scan_tle) {
    g_scan_inst.ble_timeout++;
    g_scan_inst.last_timeout_us = esp_timer_get_time();
  }
  __real_btm_ble_timeout(p_tle);
}

void *__real_osi_alarm_new(const char *name, scan_inst_cb_t cb, void *data, uint64_t expire);
void *__wrap_osi_alarm_new(const char *name, scan_inst_cb_t cb, void *data, uint64_t expire) {
  // btu_start_timer's wrap records the scan entry before the real one creates its alarm
  if (data != nullptr && data == g_scan_inst.scan_tle && s_orig_cb == nullptr) {
    s_orig_cb = cb;
    return __real_osi_alarm_new(name, scan_inst_shim, data, expire);
  }
  return __real_osi_alarm_new(name, cb, data, expire);
}

struct scan_inst_msg_t {
  uint8_t sig, aid, pid, act;
};
int __real_btc_transfer_context(scan_inst_msg_t *msg, void *arg, int arg_len, void *cp, void *fr);
int __wrap_btc_transfer_context(scan_inst_msg_t *msg, void *arg, int arg_len, void *cp, void *fr) {
  bool cmpl = msg != nullptr && arg != nullptr && msg->sig == 1 && msg->act == ESP_GAP_BLE_SCAN_RESULT_EVT &&
              arg_len >= 4 && msg->pid == 5 /* BTC_PID_GAP_BLE */ &&
              static_cast<esp_ble_gap_cb_param_t *>(arg)->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT;
  int r = __real_btc_transfer_context(msg, arg, arg_len, cp, fr);
  if (cmpl) {
    g_scan_inst.pid = msg->pid;
    g_scan_inst.inq_cmpl_post++;
    g_scan_inst.last_inq_cmpl_us = esp_timer_get_time();
    if (r != 0)
      g_scan_inst.inq_cmpl_post_fail++;
  }
  return r;
}
}
