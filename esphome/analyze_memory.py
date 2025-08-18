"""Memory usage analyzer for ESPHome compiled binaries."""

from collections import defaultdict
import json
import logging
from pathlib import Path
import re
import subprocess

_LOGGER = logging.getLogger(__name__)

# Pattern to extract ESPHome component namespaces dynamically
ESPHOME_COMPONENT_PATTERN = re.compile(r"esphome::([a-zA-Z0-9_]+)::")

# Component identification rules
# Symbol patterns: patterns found in raw symbol names
SYMBOL_PATTERNS = {
    "freertos": [
        "vTask",
        "xTask",
        "xQueue",
        "pvPort",
        "vPort",
        "uxTask",
        "pcTask",
        "prvTimerTask",
        "prvAddNewTaskToReadyList",
        "pxReadyTasksLists",
        "prvAddCurrentTaskToDelayedList",
        "xEventGroupWaitBits",
        "xRingbufferSendFromISR",
        "prvSendItemDoneNoSplit",
        "prvReceiveGeneric",
        "prvSendAcquireGeneric",
        "prvCopyItemAllowSplit",
        "xEventGroup",
        "xRingbuffer",
        "prvSend",
        "prvReceive",
        "prvCopy",
        "xPort",
        "ulTaskGenericNotifyTake",
        "prvIdleTask",
        "prvInitialiseNewTask",
        "prvIsYieldRequiredSMP",
        "prvGetItemByteBuf",
        "prvInitializeNewRingbuffer",
        "prvAcquireItemNoSplit",
        "prvNotifyQueueSetContainer",
        "ucStaticTimerQueueStorage",
        "eTaskGetState",
        "main_task",
        "do_system_init_fn",
        "xSemaphoreCreateGenericWithCaps",
        "vListInsert",
        "uxListRemove",
        "vRingbufferReturnItem",
        "vRingbufferReturnItemFromISR",
        "prvCheckItemFitsByteBuffer",
        "prvGetCurMaxSizeAllowSplit",
        "tick_hook",
        "sys_sem_new",
        "sys_arch_mbox_fetch",
        "sys_arch_sem_wait",
        "prvDeleteTCB",
        "vQueueDeleteWithCaps",
        "vRingbufferDeleteWithCaps",
        "vSemaphoreDeleteWithCaps",
        "prvCheckItemAvail",
        "prvCheckTaskCanBeScheduledSMP",
        "prvGetCurMaxSizeNoSplit",
        "prvResetNextTaskUnblockTime",
        "prvReturnItemByteBuf",
        "vApplicationStackOverflowHook",
        "vApplicationGetIdleTaskMemory",
        "sys_init",
        "sys_mbox_new",
        "sys_arch_mbox_tryfetch",
    ],
    "xtensa": ["xt_", "_xt_", "xPortEnterCriticalTimeout"],
    "heap": ["heap_", "multi_heap"],
    "spi_flash": ["spi_flash"],
    "rtc": ["rtc_", "rtcio_ll_"],
    "gpio_driver": ["gpio_", "pins"],
    "uart_driver": ["uart", "_uart", "UART"],
    "timer": ["timer_", "esp_timer"],
    "peripherals": ["periph_", "periman"],
    "network_stack": [
        "vj_compress",
        "raw_sendto",
        "raw_input",
        "etharp_",
        "icmp_input",
        "socket_ipv6",
        "ip_napt",
        "socket_ipv4_multicast",
        "socket_ipv6_multicast",
        "netconn_",
        "recv_raw",
        "accept_function",
        "netconn_recv_data",
        "netconn_accept",
        "netconn_write_vectors_partly",
        "netconn_drain",
        "raw_connect",
        "raw_bind",
        "icmp_send_response",
        "sockets",
        "icmp_dest_unreach",
        "inet_chksum_pseudo",
        "alloc_socket",
        "done_socket",
        "set_global_fd_sets",
        "inet_chksum_pbuf",
        "tryget_socket_unconn_locked",
        "tryget_socket_unconn",
        "cs_create_ctrl_sock",
        "netbuf_alloc",
    ],
    "ipv6_stack": ["nd6_", "ip6_", "mld6_", "icmp6_", "icmp6_input"],
    "wifi_stack": [
        "ieee80211",
        "hostap",
        "sta_",
        "ap_",
        "scan_",
        "wifi_",
        "wpa_",
        "wps_",
        "esp_wifi",
        "cnx_",
        "wpa3_",
        "sae_",
        "wDev_",
        "ic_",
        "mac_",
        "esf_buf",
        "gWpaSm",
        "sm_WPA",
        "eapol_",
        "owe_",
        "wifiLowLevelInit",
        "s_do_mapping",
        "gScanStruct",
        "ppSearchTxframe",
        "ppMapWaitTxq",
        "ppFillAMPDUBar",
        "ppCheckTxConnTrafficIdle",
        "ppCalTkipMic",
    ],
    "bluetooth": ["bt_", "ble_", "l2c_", "gatt_", "gap_", "hci_", "BT_init"],
    "wifi_bt_coex": ["coex"],
    "bluetooth_rom": ["r_ble", "r_lld", "r_llc", "r_llm"],
    "bluedroid_bt": [
        "bluedroid",
        "btc_",
        "bta_",
        "btm_",
        "btu_",
        "BTM_",
        "GATT",
        "L2CA_",
        "smp_",
        "gatts_",
        "attp_",
        "l2cu_",
        "l2cb",
        "smp_cb",
        "BTA_GATTC_",
        "SMP_",
        "BTU_",
        "BTA_Dm",
        "GAP_Ble",
        "BT_tx_if",
        "host_recv_pkt_cb",
        "saved_local_oob_data",
        "string_to_bdaddr",
        "string_is_bdaddr",
        "CalConnectParamTimeout",
        "transmit_fragment",
        "transmit_data",
        "event_command_ready",
        "read_command_complete_header",
        "parse_read_local_extended_features_response",
        "parse_read_local_version_info_response",
        "should_request_high",
        "btdm_wakeup_request",
        "BTA_SetAttributeValue",
        "BTA_EnableBluetooth",
        "transmit_command_futured",
        "transmit_command",
        "get_waiting_command",
        "make_command",
        "transmit_downward",
        "host_recv_adv_packet",
        "copy_extra_byte_in_db",
        "parse_read_local_supported_commands_response",
    ],
    "crypto_math": [
        "ecp_",
        "bignum_",
        "mpi_",
        "sswu",
        "modp",
        "dragonfly_",
        "gcm_mult",
        "__multiply",
        "quorem",
        "__mdiff",
        "__lshift",
        "__mprec_tens",
        "ECC_",
        "multiprecision_",
        "mix_sub_columns",
        "sbox",
        "gfm2_sbox",
        "gfm3_sbox",
        "curve_p256",
        "curve",
        "p_256_init_curve",
        "shift_sub_rows",
        "rshift",
    ],
    "hw_crypto": ["esp_aes", "esp_sha", "esp_rsa", "esp_bignum", "esp_mpi"],
    "libc": [
        "printf",
        "scanf",
        "malloc",
        "free",
        "memcpy",
        "memset",
        "strcpy",
        "strlen",
        "_dtoa",
        "_fopen",
        "__sfvwrite_r",
        "qsort",
        "__sf",
        "__sflush_r",
        "__srefill_r",
        "_impure_data",
        "_reclaim_reent",
        "_open_r",
        "strncpy",
        "_strtod_l",
        "__gethex",
        "__hexnan",
        "_setenv_r",
        "_tzset_unlocked_r",
        "__tzcalc_limits",
        "select",
        "scalbnf",
        "strtof",
        "strtof_l",
        "__d2b",
        "__b2d",
        "__s2b",
        "_Balloc",
        "__multadd",
        "__lo0bits",
        "__atexit0",
        "__smakebuf_r",
        "__swhatbuf_r",
        "_sungetc_r",
        "_close_r",
        "_link_r",
        "_unsetenv_r",
        "_rename_r",
        "__month_lengths",
        "tzinfo",
        "__ratio",
        "__hi0bits",
        "__ulp",
        "__any_on",
        "__copybits",
        "L_shift",
        "_fcntl_r",
        "_lseek_r",
        "_read_r",
        "_write_r",
        "_unlink_r",
        "_fstat_r",
        "access",
        "fsync",
        "tcsetattr",
        "tcgetattr",
        "tcflush",
        "tcdrain",
        "__ssrefill_r",
        "_stat_r",
        "__hexdig_fun",
        "__mcmp",
        "_fwalk_sglue",
        "__fpclassifyf",
        "_setlocale_r",
        "_mbrtowc_r",
        "fcntl",
        "__match",
        "_lock_close",
        "__c$",
        "__func__$",
        "__FUNCTION__$",
        "DAYS_IN_MONTH",
        "_DAYS_BEFORE_MONTH",
        "CSWTCH$",
        "dst$",
        "sulp",
    ],
    "string_ops": ["strcmp", "strncmp", "strchr", "strstr", "strtok", "strdup"],
    "memory_alloc": ["malloc", "calloc", "realloc", "free", "_sbrk"],
    "file_io": [
        "fread",
        "fwrite",
        "fopen",
        "fclose",
        "fseek",
        "ftell",
        "fflush",
        "s_fd_table",
    ],
    "string_formatting": [
        "snprintf",
        "vsnprintf",
        "sprintf",
        "vsprintf",
        "sscanf",
        "vsscanf",
    ],
    "cpp_anonymous": ["_GLOBAL__N_", "n$"],
    "cpp_runtime": ["__cxx", "_ZN", "_ZL", "_ZSt", "__gxx_personality", "_Z16"],
    "exception_handling": ["__cxa_", "_Unwind_", "__gcc_personality", "uw_frame_state"],
    "static_init": ["_GLOBAL__sub_I_"],
    "mdns_lib": ["mdns"],
    "phy_radio": [
        "phy_",
        "rf_",
        "chip_",
        "register_chipv7",
        "pbus_",
        "bb_",
        "fe_",
        "rfcal_",
        "ram_rfcal",
        "tx_pwctrl",
        "rx_chan",
        "set_rx_gain",
        "set_chan",
        "agc_reg",
        "ram_txiq",
        "ram_txdc",
        "ram_gen_rx_gain",
        "rx_11b_opt",
        "set_rx_sense",
        "set_rx_gain_cal",
        "set_chan_dig_gain",
        "tx_pwctrl_init_cal",
        "rfcal_txiq",
        "set_tx_gain_table",
        "correct_rfpll_offset",
        "pll_correct_dcap",
        "txiq_cal_init",
        "pwdet_sar",
        "pwdet_sar2_init",
        "ram_iq_est_enable",
        "ram_rfpll_set_freq",
        "ant_wifirx_cfg",
        "ant_btrx_cfg",
        "force_txrxoff",
        "force_txrx_off",
        "tx_paon_set",
        "opt_11b_resart",
        "rfpll_1p2_opt",
        "ram_dc_iq_est",
        "ram_start_tx_tone",
        "ram_en_pwdet",
        "ram_cbw2040_cfg",
        "rxdc_est_min",
        "i2cmst_reg_init",
        "temprature_sens_read",
        "ram_restart_cal",
        "ram_write_gain_mem",
        "ram_wait_rfpll_cal_end",
        "txcal_debuge_mode",
        "ant_wifitx_cfg",
        "reg_init_begin",
    ],
    "wifi_phy_pp": ["pp_", "ppT", "ppR", "ppP", "ppInstall", "ppCalTxAMPDULength"],
    "wifi_lmac": ["lmac"],
    "wifi_device": ["wdev", "wDev_"],
    "power_mgmt": [
        "pm_",
        "sleep",
        "rtc_sleep",
        "light_sleep",
        "deep_sleep",
        "power_down",
        "g_pm",
    ],
    "memory_mgmt": [
        "mem_",
        "memory_",
        "tlsf_",
        "memp_",
        "pbuf_",
        "pbuf_alloc",
        "pbuf_copy_partial_pbuf",
    ],
    "hal_layer": ["hal_"],
    "clock_mgmt": [
        "clk_",
        "clock_",
        "rtc_clk",
        "apb_",
        "cpu_freq",
        "setCpuFrequencyMhz",
    ],
    "cache_mgmt": ["cache"],
    "flash_ops": ["flash", "image_load"],
    "interrupt_handlers": [
        "isr",
        "interrupt",
        "intr_",
        "exc_",
        "exception",
        "port_IntStack",
    ],
    "wrapper_functions": ["_wrapper"],
    "error_handling": ["panic", "abort", "assert", "error_", "fault"],
    "authentication": ["auth"],
    "ppp_protocol": ["ppp", "ipcp_", "lcp_", "chap_", "LcpEchoCheck"],
    "dhcp": ["dhcp", "handle_dhcp"],
    "ethernet_phy": [
        "emac_",
        "eth_phy_",
        "phy_tlk110",
        "phy_lan87",
        "phy_ip101",
        "phy_rtl",
        "phy_dp83",
        "phy_ksz",
        "lan87xx_",
        "rtl8201_",
        "ip101_",
        "ksz80xx_",
        "jl1101_",
        "dp83848_",
        "eth_on_state_changed",
    ],
    "threading": ["pthread_", "thread_", "_task_"],
    "pthread": ["pthread"],
    "synchronization": ["mutex", "semaphore", "spinlock", "portMUX"],
    "math_lib": [
        "sin",
        "cos",
        "tan",
        "sqrt",
        "pow",
        "exp",
        "log",
        "atan",
        "asin",
        "acos",
        "floor",
        "ceil",
        "fabs",
        "round",
    ],
    "random": ["rand", "random", "rng_", "prng"],
    "time_lib": [
        "time",
        "clock",
        "gettimeofday",
        "settimeofday",
        "localtime",
        "gmtime",
        "mktime",
        "strftime",
    ],
    "console_io": ["console_", "uart_tx", "uart_rx", "puts", "putchar", "getchar"],
    "rom_functions": ["r_", "rom_"],
    "compiler_runtime": [
        "__divdi3",
        "__udivdi3",
        "__moddi3",
        "__muldi3",
        "__ashldi3",
        "__ashrdi3",
        "__lshrdi3",
        "__cmpdi2",
        "__fixdfdi",
        "__floatdidf",
    ],
    "libgcc": ["libgcc", "_divdi3", "_udivdi3"],
    "boot_startup": ["boot", "start_cpu", "call_start", "startup", "bootloader"],
    "bootloader": ["bootloader_", "esp_bootloader"],
    "app_framework": ["app_", "initArduino", "setup", "loop", "Update"],
    "weak_symbols": ["__weak_"],
    "compiler_builtins": ["__builtin_"],
    "vfs": ["vfs_", "VFS"],
    "esp32_sdk": ["esp32_", "esp32c", "esp32s"],
    "usb": ["usb_", "USB", "cdc_", "CDC"],
    "i2c_driver": ["i2c_", "I2C"],
    "i2s_driver": ["i2s_", "I2S"],
    "spi_driver": ["spi_", "SPI"],
    "adc_driver": ["adc_", "ADC"],
    "dac_driver": ["dac_", "DAC"],
    "touch_driver": ["touch_", "TOUCH"],
    "pwm_driver": ["pwm_", "PWM", "ledc_", "LEDC"],
    "rmt_driver": ["rmt_", "RMT"],
    "pcnt_driver": ["pcnt_", "PCNT"],
    "can_driver": ["can_", "CAN", "twai_", "TWAI"],
    "sdmmc_driver": ["sdmmc_", "SDMMC", "sdcard", "sd_card"],
    "temp_sensor": ["temp_sensor", "tsens_"],
    "watchdog": ["wdt_", "WDT", "watchdog"],
    "brownout": ["brownout", "bod_"],
    "ulp": ["ulp_", "ULP"],
    "psram": ["psram", "PSRAM", "spiram", "SPIRAM"],
    "efuse": ["efuse", "EFUSE"],
    "partition": ["partition", "esp_partition"],
    "esp_event": ["esp_event", "event_loop", "event_callback"],
    "esp_console": ["esp_console", "console_"],
    "chip_specific": ["chip_", "esp_chip"],
    "esp_system_utils": ["esp_system", "esp_hw", "esp_clk", "esp_sleep"],
    "ipc": ["esp_ipc", "ipc_"],
    "wifi_config": [
        "g_cnxMgr",
        "gChmCxt",
        "g_ic",
        "TxRxCxt",
        "s_dp",
        "s_ni",
        "s_reg_dump",
        "packet$",
        "d_mult_table",
        "K",
        "fcstab",
    ],
    "smartconfig": ["sc_ack_send"],
    "rc_calibration": ["rc_cal", "rcUpdate"],
    "noise_floor": ["noise_check"],
    "rf_calibration": [
        "set_rx_sense",
        "set_rx_gain_cal",
        "set_chan_dig_gain",
        "tx_pwctrl_init_cal",
        "rfcal_txiq",
        "set_tx_gain_table",
        "correct_rfpll_offset",
        "pll_correct_dcap",
        "txiq_cal_init",
        "pwdet_sar",
        "rx_11b_opt",
    ],
    "wifi_crypto": [
        "pk_use_ecparams",
        "process_segments",
        "ccmp_",
        "rc4_",
        "aria_",
        "mgf_mask",
        "dh_group",
        "ccmp_aad_nonce",
        "ccmp_encrypt",
        "rc4_skip",
        "aria_sb1",
        "aria_sb2",
        "aria_is1",
        "aria_is2",
        "aria_sl",
        "aria_a",
    ],
    "radio_control": ["fsm_input", "fsm_sconfreq"],
    "pbuf": [
        "pbuf_",
    ],
    "event_group": ["xEventGroup"],
    "ringbuffer": ["xRingbuffer", "prvSend", "prvReceive", "prvCopy"],
    "provisioning": ["prov_", "prov_stop_and_notify"],
    "scan": ["gScanStruct"],
    "port": ["xPort"],
    "elf_loader": [
        "elf_add",
        "elf_add_note",
        "elf_add_segment",
        "process_image",
        "read_encoded",
        "read_encoded_value",
        "read_encoded_value_with_base",
        "process_image_header",
    ],
    "socket_api": [
        "sockets",
        "netconn_",
        "accept_function",
        "recv_raw",
        "socket_ipv4_multicast",
        "socket_ipv6_multicast",
    ],
    "igmp": ["igmp_", "igmp_send", "igmp_input"],
    "icmp6": ["icmp6_"],
    "arp": ["arp_table"],
    "ampdu": [
        "ampdu_",
        "rcAmpdu",
        "trc_onAmpduOp",
        "rcAmpduLowerRate",
        "ampdu_dispatch_upto",
    ],
    "ieee802_11": ["ieee802_11_", "ieee802_11_parse_elems"],
    "rate_control": ["rssi_margin", "rcGetSched", "get_rate_fcc_index"],
    "nan": ["nan_dp_", "nan_dp_post_tx", "nan_dp_delete_peer"],
    "channel_mgmt": ["chm_init", "chm_set_current_channel"],
    "trace": ["trc_init", "trc_onAmpduOp"],
    "country_code": ["country_info", "country_info_24ghz"],
    "multicore": ["do_multicore_settings"],
    "Update_lib": ["Update"],
    "stdio": [
        "__sf",
        "__sflush_r",
        "__srefill_r",
        "_impure_data",
        "_reclaim_reent",
        "_open_r",
    ],
    "strncpy_ops": ["strncpy"],
    "math_internal": ["__mdiff", "__lshift", "__mprec_tens", "quorem"],
    "character_class": ["__chclass"],
    "camellia": ["camellia_", "camellia_feistel"],
    "crypto_tables": ["FSb", "FSb2", "FSb3", "FSb4"],
    "event_buffer": ["g_eb_list_desc", "eb_space"],
    "base_node": ["base_node_", "base_node_add_handler"],
    "file_descriptor": ["s_fd_table"],
    "tx_delay": ["tx_delay_cfg"],
    "deinit": ["deinit_functions"],
    "lcp_echo": ["LcpEchoCheck"],
    "raw_api": ["raw_bind", "raw_connect"],
    "checksum": ["process_checksum"],
    "entry_management": ["add_entry"],
    "esp_ota": ["esp_ota", "ota_", "read_otadata"],
    "http_server": [
        "httpd_",
        "parse_url_char",
        "cb_headers_complete",
        "delete_entry",
        "validate_structure",
        "config_save",
        "config_new",
        "verify_url",
        "cb_url",
    ],
    "misc_system": [
        "alarm_cbs",
        "start_up",
        "tokens",
        "unhex",
        "osi_funcs_ro",
        "enum_function",
        "fragment_and_dispatch",
        "alarm_set",
        "osi_alarm_new",
        "config_set_string",
        "config_update_newest_section",
        "config_remove_key",
        "method_strings",
        "interop_match",
        "interop_database",
        "__state_table",
        "__action_table",
        "s_stub_table",
        "s_context",
        "s_mmu_ctx",
        "s_get_bus_mask",
        "hli_queue_put",
        "list_remove",
        "list_delete",
        "lock_acquire_generic",
        "is_vect_desc_usable",
        "io_mode_str",
        "__c$20233",
        "interface",
        "read_id_core",
        "subscribe_idle",
        "unsubscribe_idle",
        "s_clkout_handle",
        "lock_release_generic",
        "config_set_int",
        "config_get_int",
        "config_get_string",
        "config_has_key",
        "config_remove_section",
        "osi_alarm_init",
        "osi_alarm_deinit",
        "fixed_queue_enqueue",
        "fixed_queue_dequeue",
        "fixed_queue_new",
        "fixed_pkt_queue_enqueue",
        "fixed_pkt_queue_new",
        "list_append",
        "list_prepend",
        "list_insert_after",
        "list_contains",
        "list_get_node",
        "hash_function_blob",
        "cb_no_body",
        "cb_on_body",
        "profile_tab",
        "get_arg",
        "trim",
        "buf$",
        "process_appended_hash_and_sig$constprop$0",
        "uuidType",
        "allocate_svc_db_buf",
        "_hostname_is_ours",
        "s_hli_handlers",
        "tick_cb",
        "idle_cb",
        "input",
        "entry_find",
        "section_find",
        "find_bucket_entry_",
        "config_has_section",
        "hli_queue_create",
        "hli_queue_get",
        "hli_c_handler",
        "future_ready",
        "future_await",
        "future_new",
        "pkt_queue_enqueue",
        "pkt_queue_dequeue",
        "pkt_queue_cleanup",
        "pkt_queue_create",
        "pkt_queue_destroy",
        "fixed_pkt_queue_dequeue",
        "osi_alarm_cancel",
        "osi_alarm_is_active",
        "osi_sem_take",
        "osi_event_create",
        "osi_event_bind",
        "alarm_cb_handler",
        "list_foreach",
        "list_back",
        "list_front",
        "list_clear",
        "fixed_queue_try_peek_first",
        "translate_path",
        "get_idx",
        "find_key",
        "init",
        "end",
        "start",
        "set_read_value",
        "copy_address_list",
        "copy_and_key",
        "sdk_cfg_opts",
        "leftshift_onebit",
        "config_section_end",
        "config_section_begin",
        "find_entry_and_check_all_reset",
        "image_validate",
        "xPendingReadyList",
        "vListInitialise",
        "lock_init_generic",
        "ant_bttx_cfg",
        "ant_dft_cfg",
        "cs_send_to_ctrl_sock",
        "config_llc_util_funcs_reset",
        "make_set_adv_report_flow_control",
        "make_set_event_mask",
        "raw_new",
        "raw_remove",
        "BTE_InitStack",
        "parse_read_local_supported_features_response",
        "__math_invalidf",
        "tinytens",
        "__mprec_tinytens",
        "__mprec_bigtens",
        "vRingbufferDelete",
        "vRingbufferDeleteWithCaps",
        "vRingbufferReturnItem",
        "vRingbufferReturnItemFromISR",
        "get_acl_data_size_ble",
        "get_features_ble",
        "get_features_classic",
        "get_acl_packet_size_ble",
        "get_acl_packet_size_classic",
        "supports_extended_inquiry_response",
        "supports_rssi_with_inquiry_results",
        "supports_interlaced_inquiry_scan",
        "supports_reading_remote_extended_features",
    ],
    "bluetooth_ll": [
        "lld_pdu_",
        "ld_acl_",
        "lld_stop_ind_handler",
        "lld_evt_winsize_change",
        "config_lld_evt_funcs_reset",
        "config_lld_funcs_reset",
        "config_llm_funcs_reset",
        "llm_set_long_adv_data",
        "lld_retry_tx_prog",
        "llc_link_sup_to_ind_handler",
        "config_llc_funcs_reset",
        "lld_evt_rxwin_compute",
        "config_btdm_funcs_reset",
        "config_ea_funcs_reset",
        "llc_defalut_state_tab_reset",
        "config_rwip_funcs_reset",
        "ke_lmp_rx_flooding_detect",
    ],
}

