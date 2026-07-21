/*
 * esp_now_hosted — ESP-NOW-over-CustomRpc wire protocol.
 *
 * Shared, byte-for-byte-identical contract between:
 *   - the host shim         (esphome/components/esp32_hosted/esp_now_hosted.cpp)
 *   - the coprocessor firmware (esphome/esp-hosted-firmware)
 *
 * It rides esp-hosted's CustomRpc channel (RPC ID 388, "peer data transfer",
 * available since esp-hosted v2.8.1), teaching the radio-less host <-> radio
 * co-processor link to carry esp_now.h, which esp-hosted itself does not proxy
 * (Espressif issue espressif/esp-hosted-mcu#19).
 *
 * KEEP THE TWO COPIES IN SYNC. The canonical copy lives here; the coprocessor
 * firmware uses a verbatim copy. Both sides are little-endian, so these packed
 * structs are wire-compatible with no byte-swapping.
 */

#ifndef ESP_NOW_HOSTED_RPC_H
#define ESP_NOW_HOSTED_RPC_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── CustomRpc message IDs (any uint32_t except 0xFFFFFFFF) ──────────────────
 * One REQ handler slot on the device; three event handler slots on the host.
 * The bytes spell "now" + index, a private range unlikely to clash with other
 * CustomRpc users (e.g. the stock peer_data_transfer example's 1..6).         */
#define ESP_NOW_HOSTED_MSG_REQ 0x6E6F7701u  /* host   -> device : request envelope */
#define ESP_NOW_HOSTED_MSG_RESP 0x6E6F7702u /* device -> host   : reply to a REQ   */
#define ESP_NOW_HOSTED_MSG_RECV 0x6E6F7703u /* device -> host   : async RX frame   */
#define ESP_NOW_HOSTED_MSG_SEND 0x6E6F7704u /* device -> host   : async TX status  */

/* ── Request opcodes ────────────────────────────────────────────────────── */
enum {
  ESP_NOW_HOSTED_OP_INIT = 1,          /* esp_now_init + register device recv/send cbs */
  ESP_NOW_HOSTED_OP_DEINIT = 2,        /* unregister cbs + esp_now_deinit             */
  ESP_NOW_HOSTED_OP_ADD_PEER = 3,      /* payload: esp_now_hosted_peer_t              */
  ESP_NOW_HOSTED_OP_DEL_PEER = 4,      /* payload: 6-byte peer MAC                    */
  ESP_NOW_HOSTED_OP_IS_PEER_EXIST = 5, /* payload: 6-byte MAC; ret: 1 byte bool       */
  ESP_NOW_HOSTED_OP_SEND = 6,          /* payload: esp_now_hosted_send_req_t          */
  ESP_NOW_HOSTED_OP_GET_VERSION = 7,   /* ret: uint32 version                        */
  ESP_NOW_HOSTED_OP_SET_PMK = 8,       /* payload: 16-byte PMK                        */
  ESP_NOW_HOSTED_OP_MOD_PEER = 9,      /* payload: esp_now_hosted_peer_t              */
};

/* Largest ESP-NOW payload we forward. ESP-NOW v2 (IDF >= 5.4) is 1470 B; well
 * under esp-hosted's 8166 B CustomRpc cap, so the shim never truncates.        */
#define ESP_NOW_HOSTED_MAX_FRAME 1470u
/* Envelope slack for the largest opcode payload (a SEND req wrapping a frame). */
#define ESP_NOW_HOSTED_MAX_PAYLOAD (ESP_NOW_HOSTED_MAX_FRAME + 16u)
/* Host request/response round-trip timeout over the transport. Generous:
 * normal RTT is sub-millisecond, but Wi-Fi/BLE contention on the co-processor
 * can stall the RX thread.                                                     */
#define ESP_NOW_HOSTED_TIMEOUT_MS 2000

/* ── Envelopes ──────────────────────────────────────────────────────────── */

/* These payloads are shared verbatim with the C co-processor firmware, so they
 * use C's `typedef struct {...} name;` idiom rather than C++ `using` aliases,
 * which would not compile there. Silence clang-tidy's modernize-use-using for
 * the shared struct block. */
// NOLINTBEGIN(modernize-use-using)
typedef struct {
  uint8_t opcode;       /* one of ESP_NOW_HOSTED_OP_*                        */
  uint8_t seq;          /* wraps 0..255; echoed in the response for matching */
  uint16_t payload_len; /* bytes of opcode-specific payload that follow      */
  uint8_t payload[];    /* flexible                                          */
} __attribute__((packed)) esp_now_hosted_req_t;

typedef struct {
  uint8_t opcode;   /* echoes the request opcode                         */
  uint8_t seq;      /* echoes the request seq                            */
  int32_t status;   /* esp_err_t from the native call on the co-processor */
  uint16_t ret_len; /* bytes of return payload that follow               */
  uint8_t ret[];    /* flexible (e.g. version u32, is_peer_exist bool)   */
} __attribute__((packed)) esp_now_hosted_resp_t;

/* ── Opcode payloads ────────────────────────────────────────────────────── */

/* esp_now_peer_info_t minus the host-only `priv` pointer, which is meaningless
 * across the transport and never set by ESPHome's espnow component.           */
typedef struct {
  uint8_t peer_addr[6];
  uint8_t lmk[16];
  uint8_t channel; /* 0 = current channel                              */
  uint8_t ifidx;   /* wifi_interface_t (0=STA, 1=AP)                    */
  uint8_t encrypt; /* bool                                             */
} __attribute__((packed)) esp_now_hosted_peer_t;

typedef struct {
  uint8_t has_addr; /* 0 => peer_addr is NULL (broadcast to all peers)   */
  uint8_t peer_addr[6];
  uint16_t data_len;
  uint8_t data[]; /* flexible, up to ESP_NOW_HOSTED_MAX_FRAME          */
} __attribute__((packed)) esp_now_hosted_send_req_t;

/* ── Async events (device -> host) ──────────────────────────────────────── */

/* Reconstructed on the host into an esp_now_recv_info_t + a minimal
 * wifi_pkt_rx_ctrl_t. ESPHome's espnow reads info->src_addr, info->des_addr,
 * info->rx_ctrl->rssi and info->rx_ctrl->timestamp.                           */
typedef struct {
  uint8_t src_addr[6];
  uint8_t des_addr[6];
  int8_t rssi;
  uint8_t channel;
  uint16_t data_len;
  uint8_t data[]; /* flexible                                          */
} __attribute__((packed)) esp_now_hosted_recv_evt_t;

typedef struct {
  uint8_t des_addr[6];
  uint8_t status; /* esp_now_send_status_t (0 = success)               */
} __attribute__((packed)) esp_now_hosted_send_evt_t;
// NOLINTEND(modernize-use-using)

#ifdef __cplusplus
}
#endif

#endif /* ESP_NOW_HOSTED_RPC_H */
