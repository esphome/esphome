// Linker wraps that stop Bluedroid running a failed queued MTU or execute write request again until the BTU task
// stack overflows (esphome#17954). The same fix as removing the command from p_cmd_list in bta_gattc_act.c before
// the failure path pops the queue; drop once an ESP-IDF release has it.

// Compiled only with Bluedroid's private headers (bluedroid_headers_cmake); static analysis skips it
#ifdef USE_ESP32_BLE_BLUEDROID_HEADERS

#include <stddef.h>

#include "bta_gattc_int.h"
#include "osi/list.h"

// The command bta_gattc_cfg_mtu() or bta_gattc_execute() is running, NULL outside them
static tBTA_GATTC_CLCB *running_clcb = NULL;
static tBTA_GATTC_DATA *running_cmd = NULL;

void __real_bta_gattc_cfg_mtu(tBTA_GATTC_CLCB *p_clcb, tBTA_GATTC_DATA *p_data);
void __real_bta_gattc_execute(tBTA_GATTC_CLCB *p_clcb, tBTA_GATTC_DATA *p_data);
tGATT_STATUS __real_GATTC_ConfigureMTU(UINT16 conn_id);
tGATT_STATUS __real_GATTC_ExecuteWrite(UINT16 conn_id, BOOLEAN is_execute);

// Runs on the Bluetooth host task; the failure path pops the queue again, so this nests
static void run_command(void (*real)(tBTA_GATTC_CLCB *, tBTA_GATTC_DATA *), tBTA_GATTC_CLCB *p_clcb,
                        tBTA_GATTC_DATA *p_data) {
  tBTA_GATTC_CLCB *const outer_clcb = running_clcb;
  tBTA_GATTC_DATA *const outer_cmd = running_cmd;
  running_clcb = p_clcb;
  running_cmd = p_data;
  real(p_clcb, p_data);
  running_clcb = outer_clcb;
  running_cmd = outer_cmd;
}

// The send failed, so the caller clears p_q_cmd and pops the queue. A command popped from p_cmd_list is still its
// head and would run again; remove (and free) it first. A command not from the queue is not in it: a no-op.
static void drop_failed_command(void) {
  if (running_clcb != NULL && running_clcb->p_cmd_list != NULL && running_clcb->p_q_cmd == running_cmd) {
    list_remove(running_clcb->p_cmd_list, running_cmd);
  }
}

void __wrap_bta_gattc_cfg_mtu(tBTA_GATTC_CLCB *p_clcb, tBTA_GATTC_DATA *p_data) {
  run_command(__real_bta_gattc_cfg_mtu, p_clcb, p_data);
}

void __wrap_bta_gattc_execute(tBTA_GATTC_CLCB *p_clcb, tBTA_GATTC_DATA *p_data) {
  run_command(__real_bta_gattc_execute, p_clcb, p_data);
}

// The failure checks are the ones bta_gattc_cfg_mtu() and bta_gattc_execute() make
tGATT_STATUS __wrap_GATTC_ConfigureMTU(UINT16 conn_id) {
  const tGATT_STATUS status = __real_GATTC_ConfigureMTU(conn_id);
  if (status != GATT_SUCCESS && status != GATT_CMD_STARTED) {
    drop_failed_command();
  }
  return status;
}

tGATT_STATUS __wrap_GATTC_ExecuteWrite(UINT16 conn_id, BOOLEAN is_execute) {
  const tGATT_STATUS status = __real_GATTC_ExecuteWrite(conn_id, is_execute);
  if (status != BTA_GATT_OK) {
    drop_failed_command();
  }
  return status;
}

#endif  // USE_ESP32_BLE_BLUEDROID_HEADERS