# Demangled patterns: patterns found in demangled C++ names
DEMANGLED_PATTERNS = {
    "gpio_driver": ["GPIO"],
    "uart_driver": ["UART"],
    "network_stack": [
        "lwip",
        "tcp",
        "udp",
        "ip4",
        "ip6",
        "dhcp",
        "dns",
        "netif",
        "ethernet",
        "ppp",
        "slip",
    ],
    "wifi_stack": ["NetworkInterface"],
    "nimble_bt": [
        "nimble",
        "NimBLE",
        "ble_hs",
        "ble_gap",
        "ble_gatt",
        "ble_att",
        "ble_l2cap",
        "ble_sm",
    ],
    "crypto": ["mbedtls", "crypto", "sha", "aes", "rsa", "ecc", "tls", "ssl"],
    "cpp_stdlib": ["std::", "__gnu_cxx::", "__cxxabiv"],
    "static_init": ["__static_initialization"],
    "rtti": ["__type_info", "__class_type_info"],
    "web_server_lib": ["AsyncWebServer", "AsyncWebHandler", "WebServer"],
    "async_tcp": ["AsyncClient", "AsyncServer"],
    "mdns_lib": ["mdns"],
    "json_lib": [
        "ArduinoJson",
        "JsonDocument",
        "JsonArray",
        "JsonObject",
        "deserialize",
        "serialize",
    ],
    "http_lib": ["HTTP", "http_", "Request", "Response", "Uri", "WebSocket"],
    "logging": ["log", "Log", "print", "Print", "diag_"],
    "authentication": ["checkDigestAuthentication"],
    "libgcc": ["libgcc"],
    "esp_system": ["esp_", "ESP"],
    "arduino": ["arduino"],
    "nvs": ["nvs_", "_ZTVN3nvs", "nvs::"],
    "filesystem": ["spiffs", "vfs"],
    "libc": ["newlib"],
}


