#pragma once

#ifdef USE_ESP32_VARIANT_ESP32C6

#include <esp_bt.h>

namespace esphome {
namespace esp32_ble {

static const esp_bt_controller_config_t BT_CONTROLLER_CONFIG = {
    .config_version = CONFIG_VERSION,
    .ble_ll_resolv_list_size = CONFIG_BT_LE_LL_RESOLV_LIST_SIZE,
    .ble_hci_evt_hi_buf_count = DEFAULT_BT_LE_HCI_EVT_HI_BUF_COUNT,
    .ble_hci_evt_lo_buf_count = DEFAULT_BT_LE_HCI_EVT_LO_BUF_COUNT,
    .ble_ll_sync_list_cnt = DEFAULT_BT_LE_MAX_PERIODIC_ADVERTISER_LIST,
    .ble_ll_sync_cnt = DEFAULT_BT_LE_MAX_PERIODIC_SYNCS,
    .ble_ll_rsp_dup_list_count = CONFIG_BT_LE_LL_DUP_SCAN_LIST_COUNT,
    .ble_ll_adv_dup_list_count = CONFIG_BT_LE_LL_DUP_SCAN_LIST_COUNT,
    .ble_ll_tx_pwr_dbm = BLE_LL_TX_PWR_DBM_N,
    .rtc_freq = RTC_FREQ_N,
    .ble_ll_sca = CONFIG_BT_LE_LL_SCA,
    .ble_ll_scan_phy_number = BLE_LL_SCAN_PHY_NUMBER_N,
    .ble_ll_conn_def_auth_pyld_tmo = BLE_LL_CONN_DEF_AUTH_PYLD_TMO_N,
    .ble_ll_jitter_usecs = BLE_LL_JITTER_USECS_N,
    .ble_ll_sched_max_adv_pdu_usecs = BLE_LL_SCHED_MAX_ADV_PDU_USECS_N,
    .ble_ll_sched_direct_adv_max_usecs = BLE_LL_SCHED_DIRECT_ADV_MAX_USECS_N,
    .ble_ll_sched_adv_max_usecs = BLE_LL_SCHED_ADV_MAX_USECS_N,
    .ble_scan_rsp_data_max_len = DEFAULT_BT_LE_SCAN_RSP_DATA_MAX_LEN_N,
    .ble_ll_cfg_num_hci_cmd_pkts = BLE_LL_CFG_NUM_HCI_CMD_PKTS_N,
    .ble_ll_ctrl_proc_timeout_ms = BLE_LL_CTRL_PROC_TIMEOUT_MS_N,
    .nimble_max_connections = DEFAULT_BT_LE_MAX_CONNECTIONS,
    .ble_whitelist_size = DEFAULT_BT_NIMBLE_WHITELIST_SIZE,  // NOLINT
    .ble_acl_buf_size = DEFAULT_BT_LE_ACL_BUF_SIZE,
    .ble_acl_buf_count = DEFAULT_BT_LE_ACL_BUF_COUNT,
    .ble_hci_evt_buf_size = DEFAULT_BT_LE_HCI_EVT_BUF_SIZE,
    .ble_multi_adv_instances = DEFAULT_BT_LE_MAX_EXT_ADV_INSTANCES,
    .ble_ext_adv_max_size = DEFAULT_BT_LE_EXT_ADV_MAX_SIZE,
    .controller_task_stack_size = NIMBLE_LL_STACK_SIZE,
    .controller_task_prio = ESP_TASK_BT_CONTROLLER_PRIO,
    .controller_run_cpu = 0,
    .enable_qa_test = RUN_QA_TEST,
    .enable_bqb_test = RUN_BQB_TEST,
    .enable_uart_hci = HCI_UART_EN,
    .ble_hci_uart_port = DEFAULT_BT_LE_HCI_UART_PORT,
    .ble_hci_uart_baud = DEFAULT_BT_LE_HCI_UART_BAUD,
    .ble_hci_uart_data_bits = DEFAULT_BT_LE_HCI_UART_DATA_BITS,
    .ble_hci_uart_stop_bits = DEFAULT_BT_LE_HCI_UART_STOP_BITS,
    .ble_hci_uart_flow_ctrl = DEFAULT_BT_LE_HCI_UART_FLOW_CTRL,
    .ble_hci_uart_uart_parity = DEFAULT_BT_LE_HCI_UART_PARITY,
    .enable_tx_cca = DEFAULT_BT_LE_TX_CCA_ENABLED,
    .cca_rssi_thresh = 256 - DEFAULT_BT_LE_CCA_RSSI_THRESH,
    .sleep_en = NIMBLE_SLEEP_ENABLE,
    .coex_phy_coded_tx_rx_time_limit = DEFAULT_BT_LE_COEX_PHY_CODED_TX_RX_TLIM_EFF,
    .dis_scan_backoff = NIMBLE_DISABLE_SCAN_BACKOFF,
    .ble_scan_classify_filter_enable = 1,
    .main_xtal_freq = CONFIG_XTAL_FREQ,
    .version_num = (uint8_t) efuse_hal_chip_revision(),
    .cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
    .ignore_wl_for_direct_adv = 0,
    .enable_pcl = DEFAULT_BT_LE_POWER_CONTROL_ENABLED,
    .config_magic = CONFIG_MAGIC,
};

}  // namespace esp32_ble
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32C6
