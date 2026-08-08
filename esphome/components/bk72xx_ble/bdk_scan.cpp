// The raw BDK scan surface, kept in one file: every SDK call the scan
// reconciler makes goes through here. Of note is the active-scan start — the
// BDK's scan API is passive-only (its start path hardcodes the observer type
// with the active property bit commented out, identically in the 5.1 and 5.2
// stacks), so bdk_scan_start_active() packs the GAPM_ACTIVITY_START_CMD
// itself, field-for-field the SDK's app_ble_start_scaning() except for that
// bit, armed through the SDK's own operation bookkeeping so its state machine
// runs unchanged. The static asserts pin the SDK layout this depends on; the
// component pins beken-bdk 3.0.78.

#include "bdk_scan.h"

#ifdef USE_BK72XX_BLE

// Same SDK gate as bk72xx_ble.cpp (which carries the explanatory #error).
#if !defined(CLANG_TIDY) && __has_include("ble_api.h")

extern "C" {
#include "app_ble.h"     // app_ble_env, app_ble_run, app_ble_reset, actv_state_t,
                         // app_ble_actv_state_get, app_ble_env_state_get,
                         // app_ble_get_idle_actv_idx_handle, UNKNOW_ACT_IDX,
                         // bk_ble_* (via ble_api_5_x.h)
#include "kernel_msg.h"  // KERNEL_MSG_ALLOC, kernel_msg_send
#if __has_include("gapm_msg.h")
#include "gapm_msg.h"  // BLE 5.2 (BK7238/BK7252N): gapm_activity_start_cmd, GAPM_SCAN_*
#else
#include "gapm_task.h"  // BLE 5.1 (BK7231N/BK7236): same declarations, older header name
#endif
}

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::bk72xx_ble {

static const char *const TAG = "bk72xx_ble";

// Pin the SDK surface this file depends on: a beken-bdk bump that moves these
// must fail the build, not corrupt the kernel message.
static_assert(GAPM_SCAN_PROP_PHY_1M_BIT == (1 << 0) && GAPM_SCAN_PROP_ACTIVE_1M_BIT == (1 << 2),
              "beken-bdk GAPM scan property bits changed; revalidate bdk_scan_start_active() "
              "against the SDK's app_ble_start_scaning()");
static_assert(sizeof(struct gapm_scan_param) == 16 && sizeof(struct gapm_scan_wd_op_param) == 4,
              "beken-bdk gapm_scan_param layout changed; revalidate bdk_scan_start_active() "
              "against the SDK's app_ble_start_scaning()");
static_assert(INVALID_ACTIVITY_IDX == UNKNOW_ACT_IDX,
              "beken-bdk activity sentinel changed; revalidate the scan reconciler");

bool bdk_scan_ready() { return app_ble_env_state_get() == APP_BLE_READY; }

BdkActivityState bdk_scan_state(uint8_t activity_idx) {
  if (activity_idx == INVALID_ACTIVITY_IDX)
    return BdkActivityState::IDLE;
  switch (app_ble_actv_state_get(activity_idx)) {
    case ACTV_IDLE:
      return BdkActivityState::IDLE;
    case ACTV_SCAN_CREATED:
      return BdkActivityState::CREATED;
    case ACTV_SCAN_STARTED:
      return BdkActivityState::STARTED;
    default:
      return BdkActivityState::OTHER;
  }
}

uint8_t bdk_scan_acquire_activity() {
  uint8_t idx = app_ble_get_idle_actv_idx_handle(SCAN_ACTV);
  if (idx == INVALID_ACTIVITY_IDX)
    ESP_LOGE(TAG, "Scan start failed: no idle activity handle");
  return idx;
}

BdkOpResult bdk_scan_create(uint8_t activity_idx) {
  ble_err_t ret = bk_ble_create_scaning(activity_idx, nullptr);
  if (ret == ERR_SUCCESS)
    return BdkOpResult::OK;
  if (ret == ERR_BLE_STATUS)
    return BdkOpResult::BUSY;
  ESP_LOGE(TAG, "Scan activity create failed (err %d)", static_cast<int>(ret));
  return BdkOpResult::FAILED;
}

bool bdk_scan_start_passive(uint8_t activity_idx, uint16_t interval, uint16_t window) {
  struct scan_param sp;
  memset(&sp, 0, sizeof(sp));
  sp.channel_map = 7;  // advertising channels 37/38/39
  sp.interval = interval;
  sp.window = window;
  ble_err_t ret = bk_ble_scan_start(activity_idx, &sp, nullptr);
  if (ret != ERR_SUCCESS) {
    ESP_LOGE(TAG, "Scan start failed (err %d)", static_cast<int>(ret));
    return false;
  }
  return true;
}

bool bdk_scan_start_active(uint8_t activity_idx, uint16_t interval, uint16_t window) {
  app_ble_run(activity_idx, BLE_START_SCAN, 1 << BLE_OP_START_SCAN_POS, nullptr);
  struct gapm_activity_start_cmd *cmd =
      KERNEL_MSG_ALLOC(GAPM_ACTIVITY_START_CMD, TASK_BLE_GAPM, TASK_BLE_APP, gapm_activity_start_cmd);
  if (cmd == nullptr) {
    app_ble_reset();  // the SDK's own failure path for an unsent operation
    ESP_LOGE(TAG, "Scan start failed: kernel message allocation");
    return false;
  }
  cmd->operation = GAPM_START_ACTIVITY;
  cmd->actv_idx = app_ble_env.actvs[activity_idx].gap_advt_idx;
  cmd->u_param.scan_param.type = GAPM_SCAN_TYPE_OBSERVER;
  cmd->u_param.scan_param.prop = GAPM_SCAN_PROP_PHY_1M_BIT | GAPM_SCAN_PROP_ACTIVE_1M_BIT;
  cmd->u_param.scan_param.scan_param_1m.scan_intv = interval;
  cmd->u_param.scan_param.scan_param_1m.scan_wd = window;
  cmd->u_param.scan_param.scan_param_coded.scan_intv = 0;
  cmd->u_param.scan_param.scan_param_coded.scan_wd = 0;
  cmd->u_param.scan_param.dup_filt_pol = 0;
  cmd->u_param.scan_param.rsvd = 0;
  cmd->u_param.scan_param.duration = 0;  // scan until stopped
  cmd->u_param.scan_param.period = 10;   // matches the SDK's passive start
  kernel_msg_send(cmd);
  return true;
}

int bdk_scan_release(uint8_t activity_idx, bool created) {
  ble_err_t ret = created ? bk_ble_delete_scaning(activity_idx, nullptr) : bk_ble_scan_stop(activity_idx, nullptr);
  return static_cast<int>(ret == ERR_SUCCESS ? 0 : ret);
}

}  // namespace esphome::bk72xx_ble

#endif  // !CLANG_TIDY && ble_api.h
#endif  // USE_BK72XX_BLE