# Get the list of actual ESPHome components by scanning the components directory
def get_esphome_components():
    """Get set of actual ESPHome components from the components directory."""
    components = set()

    # Find the components directory relative to this file
    current_dir = Path(__file__).parent
    components_dir = current_dir / "components"

    if components_dir.exists() and components_dir.is_dir():
        for item in components_dir.iterdir():
            if (
                item.is_dir()
                and not item.name.startswith(".")
                and not item.name.startswith("__")
            ):
                components.add(item.name)

    return components


# Cache the component list
ESPHOME_COMPONENTS = get_esphome_components()


class MemorySection:
    """Represents a memory section with its symbols."""

    def __init__(self, name: str):
        self.name = name
        self.symbols: list[tuple[str, int, str]] = []  # (symbol_name, size, component)
        self.total_size = 0


class ComponentMemory:
    """Tracks memory usage for a component."""

    def __init__(self, name: str):
        self.name = name
        self.text_size = 0  # Code in flash
        self.rodata_size = 0  # Read-only data in flash
        self.data_size = 0  # Initialized data (flash + ram)
        self.bss_size = 0  # Uninitialized data (ram only)
        self.symbol_count = 0

    @property
    def flash_total(self) -> int:
        return self.text_size + self.rodata_size + self.data_size

    @property
    def ram_total(self) -> int:
        return self.data_size + self.bss_size


class MemoryAnalyzer:
    """Analyzes memory usage from ELF files."""

    def __init__(
        self,
        elf_path: str,
        objdump_path: str | None = None,
        readelf_path: str | None = None,
        external_components: set[str] | None = None,
    ):
        self.elf_path = Path(elf_path)
        if not self.elf_path.exists():
            raise FileNotFoundError(f"ELF file not found: {elf_path}")

        self.objdump_path = objdump_path or "objdump"
        self.readelf_path = readelf_path or "readelf"
        self.external_components = external_components or set()

        self.sections: dict[str, MemorySection] = {}
        self.components: dict[str, ComponentMemory] = defaultdict(
            lambda: ComponentMemory("")
        )
        self._demangle_cache: dict[str, str] = {}
        self._uncategorized_symbols: list[tuple[str, str, int]] = []
        self._esphome_core_symbols: list[
            tuple[str, str, int]
        ] = []  # Track core symbols
        self._component_symbols: dict[str, list[tuple[str, str, int]]] = defaultdict(
            list
        )  # Track symbols for all components

    def analyze(self) -> dict[str, ComponentMemory]:
        """Analyze the ELF file and return component memory usage."""
        self._parse_sections()
        self._parse_symbols()
        self._categorize_symbols()
        return dict(self.components)

    def _parse_sections(self) -> None:
        """Parse section headers from ELF file."""
        try:
            result = subprocess.run(
                [self.readelf_path, "-S", str(self.elf_path)],
                capture_output=True,
                text=True,
                check=True,
            )

            # Parse section headers
            for line in result.stdout.splitlines():
                # Look for section entries
                match = re.match(
                    r"\s*\[\s*\d+\]\s+([\.\w]+)\s+\w+\s+[\da-fA-F]+\s+[\da-fA-F]+\s+([\da-fA-F]+)",
                    line,
                )
                if match:
                    section_name = match.group(1)
                    size_hex = match.group(2)
                    size = int(size_hex, 16)

                    # Map various section names to standard categories
                    mapped_section = None
                    if ".text" in section_name or ".iram" in section_name:
                        mapped_section = ".text"
                    elif ".rodata" in section_name:
                        mapped_section = ".rodata"
                    elif ".data" in section_name and "bss" not in section_name:
                        mapped_section = ".data"
                    elif ".bss" in section_name:
                        mapped_section = ".bss"

                    if mapped_section:
                        if mapped_section not in self.sections:
                            self.sections[mapped_section] = MemorySection(
                                mapped_section
                            )
                        self.sections[mapped_section].total_size += size

        except subprocess.CalledProcessError as e:
            _LOGGER.error(f"Failed to parse sections: {e}")
            raise

    def _parse_symbols(self) -> None:
        """Parse symbols from ELF file."""
        # Section mapping - centralizes the logic
        SECTION_MAPPING = {
            ".text": [".text", ".iram"],
            ".rodata": [".rodata"],
            ".data": [".data", ".dram"],
            ".bss": [".bss"],
        }

        def map_section_name(raw_section: str) -> str | None:
            """Map raw section name to standard section."""
            for standard_section, patterns in SECTION_MAPPING.items():
                if any(pattern in raw_section for pattern in patterns):
                    return standard_section
            return None

        def parse_symbol_line(line: str) -> tuple[str, str, int, str] | None:
            """Parse a single symbol line from objdump output.

            Returns (section, name, size, address) or None if not a valid symbol.
            Format: address l/g w/d F/O section size name
            Example: 40084870 l     F .iram0.text    00000000 _xt_user_exc
            """
            parts = line.split()
            if len(parts) < 5:
                return None

            try:
                # Validate and extract address
                address = parts[0]
                int(address, 16)
            except ValueError:
                return None

            # Look for F (function) or O (object) flag
            if "F" not in parts and "O" not in parts:
                return None

            # Find section, size, and name
            for i, part in enumerate(parts):
                if part.startswith("."):
                    section = map_section_name(part)
                    if section and i + 1 < len(parts):
                        try:
                            size = int(parts[i + 1], 16)
                            if i + 2 < len(parts) and size > 0:
                                name = " ".join(parts[i + 2 :])
                                return (section, name, size, address)
                        except ValueError:
                            pass
                    break
            return None

        try:
            result = subprocess.run(
                [self.objdump_path, "-t", str(self.elf_path)],
                capture_output=True,
                text=True,
                check=True,
            )

            # Track seen addresses to avoid duplicates
            seen_addresses: set[str] = set()

            for line in result.stdout.splitlines():
                symbol_info = parse_symbol_line(line)
                if symbol_info:
                    section, name, size, address = symbol_info
                    # Skip duplicate symbols at the same address (e.g., C1/C2 constructors)
                    if address not in seen_addresses and section in self.sections:
                        self.sections[section].symbols.append((name, size, ""))
                        seen_addresses.add(address)

        except subprocess.CalledProcessError as e:
            _LOGGER.error(f"Failed to parse symbols: {e}")
            raise

    def _categorize_symbols(self) -> None:
        """Categorize symbols by component."""
        # First, collect all unique symbol names for batch demangling
        all_symbols = set()
        for section in self.sections.values():
            for symbol_name, _, _ in section.symbols:
                all_symbols.add(symbol_name)

        # Batch demangle all symbols at once
        self._batch_demangle_symbols(list(all_symbols))

        # Now categorize with cached demangled names
        for section_name, section in self.sections.items():
            for symbol_name, size, _ in section.symbols:
                component = self._identify_component(symbol_name)

                if component not in self.components:
                    self.components[component] = ComponentMemory(component)

                comp_mem = self.components[component]
                comp_mem.symbol_count += 1

                if section_name == ".text":
                    comp_mem.text_size += size
                elif section_name == ".rodata":
                    comp_mem.rodata_size += size
                elif section_name == ".data":
                    comp_mem.data_size += size
                elif section_name == ".bss":
                    comp_mem.bss_size += size

                # Track uncategorized symbols
                if component == "other" and size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._uncategorized_symbols.append((symbol_name, demangled, size))

                # Track ESPHome core symbols for detailed analysis
                if component == "[esphome]core" and size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._esphome_core_symbols.append((symbol_name, demangled, size))

                # Track all component symbols for detailed analysis
                if size > 0:
                    demangled = self._demangle_symbol(symbol_name)
                    self._component_symbols[component].append(
                        (symbol_name, demangled, size)
                    )

    def _identify_component(self, symbol_name: str) -> str:
        """Identify which component a symbol belongs to."""
        # Demangle C++ names if needed
        demangled = self._demangle_symbol(symbol_name)

        # Check for special component classes first (before namespace pattern)
        # This handles cases like esphome::ESPHomeOTAComponent which should map to ota
        if "esphome::" in demangled:
            # Check for special component classes that include component name in the class
            # For example: esphome::ESPHomeOTAComponent -> ota component
            for component_name in ESPHOME_COMPONENTS:
                # Check various naming patterns
                component_upper = component_name.upper()
                component_camel = component_name.replace("_", "").title()
                patterns = [
                    f"esphome::{component_upper}Component",  # e.g., esphome::OTAComponent
                    f"esphome::ESPHome{component_upper}Component",  # e.g., esphome::ESPHomeOTAComponent
                    f"esphome::{component_camel}Component",  # e.g., esphome::OtaComponent
                    f"esphome::ESPHome{component_camel}Component",  # e.g., esphome::ESPHomeOtaComponent
                ]

                if any(pattern in demangled for pattern in patterns):
                    return f"[esphome]{component_name}"

        # Check for ESPHome component namespaces
        match = ESPHOME_COMPONENT_PATTERN.search(demangled)
        if match:
            component_name = match.group(1)
            # Strip trailing underscore if present (e.g., switch_ -> switch)
            component_name = component_name.rstrip("_")

            # Check if this is an actual component in the components directory
            if component_name in ESPHOME_COMPONENTS:
                return f"[esphome]{component_name}"
            # Check if this is a known external component from the config
            if component_name in self.external_components:
                return f"[external]{component_name}"
            # Everything else in esphome:: namespace is core
            return "[esphome]core"

        # Check for esphome core namespace (no component namespace)
        if "esphome::" in demangled:
            # If no component match found, it's core
            return "[esphome]core"

        # Check against symbol patterns
        for component, patterns in SYMBOL_PATTERNS.items():
            if any(pattern in symbol_name for pattern in patterns):
                return component

        # Check against demangled patterns
        for component, patterns in DEMANGLED_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return component

        # Special cases that need more complex logic

        # Check if spi_flash vs spi_driver
        if "spi_" in symbol_name or "SPI" in symbol_name:
            if "spi_flash" in symbol_name:
                return "spi_flash"
            return "spi_driver"

        # libc special printf variants
        if symbol_name.startswith("_") and symbol_name[1:].replace("_r", "").replace(
            "v", ""
        ).replace("s", "") in ["printf", "fprintf", "sprintf", "scanf"]:
            return "libc"

        # Track uncategorized symbols for analysis
        return "other"

    def _batch_demangle_symbols(self, symbols: list[str]) -> None:
        """Batch demangle C++ symbol names for efficiency."""
        if not symbols:
            return

        # Try to find the appropriate c++filt for the platform
        cppfilt_cmd = "c++filt"

        # Check if we have a toolchain-specific c++filt
        if self.objdump_path and self.objdump_path != "objdump":
            # Replace objdump with c++filt in the path
            potential_cppfilt = self.objdump_path.replace("objdump", "c++filt")
            if Path(potential_cppfilt).exists():
                cppfilt_cmd = potential_cppfilt

        try:
            # Send all symbols to c++filt at once
            result = subprocess.run(
                [cppfilt_cmd],
                input="\n".join(symbols),
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0:
                demangled_lines = result.stdout.strip().split("\n")
                # Map original to demangled names
                for original, demangled in zip(symbols, demangled_lines):
                    self._demangle_cache[original] = demangled
            else:
                # If batch fails, cache originals
                for symbol in symbols:
                    self._demangle_cache[symbol] = symbol
        except Exception:
            # On error, cache originals
            for symbol in symbols:
                self._demangle_cache[symbol] = symbol

    def _demangle_symbol(self, symbol: str) -> str:
        """Get demangled C++ symbol name from cache."""
        return self._demangle_cache.get(symbol, symbol)

    def _categorize_esphome_core_symbol(self, demangled: str) -> str:
        """Categorize ESPHome core symbols into subcategories."""
        # Dictionary of patterns for core subcategories
        CORE_SUBCATEGORY_PATTERNS = {
            "Component Framework": ["Component"],
            "Application Core": ["Application"],
            "Scheduler": ["Scheduler"],
            "Logging": ["Logger", "log_"],
            "Preferences": ["preferences", "Preferences"],
            "Synchronization": ["Mutex", "Lock"],
            "Helpers": ["Helper"],
            "Network Utilities": ["network", "Network"],
            "Time Management": ["time", "Time"],
            "String Utilities": ["str_", "string"],
            "Parsing/Formatting": ["parse_", "format_"],
            "Optional Types": ["optional", "Optional"],
            "Callbacks": ["Callback", "callback"],
            "Color Utilities": ["Color"],
            "C++ Operators": ["operator"],
            "Global Variables": ["global_", "_GLOBAL"],
            "Setup/Loop": ["setup", "loop"],
            "System Control": ["reboot", "restart"],
            "GPIO Management": ["GPIO", "gpio"],
            "Interrupt Handling": ["ISR", "interrupt"],
            "Hooks": ["Hook", "hook"],
            "Entity Base Classes": ["Entity"],
            "Automation Framework": ["automation", "Automation"],
            "Automation Components": ["Condition", "Action", "Trigger"],
            "Lambda Support": ["lambda"],
        }

        # Special patterns that need to be checked separately
        if any(pattern in demangled for pattern in ["vtable", "typeinfo", "thunk"]):
            return "C++ Runtime (vtables/RTTI)"

        if demangled.startswith("std::"):
            return "C++ STL"

        # Check against patterns
        for category, patterns in CORE_SUBCATEGORY_PATTERNS.items():
            if any(pattern in demangled for pattern in patterns):
                return category

        return "Other Core"

    def generate_report(self, detailed: bool = False) -> str:
        """Generate a formatted memory report."""
        components = sorted(
            self.components.items(), key=lambda x: x[1].flash_total, reverse=True
        )

        # Calculate totals
        total_flash = sum(c.flash_total for _, c in components)
        total_ram = sum(c.ram_total for _, c in components)

        # Build report
        lines = []

        # Column width constants
        COL_COMPONENT = 29
        COL_FLASH_TEXT = 14
        COL_FLASH_DATA = 14
        COL_RAM_DATA = 12
        COL_RAM_BSS = 12
        COL_TOTAL_FLASH = 15
        COL_TOTAL_RAM = 12
        COL_SEPARATOR = 3  # " | "

        # Core analysis column widths
        COL_CORE_SUBCATEGORY = 30
        COL_CORE_SIZE = 12
        COL_CORE_COUNT = 6
        COL_CORE_PERCENT = 10

        # Calculate the exact table width
        table_width = (
            COL_COMPONENT
            + COL_SEPARATOR
            + COL_FLASH_TEXT
            + COL_SEPARATOR
            + COL_FLASH_DATA
            + COL_SEPARATOR
            + COL_RAM_DATA
            + COL_SEPARATOR
            + COL_RAM_BSS
            + COL_SEPARATOR
            + COL_TOTAL_FLASH
            + COL_SEPARATOR
            + COL_TOTAL_RAM
        )

        lines.append("=" * table_width)
        lines.append("Component Memory Analysis".center(table_width))
        lines.append("=" * table_width)
        lines.append("")

        # Main table - fixed column widths
        lines.append(
            f"{'Component':<{COL_COMPONENT}} | {'Flash (text)':>{COL_FLASH_TEXT}} | {'Flash (data)':>{COL_FLASH_DATA}} | {'RAM (data)':>{COL_RAM_DATA}} | {'RAM (bss)':>{COL_RAM_BSS}} | {'Total Flash':>{COL_TOTAL_FLASH}} | {'Total RAM':>{COL_TOTAL_RAM}}"
        )
        lines.append(
            "-" * COL_COMPONENT
            + "-+-"
            + "-" * COL_FLASH_TEXT
            + "-+-"
            + "-" * COL_FLASH_DATA
            + "-+-"
            + "-" * COL_RAM_DATA
            + "-+-"
            + "-" * COL_RAM_BSS
            + "-+-"
            + "-" * COL_TOTAL_FLASH
            + "-+-"
            + "-" * COL_TOTAL_RAM
        )

        for name, mem in components:
            if mem.flash_total > 0 or mem.ram_total > 0:
                flash_rodata = mem.rodata_size + mem.data_size
                lines.append(
                    f"{name:<{COL_COMPONENT}} | {mem.text_size:>{COL_FLASH_TEXT - 2},} B | {flash_rodata:>{COL_FLASH_DATA - 2},} B | "
                    f"{mem.data_size:>{COL_RAM_DATA - 2},} B | {mem.bss_size:>{COL_RAM_BSS - 2},} B | "
                    f"{mem.flash_total:>{COL_TOTAL_FLASH - 2},} B | {mem.ram_total:>{COL_TOTAL_RAM - 2},} B"
                )

        lines.append(
            "-" * COL_COMPONENT
            + "-+-"
            + "-" * COL_FLASH_TEXT
            + "-+-"
            + "-" * COL_FLASH_DATA
            + "-+-"
            + "-" * COL_RAM_DATA
            + "-+-"
            + "-" * COL_RAM_BSS
            + "-+-"
            + "-" * COL_TOTAL_FLASH
            + "-+-"
            + "-" * COL_TOTAL_RAM
        )
        lines.append(
            f"{'TOTAL':<{COL_COMPONENT}} | {' ':>{COL_FLASH_TEXT}} | {' ':>{COL_FLASH_DATA}} | "
            f"{' ':>{COL_RAM_DATA}} | {' ':>{COL_RAM_BSS}} | "
            f"{total_flash:>{COL_TOTAL_FLASH - 2},} B | {total_ram:>{COL_TOTAL_RAM - 2},} B"
        )

        # Top consumers
        lines.append("")
        lines.append("Top Flash Consumers:")
        for i, (name, mem) in enumerate(components[:25]):
            if mem.flash_total > 0:
                percentage = (
                    (mem.flash_total / total_flash * 100) if total_flash > 0 else 0
                )
                lines.append(
                    f"{i + 1}. {name} ({mem.flash_total:,} B) - {percentage:.1f}% of analyzed flash"
                )

        lines.append("")
        lines.append("Top RAM Consumers:")
        ram_components = sorted(components, key=lambda x: x[1].ram_total, reverse=True)
        for i, (name, mem) in enumerate(ram_components[:25]):
            if mem.ram_total > 0:
                percentage = (mem.ram_total / total_ram * 100) if total_ram > 0 else 0
                lines.append(
                    f"{i + 1}. {name} ({mem.ram_total:,} B) - {percentage:.1f}% of analyzed RAM"
                )

        lines.append("")
        lines.append(
            "Note: This analysis covers symbols in the ELF file. Some runtime allocations may not be included."
        )
        lines.append("=" * table_width)

        # Add ESPHome core detailed analysis if there are core symbols
        if self._esphome_core_symbols:
            lines.append("")
            lines.append("=" * table_width)
            lines.append("[esphome]core Detailed Analysis".center(table_width))
            lines.append("=" * table_width)
            lines.append("")

            # Group core symbols by subcategory
            core_subcategories: dict[str, list[tuple[str, str, int]]] = defaultdict(
                list
            )

            for symbol, demangled, size in self._esphome_core_symbols:
                # Categorize based on demangled name patterns
                subcategory = self._categorize_esphome_core_symbol(demangled)
                core_subcategories[subcategory].append((symbol, demangled, size))

            # Sort subcategories by total size
            sorted_subcategories = sorted(
                [
                    (name, symbols, sum(s[2] for s in symbols))
                    for name, symbols in core_subcategories.items()
                ],
                key=lambda x: x[2],
                reverse=True,
            )

            lines.append(
                f"{'Subcategory':<{COL_CORE_SUBCATEGORY}} | {'Size':>{COL_CORE_SIZE}} | "
                f"{'Count':>{COL_CORE_COUNT}} | {'% of Core':>{COL_CORE_PERCENT}}"
            )
            lines.append(
                "-" * COL_CORE_SUBCATEGORY
                + "-+-"
                + "-" * COL_CORE_SIZE
                + "-+-"
                + "-" * COL_CORE_COUNT
                + "-+-"
                + "-" * COL_CORE_PERCENT
            )

            core_total = sum(size for _, _, size in self._esphome_core_symbols)

            for subcategory, symbols, total_size in sorted_subcategories:
                percentage = (total_size / core_total * 100) if core_total > 0 else 0
                lines.append(
                    f"{subcategory:<{COL_CORE_SUBCATEGORY}} | {total_size:>{COL_CORE_SIZE - 2},} B | "
                    f"{len(symbols):>{COL_CORE_COUNT}} | {percentage:>{COL_CORE_PERCENT - 1}.1f}%"
                )

            # Top 10 largest core symbols
            lines.append("")
            lines.append("Top 10 Largest [esphome]core Symbols:")
            sorted_core_symbols = sorted(
                self._esphome_core_symbols, key=lambda x: x[2], reverse=True
            )

            for i, (symbol, demangled, size) in enumerate(sorted_core_symbols[:15]):
                lines.append(f"{i + 1}. {demangled} ({size:,} B)")

            lines.append("=" * table_width)

        # Add detailed analysis for top 5 ESPHome components
        esphome_components = [
            (name, mem)
            for name, mem in components
            if name.startswith("[esphome]") and name != "[esphome]core"
        ]
        top_esphome_components = sorted(
            esphome_components, key=lambda x: x[1].flash_total, reverse=True
        )[:25]

        # Check if API component exists and ensure it's included
        api_component = None
        for name, mem in components:
            if name == "[esphome]api":
                api_component = (name, mem)
                break

        # If API exists and not in top 5, add it to the list
        components_to_analyze = list(top_esphome_components)
        if api_component and api_component not in components_to_analyze:
            components_to_analyze.append(api_component)

        if components_to_analyze:
            for comp_name, comp_mem in components_to_analyze:
                comp_symbols = self._component_symbols.get(comp_name, [])
                if comp_symbols:
                    lines.append("")
                    lines.append("=" * table_width)
                    lines.append(f"{comp_name} Detailed Analysis".center(table_width))
                    lines.append("=" * table_width)
                    lines.append("")

                    # Sort symbols by size
                    sorted_symbols = sorted(
                        comp_symbols, key=lambda x: x[2], reverse=True
                    )

                    lines.append(f"Total symbols: {len(sorted_symbols)}")
                    lines.append(f"Total size: {comp_mem.flash_total:,} B")
                    lines.append("")

                    # For API component, show all symbols; for others show top 10
                    if comp_name == "[esphome]api":
                        lines.append(f"All {comp_name} Symbols (sorted by size):")
                        for i, (symbol, demangled, size) in enumerate(sorted_symbols):
                            lines.append(f"{i + 1}. {demangled} ({size:,} B)")
                    else:
                        lines.append(f"Top 10 Largest {comp_name} Symbols:")
                        for i, (symbol, demangled, size) in enumerate(
                            sorted_symbols[:10]
                        ):
                            lines.append(f"{i + 1}. {demangled} ({size:,} B)")

                    lines.append("=" * table_width)

        return "\n".join(lines)

    def to_json(self) -> str:
        """Export analysis results as JSON."""
        data = {
            "components": {
                name: {
                    "text": mem.text_size,
                    "rodata": mem.rodata_size,
                    "data": mem.data_size,
                    "bss": mem.bss_size,
                    "flash_total": mem.flash_total,
                    "ram_total": mem.ram_total,
                    "symbol_count": mem.symbol_count,
                }
                for name, mem in self.components.items()
            },
            "totals": {
                "flash": sum(c.flash_total for c in self.components.values()),
                "ram": sum(c.ram_total for c in self.components.values()),
            },
        }
        return json.dumps(data, indent=2)

    def dump_uncategorized_symbols(self, output_file: str | None = None) -> None:
        """Dump uncategorized symbols for analysis."""
        # Sort by size descending
        sorted_symbols = sorted(
            self._uncategorized_symbols, key=lambda x: x[2], reverse=True
        )

        lines = ["Uncategorized Symbols Analysis", "=" * 80]
        lines.append(f"Total uncategorized symbols: {len(sorted_symbols)}")
        lines.append(
            f"Total uncategorized size: {sum(s[2] for s in sorted_symbols):,} bytes"
        )
        lines.append("")
        lines.append(f"{'Size':>10} | {'Symbol':<60} | Demangled")
        lines.append("-" * 10 + "-+-" + "-" * 60 + "-+-" + "-" * 40)

        for symbol, demangled, size in sorted_symbols[:100]:  # Top 100
            if symbol != demangled:
                lines.append(f"{size:>10,} | {symbol[:60]:<60} | {demangled[:100]}")
            else:
                lines.append(f"{size:>10,} | {symbol[:60]:<60} | [not demangled]")

        if len(sorted_symbols) > 100:
            lines.append(f"\n... and {len(sorted_symbols) - 100} more symbols")

        content = "\n".join(lines)

        if output_file:
            with open(output_file, "w") as f:
                f.write(content)
        else:
            print(content)


def analyze_elf(
    elf_path: str,
    objdump_path: str | None = None,
    readelf_path: str | None = None,
    detailed: bool = False,
    external_components: set[str] | None = None,
) -> str:
    """Analyze an ELF file and return a memory report."""
    analyzer = MemoryAnalyzer(elf_path, objdump_path, readelf_path, external_components)
    analyzer.analyze()
    return analyzer.generate_report(detailed)


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: analyze_memory.py <elf_file>")
        sys.exit(1)

    try:
        report = analyze_elf(sys.argv[1])
        print(report)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
