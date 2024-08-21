// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(USE_ESP32)

#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <esp_attr.h>
#include <esp_eth.h>
#include <esp_heap_caps.h>
#include <esp_intr_alloc.h>
#include <esp_log.h>
#if ESP_IDF_VERSION_MAJOR >= 5
#include "esp_eth_phy_802_3.h"
#else
#include "eth_phy_regs_struct.h"
#endif
#include <esp_rom_sys.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <inttypes.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <hal/cpu_hal.h>
#include <sdkconfig.h>

/**
 * @brief SPI Instruction Set
 *
 */
#define ENC28J60_SPI_CMD_RCR (0x00) // Read Control Register
#define ENC28J60_SPI_CMD_RBM (0x01) // Read Buffer Memory
#define ENC28J60_SPI_CMD_WCR (0x02) // Write Control Register
#define ENC28J60_SPI_CMD_WBM (0x03) // Write Buffer Memory
#define ENC28J60_SPI_CMD_BFS (0x04) // Bit Field Set
#define ENC28J60_SPI_CMD_BFC (0x05) // Bit Field Clear
#define ENC28J60_SPI_CMD_SRC (0x07) // Soft Reset

/**
 * @brief Shared Registers in ENC28J60 (accessible on each bank)
 *
 */
#define ENC28J60_EIE      (0x1B) // Ethernet Interrupt Enable
#define ENC28J60_EIR      (0x1C) // Ethernet Interrupt flags
#define ENC28J60_ESTAT    (0x1D) // Ethernet Status
#define ENC28J60_ECON2    (0x1E) // Ethernet Control Register2
#define ENC28J60_ECON1    (0x1F) // Ethernet Control Register1

/**
 * @brief Per-bank Registers in ENC28J60
 * @note Address[15:12]: Register Type, 0 -> ETH, 1 -> MII/MAC
 *       Address[11:8] : Bank address
 *       Address[7:0]  : Register Index
 */
// Bank 0 Registers
#define ENC28J60_ERDPTL   (0x0000) // Read Pointer Low Byte ERDPT<7:0>)
#define ENC28J60_ERDPTH   (0x0001) // Read Pointer High Byte (ERDPT<12:8>)
#define ENC28J60_EWRPTL   (0x0002) // Write Pointer Low Byte (EWRPT<7:0>)
#define ENC28J60_EWRPTH   (0x0003) // Write Pointer High Byte (EWRPT<12:8>)
#define ENC28J60_ETXSTL   (0x0004) // TX Start Low Byte (ETXST<7:0>)
#define ENC28J60_ETXSTH   (0x0005) // TX Start High Byte (ETXST<12:8>)
#define ENC28J60_ETXNDL   (0x0006) // TX End Low Byte (ETXND<7:0>)
#define ENC28J60_ETXNDH   (0x0007) // TX End High Byte (ETXND<12:8>)
#define ENC28J60_ERXSTL   (0x0008) // RX Start Low Byte (ERXST<7:0>)
#define ENC28J60_ERXSTH   (0x0009) // RX Start High Byte (ERXST<12:8>)
#define ENC28J60_ERXNDL   (0x000A) // RX End Low Byte (ERXND<7:0>)
#define ENC28J60_ERXNDH   (0x000B) // RX End High Byte (ERXND<12:8>)
#define ENC28J60_ERXRDPTL (0x000C) // RX RD Pointer Low Byte (ERXRDPT<7:0>)
#define ENC28J60_ERXRDPTH (0x000D) // RX RD Pointer High Byte (ERXRDPT<12:8>)
#define ENC28J60_ERXWRPTL (0x000E) // RX WR Pointer Low Byte (ERXWRPT<7:0>)
#define ENC28J60_ERXWRPTH (0x000F) // RX WR Pointer High Byte (ERXWRPT<12:8>)
#define ENC28J60_EDMASTL  (0x0010) // DMA Start Low Byte (EDMAST<7:0>)
#define ENC28J60_EDMASTH  (0x0011) // DMA Start High Byte (EDMAST<12:8>)
#define ENC28J60_EDMANDL  (0x0012) // DMA End Low Byte (EDMAND<7:0>)
#define ENC28J60_EDMANDH  (0x0013) // DMA End High Byte (EDMAND<12:8>)
#define ENC28J60_EDMADSTL (0x0014) // DMA Destination Low Byte (EDMADST<7:0>)
#define ENC28J60_EDMADSTH (0x0015) // DMA Destination High Byte (EDMADST<12:8>)
#define ENC28J60_EDMACSL  (0x0016) // DMA Checksum Low Byte (EDMACS<7:0>)
#define ENC28J60_EDMACSH  (0x0017) // DMA Checksum High Byte (EDMACS<15:8>)

// Bank 1 Registers
#define ENC28J60_EHT0     (0x0100) // Hash Table Byte 0 (EHT<7:0>)
#define ENC28J60_EHT1     (0x0101) // Hash Table Byte 1 (EHT<15:8>)
#define ENC28J60_EHT2     (0x0102) // Hash Table Byte 2 (EHT<23:16>)
#define ENC28J60_EHT3     (0x0103) // Hash Table Byte 3 (EHT<31:24>)
#define ENC28J60_EHT4     (0x0104) // Hash Table Byte 4 (EHT<39:32>)
#define ENC28J60_EHT5     (0x0105) // Hash Table Byte 5 (EHT<47:40>)
#define ENC28J60_EHT6     (0x0106) // Hash Table Byte 6 (EHT<55:48>)
#define ENC28J60_EHT7     (0x0107) // Hash Table Byte 7 (EHT<63:56>)
#define ENC28J60_EPMM0    (0x0108) // Pattern Match Mask Byte 0 (EPMM<7:0>)
#define ENC28J60_EPMM1    (0x0109) // Pattern Match Mask Byte 1 (EPMM<15:8>)
#define ENC28J60_EPMM2    (0x010A) // Pattern Match Mask Byte 2 (EPMM<23:16>)
#define ENC28J60_EPMM3    (0x010B) // Pattern Match Mask Byte 3 (EPMM<31:24>)
#define ENC28J60_EPMM4    (0x010C) // Pattern Match Mask Byte 4 (EPMM<39:32>)
#define ENC28J60_EPMM5    (0x010D) // Pattern Match Mask Byte 5 (EPMM<47:40>)
#define ENC28J60_EPMM6    (0x010E) // Pattern Match Mask Byte 6 (EPMM<55:48>)
#define ENC28J60_EPMM7    (0x010F) // Pattern Match Mask Byte 7 (EPMM<63:56>)
#define ENC28J60_EPMCSL   (0x0110) // Pattern Match Checksum Low Byte (EPMCS<7:0>)
#define ENC28J60_EPMCSH   (0x0111) // Pattern Match Checksum High Byte (EPMCS<15:0>)
#define ENC28J60_EPMOL    (0x0114) // Pattern Match Offset Low Byte (EPMO<7:0>)
#define ENC28J60_EPMOH    (0x0115) // Pattern Match Offset High Byte (EPMO<12:8>)
#define ENC28J60_ERXFCON  (0x0118) // Receive Fileter Control
#define ENC28J60_EPKTCNT  (0x0119) // Ethernet Packet Count

// Bank 2 Register
#define ENC28J60_MACON1   (0x1200) // MAC Control Register 1
#define ENC28J60_MACON2   (0x1201) // MAC Control Register 2
#define ENC28J60_MACON3   (0x1202) // MAC Control Register 3
#define ENC28J60_MACON4   (0x1203) // MAC Control Register 4
#define ENC28J60_MABBIPG  (0x1204) // Back-to-Back Inter-Packet Gap (BBIPG<6:0>)
#define ENC28J60_MAIPGL   (0x1206) // Non-Back-to-Back Inter-Packet Gap Low Byte (MAIPGL<6:0>)
#define ENC28J60_MAIPGH   (0x1207) // Non-Back-to-Back Inter-Packet Gap High Byte (MAIPGH<6:0>)
#define ENC28J60_MACLCON1 (0x1208) // Retransmission Maximum (RETMAX<3:0>)
#define ENC28J60_MACLCON2 (0x1209) // Collision Window (COLWIN<5:0>)
#define ENC28J60_MAMXFLL  (0x120A) // Maximum Frame Length Low Byte (MAMXFL<7:0>)
#define ENC28J60_MAMXFLH  (0x120B) // Maximum Frame Length High Byte (MAMXFL<15:8>)
#define ENC28J60_MICMD    (0x1212) // MII Command Register
#define ENC28J60_MIREGADR (0x1214) // MII Register Address (MIREGADR<4:0>)
#define ENC28J60_MIWRL    (0x1216) // MII Write Data Low Byte (MIWR<7:0>)
#define ENC28J60_MIWRH    (0x1217) // MII Write Data High Byte (MIWR<15:8>)
#define ENC28J60_MIRDL    (0x1218) // MII Read Data Low Byte (MIRD<7:0>)
#define ENC28J60_MIRDH    (0x1219) // MII Read Data High Byte(MIRD<15:8>)

// Bank 3 Registers
#define ENC28J60_MAADR5   (0x1300) // MAC Address Byte 5 (MAADR<15:8>)
#define ENC28J60_MAADR6   (0x1301) // MAC Address Byte 6 (MAADR<7:0>)
#define ENC28J60_MAADR3   (0x1302) // MAC Address Byte 3 (MAADR<31:24>), OUI Byte 3
#define ENC28J60_MAADR4   (0x1303) // MAC Address Byte 4 (MAADR<23:16>)
#define ENC28J60_MAADR1   (0x1304) // MAC Address Byte 1 (MAADR<47:40>), OUI Byte 1
#define ENC28J60_MAADR2   (0x1305) // MAC Address Byte 2 (MAADR<39:32>), OUI Byte 2
#define ENC28J60_EBSTSD   (0x0306) // Built-in Self-Test Fill Seed (EBSTSD<7:0>)
#define ENC28J60_EBSTCON  (0x0307) // Built-in Self-Test Control
#define ENC28J60_EBSTCSL  (0x0308) // Built-in Self-Test Checksum Low Byte (EBSTCS<7:0>)
#define ENC28J60_EBSTCSH  (0x0309) // Built-in Self-Test Checksum High Byte (EBSTCS<15:8>)
#define ENC28J60_MISTAT   (0x130A) // MII Status Register
#define ENC28J60_EREVID   (0x0312) // Ethernet Revision ID (EREVID<4:0>)
#define ENC28J60_ECOCON   (0x0315) // Clock Output Control Register
#define ENC28J60_EFLOCON  (0x0317) // Ethernet Flow Control
#define ENC28J60_EPAUSL   (0x0318) // Pause Timer Value Low Byte (EPAUS<7:0>)
#define ENC28J60_EPAUSH   (0x0319) // Pause Timer Value High Byte (EPAUS<15:8>)

/**
 * @brief status and flag of ENC28J60 specific registers
 *
 */
// EIE bit definitions
#define EIE_INTIE     (1<<7) // Global INT Interrupt Enable
#define EIE_PKTIE     (1<<6) // Receive Packet Pending Interrupt Enable
#define EIE_DMAIE     (1<<5) // DMA Interrupt Enable
#define EIE_LINKIE    (1<<4) // Link Status Change Interrupt Enable
#define EIE_TXIE      (1<<3) // Transmit Enable
#define EIE_TXERIE    (1<<1) // Transmit Error Interrupt Enable
#define EIE_RXERIE    (1<<0) // Receive Error Interrupt Enable

// EIR bit definitions
#define EIR_PKTIF     (1<<6) // Receive Packet Pending Interrupt Flag
#define EIR_DMAIF     (1<<5) // DMA Interrupt Flag
#define EIR_LINKIF    (1<<4) // Link Change Interrupt Flag
#define EIR_TXIF      (1<<3) // Transmit Interrupt Flag
#define EIR_TXERIF    (1<<1) // Transmit Error Interrupt Flag
#define EIR_RXERIF    (1<<0) // Receive Error Interrupt Flag

// ESTAT bit definitions
#define ESTAT_INT       (1<<7) // INT Interrupt Flag
#define ESTAT_BUFER     (1<<6) // Buffer Error Status
#define ESTAT_LATECOL   (1<<4) // Late Collision Error
#define ESTAT_RXBUSY    (1<<2) // Receive Busy
#define ESTAT_TXABRT    (1<<1) // Transmit Abort Error
#define ESTAT_CLKRDY    (1<<0) // Clock Ready

// ECON2 bit definitions
#define ECON2_AUTOINC   (1<<7) // Automatic Buffer Pointer Increment Enable
#define ECON2_PKTDEC    (1<<6) // Packet Decrement
#define ECON2_PWRSV     (1<<5) // Power Save Enable
#define ECON2_VRPS      (1<<3) // Voltage Regulator Power Save Enable

// ECON1 bit definitions
#define ECON1_TXRST     (1<<7) // Transmit Logic Reset
#define ECON1_RXRST     (1<<6) // Receive Logic Reset
#define ECON1_DMAST     (1<<5) // DMA Start and Busy Status
#define ECON1_CSUMEN    (1<<4) // DMA Checksum Enable
#define ECON1_TXRTS     (1<<3) // Transmit Request to Send
#define ECON1_RXEN      (1<<2) // Receive Enable
#define ECON1_BSEL1     (1<<1) // Bank Select1
#define ECON1_BSEL0     (1<<0) // Bank Select0

// ERXFCON bit definitions
#define ERXFCON_UCEN    (1<<7) // Unicast Filter Enable
#define ERXFCON_ANDOR   (1<<6) // AND/OR Filter Select
#define ERXFCON_CRCEN   (1<<5) // Post-Filter CRC Check Enable
#define ERXFCON_PMEN    (1<<4) // Pattern Match Filter Enable
#define ERXFCON_MPEN    (1<<3) // Magic Packet Filter Enable
#define ERXFCON_HTEN    (1<<2) // Hash Table Filter Enable
#define ERXFCON_MCEN    (1<<1) // Multicast Filter Enable
#define ERXFCON_BCEN    (1<<0) // Broadcast Filter Enable

// MACON1 bit definitions
#define MACON1_TXPAUS   (1<<3) // Pause Control Frame Transmission Enable
#define MACON1_RXPAUS   (1<<2) // Pause Control Frame Reception Enable
#define MACON1_PASSALL  (1<<1) // Pass All Received Frames Enable
#define MACON1_MARXEN   (1<<0) // MAC Receive Enable

// MACON3 bit definitions
#define MACON3_PADCFG2   (1<<7) // Automatic Pad and CRC Configuration bit 2
#define MACON3_PADCFG1   (1<<6) // Automatic Pad and CRC Configuration bit 1
#define MACON3_PADCFG0   (1<<5) // Automatic Pad and CRC Configuration bit 0
#define MACON3_TXCRCEN   (1<<4) // Transmit CRC Enable
#define MACON3_PHDRLEN   (1<<3) // Proprietary Header Enable
#define MACON3_HFRMLEN   (1<<2) // Huge Frame Enable
#define MACON3_FRMLNEN   (1<<1) // Frame Length Checking Enable
#define MACON3_FULDPX    (1<<0) // MAC Full-Duplex Enable

// MACON4 bit definitions
#define MACON4_DEFER     (1<<6) // Defer Transmission Enable
#define MACON4_BPEN      (1<<5) // No Backoff During Backpressure Enable
#define MACON4_NOBKFF    (1<<4) // No Backoff Enable

// MICMD bit definitions
#define MICMD_MIISCAN    (1<<1) // MII Scan Enable
#define MICMD_MIIRD      (1<<0) // MII Read Enable

// EBSTCON bit definitions
#define EBSTCON_PSV2     (1<<7) // Pattern Shift Value 2
#define EBSTCON_PSV1     (1<<6) // Pattern Shift Value 1
#define EBSTCON_PSV0     (1<<5) // Pattern Shift Value 0
#define EBSTCON_PSEL     (1<<4) // Port Select
#define EBSTCON_TMSEL1   (1<<3) // Test Mode Select 1
#define EBSTCON_TMSEL0   (1<<2) // Test Mode Select 0
#define EBSTCON_TME      (1<<1) // Test Mode Enable
#define EBSTCON_BISTST   (1<<0) // Built-in Self-Test Start/Busy

// MISTAT bit definitions
#define MISTAT_NVALID    (1<<2) // MII Management Read Data Not Valid
#define MISTAT_SCAN      (1<<1) // MII Management Scan Operation in Progress
#define MISTAT_BUSY      (1<<0) // MII Management Busy

// EFLOCON bit definitions
#define EFLOCON_FULDPXS  (1<<2) // Full-Duplex Shadown
#define EFLOCON_FCEN1    (1<<1) // Flow Control Enable 1
#define EFLOCON_FCEN0    (1<<0) // Flow Control Enable 0

/**
 * @brief ENC28J60 Supported Revisions
 *
 */
typedef enum {
    ENC28J60_REV_B1 = 0b00000010,
    ENC28J60_REV_B4 = 0b00000100,
    ENC28J60_REV_B5 = 0b00000101,
    ENC28J60_REV_B7 = 0b00000110
} eth_enc28j60_rev_t;


static const char *TAG = "enc28j60";
#define PHY_CHECK(a, str, goto_tag, ...)                                          \
    do                                                                            \
    {                                                                             \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                        \
        }                                                                         \
    } while (0)

#define MAC_CHECK(a, str, goto_tag, ret_value, ...)                               \
    do                                                                            \
    {                                                                             \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            ret = ret_value;                                                      \
            goto goto_tag;                                                        \
        }                                                                         \
    } while (0)

#define MAC_CHECK_NO_RET(a, str, goto_tag, ...)                                   \
    do                                                                            \
    {                                                                             \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                        \
        }                                                                         \
    } while (0)

#define ENC28J60_SPI_LOCK_TIMEOUT_MS (50)
#define ENC28J60_REG_TRANS_LOCK_TIMEOUT_MS (150)
#define ENC28J60_PHY_OPERATION_TIMEOUT_US (150)
#define ENC28J60_SYSTEM_RESET_ADDITION_TIME_US (1000)
#define ENC28J60_TX_READY_TIMEOUT_MS (2000)

#define ENC28J60_BUFFER_SIZE (0x2000) // 8KB built-in buffer
/**
 *  ______
 * |__TX__| TX: 2 KB : [0x1800, 0x2000)
 * |      |
 * |  RX  | RX: 6 KB : [0x0000, 0x1800)
 * |______|
 *
 */
#define ENC28J60_BUF_RX_START (0)
#define ENC28J60_BUF_RX_END (ENC28J60_BUF_TX_START - 1)
#define ENC28J60_BUF_TX_START ((ENC28J60_BUFFER_SIZE / 4) * 3)
#define ENC28J60_BUF_TX_END (ENC28J60_BUFFER_SIZE - 1)

#define ENC28J60_RSV_SIZE (6) // Receive Status Vector Size
#define ENC28J60_TSV_SIZE (6) // Transmit Status Vector Size

typedef struct {
    uint8_t next_packet_low;
    uint8_t next_packet_high;
    uint8_t length_low;
    uint8_t length_high;
    uint8_t status_low;
    uint8_t status_high;
} enc28j60_rx_header_t;

typedef struct {
    uint16_t byte_cnt;

    uint8_t collision_cnt:4;
    uint8_t crc_err:1;
    uint8_t len_check_err:1;
    uint8_t len_out_range:1;
    uint8_t tx_done:1;

    uint8_t multicast:1;
    uint8_t broadcast:1;
    uint8_t pkt_defer:1;
    uint8_t excessive_defer:1;
    uint8_t excessive_collision:1;
    uint8_t late_collision:1;
    uint8_t giant:1;
    uint8_t underrun:1;

    uint16_t bytes_on_wire;

    uint8_t ctrl_frame:1;
    uint8_t pause_ctrl_frame:1;
    uint8_t backpressure_app:1;
    uint8_t vlan_frame:1;
} enc28j60_tsv_t;

typedef struct {
    esp_eth_mac_t parent;
    esp_eth_mediator_t *eth;
    spi_device_handle_t spi_hdl;
    SemaphoreHandle_t spi_lock;
    SemaphoreHandle_t reg_trans_lock;
    SemaphoreHandle_t tx_ready_sem;
    TaskHandle_t rx_task_hdl;
    uint32_t sw_reset_timeout_ms;
    uint32_t next_packet_ptr;
    uint32_t last_tsv_addr;
    int int_gpio_num;
    uint8_t addr[6];
    uint8_t last_bank;
    bool packets_remain;
    eth_enc28j60_rev_t revision;
} emac_enc28j60_t;

/***************Vendor Specific Register***************/

/**
 * @brief PHCON2(PHY Control Register 2)
 *
 */
typedef union {
    struct {
        uint32_t reserved_7_0 : 8;  // Reserved
        uint32_t pdpxmd : 1;        // PHY Duplex Mode bit
        uint32_t reserved_10_9: 2;  // Reserved
        uint32_t ppwrsv: 1;         // PHY Power-Down bit
        uint32_t reserved_13_12: 2; // Reserved
        uint32_t ploopbk: 1;        // PHY Loopback bit
        uint32_t prst: 1;           // PHY Software Reset bit
    };
    uint32_t val;
} phcon1_reg_t;
#define ETH_PHY_PHCON1_REG_ADDR (0x00)

/**
 * @brief PHCON2(PHY Control Register 2)
 *
 */
typedef union {
    struct {
        uint32_t reserved_7_0 : 8;  // Reserved
        uint32_t hdldis : 1;        // Half-Duplex Loopback Disable
        uint32_t reserved_9: 1;     // Reserved
        uint32_t jabber: 1;         // Disable Jabber Correction
        uint32_t reserved_12_11: 2; // Reserved
        uint32_t txdis: 1;          // Disable Twist-Pair Transmitter
        uint32_t frclnk: 1;         // Force Linkup
        uint32_t reserved_15: 1;    //Reserved
    };
    uint32_t val;
} phcon2_reg_t;
#define ETH_PHY_PHCON2_REG_ADDR (0x10)

/**
 * @brief PHSTAT2(PHY Status Register 2)
 *
 */
typedef union {
    struct {
        uint32_t reserved_4_0 : 5;   // Reserved
        uint32_t plrity : 1;         // Polarity Status
        uint32_t reserved_8_6 : 3;   // Reserved
        uint32_t dpxstat : 1;        // PHY Duplex Status
        uint32_t lstat : 1;          // PHY Link Status (non-latching)
        uint32_t colstat : 1;        // PHY Collision Status
        uint32_t rxstat : 1;         // PHY Receive Status
        uint32_t txstat : 1;         // PHY Transmit Status
        uint32_t reserved_15_14 : 2; // Reserved
    };
    uint32_t val;
} phstat2_reg_t;
#define ETH_PHY_PHSTAT2_REG_ADDR (0x11)

typedef struct {
    esp_eth_phy_t parent;
    esp_eth_mediator_t *eth;
    uint32_t addr;
    uint32_t reset_timeout_ms;
    eth_link_t link_status;
    int reset_gpio_num;
} phy_enc28j60_t;

static esp_err_t enc28j60_update_link_duplex_speed(phy_enc28j60_t *enc28j60)
{
    esp_eth_mediator_t *eth = enc28j60->eth;
    eth_speed_t speed = ETH_SPEED_10M; // enc28j60 speed is fixed to 10Mbps
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    phstat2_reg_t phstat;
    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_PHSTAT2_REG_ADDR, &(phstat.val)) == ESP_OK,
              "read PHSTAT2 failed", err);
    eth_link_t link = phstat.lstat ? ETH_LINK_UP : ETH_LINK_DOWN;
    /* check if link status changed */
    if (enc28j60->link_status != link) {
        /* when link up, read result */
        if (link == ETH_LINK_UP) {
            if (phstat.dpxstat) {
                duplex = ETH_DUPLEX_FULL;
            } else {
                duplex = ETH_DUPLEX_HALF;
            }
            PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed) == ESP_OK,
                      "change speed failed", err);
            PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex) == ESP_OK,
                      "change duplex failed", err);
        }
        PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link) == ESP_OK,
                  "change link failed", err);
        enc28j60->link_status = link;
    }
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t enc28j60_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth)
{
    PHY_CHECK(eth, "can't set mediator for enc28j60 to null", err);
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    enc28j60->eth = eth;
    return ESP_OK;
err:
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t enc28j60_get_link(esp_eth_phy_t *phy)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    /* Updata information about link, speed, duplex */
    PHY_CHECK(enc28j60_update_link_duplex_speed(enc28j60) == ESP_OK, "update link duplex speed failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t enc28j60_reset(esp_eth_phy_t *phy)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    enc28j60->link_status = ETH_LINK_DOWN;
    esp_eth_mediator_t *eth = enc28j60->eth;
    bmcr_reg_t bmcr = {.reset = 1};
    PHY_CHECK(eth->phy_reg_write(eth, enc28j60->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK,
              "write BMCR failed", err);
    /* Wait for reset complete */
    uint32_t to = 0;
    for (to = 0; to < enc28j60->reset_timeout_ms / 10; to++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK,
                  "read BMCR failed", err);
        if (!bmcr.reset) {
            break;
        }
    }
    PHY_CHECK(to < enc28j60->reset_timeout_ms / 10, "PHY reset timeout", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t enc28j60_reset_hw(esp_eth_phy_t *phy)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    // set reset_gpio_num minus zero can skip hardware reset phy chip
    if (enc28j60->reset_gpio_num >= 0) {
        gpio_reset_pin(enc28j60->reset_gpio_num);
        gpio_set_direction(enc28j60->reset_gpio_num, GPIO_MODE_OUTPUT);
        gpio_set_level(enc28j60->reset_gpio_num, 0);
        gpio_set_level(enc28j60->reset_gpio_num, 1);
    }
    return ESP_OK;
}
#if USE_ESP_IDF && (ESP_IDF_VERSION_MAJOR >= 5)
static esp_err_t enc28j60_autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autoneg_en_stat)
{
    /**
     * ENC28J60 does not support automatic duplex negotiation.
     * If it is connected to an automatic duplex negotiation enabled network switch,
     * ENC28J60 will be detected as a half-duplex device.
     * To communicate in Full-Duplex mode, ENC28J60 and the remote node
     * must be manually configured for full-duplex operation.
     */

    switch (cmd)
    {
    case ESP_ETH_PHY_AUTONEGO_RESTART:
        /* Fallthrough */
    case ESP_ETH_PHY_AUTONEGO_EN:
        return ESP_ERR_NOT_SUPPORTED;
    case ESP_ETH_PHY_AUTONEGO_DIS:
        /* Fallthrough */
    case ESP_ETH_PHY_AUTONEGO_G_STAT:
        *autoneg_en_stat = false;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}
#else
static esp_err_t enc28j60_negotiate(esp_eth_phy_t *phy)
{
    /**
     * ENC28J60 does not support automatic duplex negotiation.
     * If it is connected to an automatic duplex negotiation enabled network switch,
     * ENC28J60 will be detected as a half-duplex device.
     * To communicate in Full-Duplex mode, ENC28J60 and the remote node
     * must be manually configured for full-duplex operation.
     */
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    /* Updata information about link, speed, duplex */
    PHY_CHECK(enc28j60_update_link_duplex_speed(enc28j60) == ESP_OK, "update link duplex speed failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}
#endif

esp_err_t enc28j60_set_phy_duplex(esp_eth_phy_t *phy, eth_duplex_t duplex)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    esp_eth_mediator_t *eth = enc28j60->eth;
    phcon1_reg_t phcon1;

    /* Since the link is going to be reconfigured, consider it down to be status updated once the driver re-started */
    enc28j60->link_status = ETH_LINK_DOWN;

    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, 0, &phcon1.val) == ESP_OK,
              "read PHCON1 failed", err);
    switch (duplex) {
    case ETH_DUPLEX_HALF:
        phcon1.pdpxmd = 0;
        break;
    case ETH_DUPLEX_FULL:
        phcon1.pdpxmd = 1;
        break;
    default:
        PHY_CHECK(false, "unknown duplex", err);
        break;
    }

    PHY_CHECK(eth->phy_reg_write(eth, enc28j60->addr, 0, phcon1.val) == ESP_OK,
              "write PHCON1 failed", err);

    PHY_CHECK(enc28j60_update_link_duplex_speed(enc28j60) == ESP_OK, "update link duplex speed failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t enc28j60_pwrctl(esp_eth_phy_t *phy, bool enable)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    esp_eth_mediator_t *eth = enc28j60->eth;
    bmcr_reg_t bmcr;
    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK,
              "read BMCR failed", err);
    if (!enable) {
        /* Enable IEEE Power Down Mode */
        bmcr.power_down = 1;
    } else {
        /* Disable IEEE Power Down Mode */
        bmcr.power_down = 0;
    }
    PHY_CHECK(eth->phy_reg_write(eth, enc28j60->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK,
              "write BMCR failed", err);
    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK,
              "read BMCR failed", err);
    if (!enable) {
        PHY_CHECK(bmcr.power_down == 1, "power down failed", err);
    } else {
        PHY_CHECK(bmcr.power_down == 0, "power up failed", err);
    }
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t enc28j60_set_addr(esp_eth_phy_t *phy, uint32_t addr)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    enc28j60->addr = addr;
    return ESP_OK;
}

static esp_err_t enc28j60_get_addr(esp_eth_phy_t *phy, uint32_t *addr)
{
    PHY_CHECK(addr, "addr can't be null", err);
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    *addr = enc28j60->addr;
    return ESP_OK;
err:
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t enc28j60_del(esp_eth_phy_t *phy)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    free(enc28j60);
    return ESP_OK;
}

static esp_err_t enc28j60_init(esp_eth_phy_t *phy)
{
    phy_enc28j60_t *enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    esp_eth_mediator_t *eth = enc28j60->eth;
    /* Power on Ethernet PHY */
    PHY_CHECK(enc28j60_pwrctl(phy, true) == ESP_OK, "power control failed", err);
    /* Reset Ethernet PHY */
    PHY_CHECK(enc28j60_reset(phy) == ESP_OK, "reset failed", err);
    /* Check PHY ID */
    phyidr1_reg_t id1;
    phyidr2_reg_t id2;
    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_IDR1_REG_ADDR, &(id1.val)) == ESP_OK,
              "read ID1 failed", err);
    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_IDR2_REG_ADDR, &(id2.val)) == ESP_OK,
              "read ID2 failed", err);
    PHY_CHECK(id1.oui_msb == 0x0083 && id2.oui_lsb == 0x05 && id2.vendor_model == 0x00,
              "wrong chip ID", err);
    /* Disable half duplex loopback */
    phcon2_reg_t phcon2;
    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_PHCON2_REG_ADDR, &(phcon2.val)) == ESP_OK,
              "read PHCON2 failed", err);
    phcon2.hdldis = 1;
    PHY_CHECK(eth->phy_reg_write(eth, enc28j60->addr, ETH_PHY_PHCON2_REG_ADDR, phcon2.val) == ESP_OK,
              "write PHCON2 failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

static esp_err_t enc28j60_deinit(esp_eth_phy_t *phy)
{
    /* Power off Ethernet PHY */
    PHY_CHECK(enc28j60_pwrctl(phy, false) == ESP_OK, "power off Ethernet PHY failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

esp_eth_phy_t *esp_eth_phy_new_enc28j60(const eth_phy_config_t *config)
{
    PHY_CHECK(config, "can't set phy config to null", err);
    phy_enc28j60_t *enc28j60 = calloc(1, sizeof(phy_enc28j60_t));
    PHY_CHECK(enc28j60, "calloc enc28j60 failed", err);
    enc28j60->addr = config->phy_addr; // although PHY addr is meaningless to ENC28J60
    enc28j60->reset_timeout_ms = config->reset_timeout_ms;
    enc28j60->reset_gpio_num = config->reset_gpio_num;
    enc28j60->link_status = ETH_LINK_DOWN;
    enc28j60->parent.reset = enc28j60_reset;
    enc28j60->parent.reset_hw = enc28j60_reset_hw;
    enc28j60->parent.init = enc28j60_init;
    enc28j60->parent.deinit = enc28j60_deinit;
    enc28j60->parent.set_mediator = enc28j60_set_mediator;
#if USE_ESP_IDF && (ESP_IDF_VERSION_MAJOR >= 5)
    enc28j60->parent.autonego_ctrl = enc28j60_autonego_ctrl;
#else
    enc28j60->parent.negotiate = enc28j60_negotiate;
#endif
    enc28j60->parent.get_link = enc28j60_get_link;
    enc28j60->parent.pwrctl = enc28j60_pwrctl;
    enc28j60->parent.get_addr = enc28j60_get_addr;
    enc28j60->parent.set_addr = enc28j60_set_addr;
    enc28j60->parent.del = enc28j60_del;
    return &(enc28j60->parent);
err:
    return NULL;
}


static inline bool enc28j60_spi_lock(emac_enc28j60_t *emac)
{
    return xSemaphoreTake(emac->spi_lock, pdMS_TO_TICKS(ENC28J60_SPI_LOCK_TIMEOUT_MS)) == pdTRUE;
}

static inline bool enc28j60_spi_unlock(emac_enc28j60_t *emac)
{
    return xSemaphoreGive(emac->spi_lock) == pdTRUE;
}

static inline bool enc28j60_reg_trans_lock(emac_enc28j60_t *emac)
{
    return xSemaphoreTake(emac->reg_trans_lock, pdMS_TO_TICKS(ENC28J60_REG_TRANS_LOCK_TIMEOUT_MS)) == pdTRUE;
}

static inline bool enc28j60_reg_trans_unlock(emac_enc28j60_t *emac)
{
    return xSemaphoreGive(emac->reg_trans_lock) == pdTRUE;
}

/**
 * @brief ERXRDPT need to be set always at odd addresses
 */
static inline uint32_t enc28j60_next_ptr_align_odd(uint32_t next_packet_ptr, uint32_t start, uint32_t end)
{
    uint32_t erxrdpt;

    if ((next_packet_ptr - 1 < start) || (next_packet_ptr - 1 > end)) {
        erxrdpt = end;
    } else {
        erxrdpt = next_packet_ptr - 1;
    }

    return erxrdpt;
}

/**
 * @brief Calculate wrap around when reading beyond the end of the RX buffer
 */
static inline uint32_t enc28j60_rx_packet_start(uint32_t start_addr, uint32_t off)
{
    if (start_addr + off > ENC28J60_BUF_RX_END) {
        return (start_addr + off) - (ENC28J60_BUF_RX_END - ENC28J60_BUF_RX_START + 1);
    } else {
        return start_addr + off;
    }
}

/**
 * @brief SPI operation wrapper for writing ENC28J60 internal register
 */
static esp_err_t enc28j60_do_register_write(emac_enc28j60_t *emac, uint8_t reg_addr, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t trans = {
        .cmd = ENC28J60_SPI_CMD_WCR, // Write control register
        .addr = reg_addr,
        .length = 8,
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_data = {
            [0] = value
        }
    };
    if (enc28j60_spi_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        }
        enc28j60_spi_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

/**
 * @brief SPI operation wrapper for reading ENC28J60 internal register
 */
static esp_err_t enc28j60_do_register_read(emac_enc28j60_t *emac, bool is_eth_reg, uint8_t reg_addr, uint8_t *value)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t trans = {
        .cmd = ENC28J60_SPI_CMD_RCR, // Read control register
        .addr = reg_addr,
        .length = is_eth_reg ? 8 : 16, // read operation is different for ETH register and non-ETH register
        .flags = SPI_TRANS_USE_RXDATA
    };
    if (enc28j60_spi_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        } else {
            *value = is_eth_reg ? trans.rx_data[0] : trans.rx_data[1];
        }
        enc28j60_spi_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

/**
 * @brief SPI operation wrapper for bitwise setting ENC28J60 internal register
 * @note can only be used for ETH registers
 */
static esp_err_t enc28j60_do_bitwise_set(emac_enc28j60_t *emac, uint8_t reg_addr, uint8_t mask)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t trans = {
        .cmd = ENC28J60_SPI_CMD_BFS, // Bit field set
        .addr = reg_addr,
        .length = 8,
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_data = {
            [0] = mask
        }
    };
    if (enc28j60_spi_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        }
        enc28j60_spi_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

/**
 * @brief SPI operation wrapper for bitwise clearing ENC28J60 internal register
 * @note can only be used for ETH registers
 */
static esp_err_t enc28j60_do_bitwise_clr(emac_enc28j60_t *emac, uint8_t reg_addr, uint8_t mask)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t trans = {
        .cmd = ENC28J60_SPI_CMD_BFC, // Bit field clear
        .addr = reg_addr,
        .length = 8,
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_data = {
            [0] = mask
        }
    };
    if (enc28j60_spi_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        }
        enc28j60_spi_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

/**
 * @brief SPI operation wrapper for writing ENC28J60 internal memory
 */
static esp_err_t enc28j60_do_memory_write(emac_enc28j60_t *emac, uint8_t *buffer, uint32_t len)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t trans = {
        .cmd = ENC28J60_SPI_CMD_WBM, // Write buffer memory
        .addr = 0x1A,
        .length = len * 8,
        .tx_buffer = buffer
    };
    if (enc28j60_spi_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        }
        enc28j60_spi_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

/**
 * @brief SPI operation wrapper for reading ENC28J60 internal memory
 */
static esp_err_t enc28j60_do_memory_read(emac_enc28j60_t *emac, uint8_t *buffer, uint32_t len)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t trans = {
        .cmd = ENC28J60_SPI_CMD_RBM, // Read buffer memory
        .addr = 0x1A,
        .length = len * 8,
        .rx_buffer = buffer
    };

    if (enc28j60_spi_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG, "%s(%d): spi transmit failed, wanted to read %d", __FUNCTION__, __LINE__, trans.length);
            ret = ESP_FAIL;
        }
        enc28j60_spi_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

/**
 * @brief SPI operation wrapper for resetting ENC28J60
 */
static esp_err_t enc28j60_do_reset(emac_enc28j60_t *emac)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t trans = {
        .cmd = ENC28J60_SPI_CMD_SRC, // Soft reset
        .addr = 0x1F,
    };
    if (enc28j60_spi_lock(emac)) {
        if (spi_device_polling_transmit(emac->spi_hdl, &trans) != ESP_OK) {
            ESP_LOGE(TAG, "%s(%d): spi transmit failed", __FUNCTION__, __LINE__);
            ret = ESP_FAIL;
        }
        enc28j60_spi_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }

    // After reset, wait at least 1ms for the device to be ready
    esp_rom_delay_us(ENC28J60_SYSTEM_RESET_ADDITION_TIME_US);

    return ret;
}

/**
 * @brief Switch ENC28J60 register bank
 */
static esp_err_t enc28j60_switch_register_bank(emac_enc28j60_t *emac, uint8_t bank)
{
    esp_err_t ret = ESP_OK;
    if (bank != emac->last_bank) {
        MAC_CHECK(enc28j60_do_bitwise_clr(emac, ENC28J60_ECON1, 0x03) == ESP_OK,
                  "clear ECON1[1:0] failed", out, ESP_FAIL);
        MAC_CHECK(enc28j60_do_bitwise_set(emac, ENC28J60_ECON1, bank & 0x03) == ESP_OK,
                  "set ECON1[1:0] failed", out, ESP_FAIL);
        emac->last_bank = bank;
    }
out:
    return ret;
}

/**
 * @brief Write ENC28J60 register
 */
static esp_err_t enc28j60_register_write(emac_enc28j60_t *emac, uint16_t reg_addr, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    if (enc28j60_reg_trans_lock(emac)) {
        MAC_CHECK(enc28j60_switch_register_bank(emac, (reg_addr & 0xF00) >> 8) == ESP_OK,
                "switch bank failed", out, ESP_FAIL);
        MAC_CHECK(enc28j60_do_register_write(emac, reg_addr & 0xFF, value) == ESP_OK,
                "write register failed", out, ESP_FAIL);
        enc28j60_reg_trans_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
out:
    enc28j60_reg_trans_unlock(emac);
    return ret;
}

/**
 * @brief Read ENC28J60 register
 */
static esp_err_t enc28j60_register_read(emac_enc28j60_t *emac, uint16_t reg_addr, uint8_t *value)
{
    esp_err_t ret = ESP_OK;
    if (enc28j60_reg_trans_lock(emac)) {
        MAC_CHECK(enc28j60_switch_register_bank(emac, (reg_addr & 0xF00) >> 8) == ESP_OK,
                "switch bank failed", out, ESP_FAIL);
        MAC_CHECK(enc28j60_do_register_read(emac, !(reg_addr & 0xF000), reg_addr & 0xFF, value) == ESP_OK,
                "read register failed", out, ESP_FAIL);
        enc28j60_reg_trans_unlock(emac);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
out:
    enc28j60_reg_trans_unlock(emac);
    return ret;
}

/**
 * @brief Read ENC28J60 internal memroy
 */
static esp_err_t enc28j60_read_packet(emac_enc28j60_t *emac, uint32_t addr, uint8_t *packet, uint32_t len)
{
    esp_err_t ret = ESP_OK;
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERDPTL, addr & 0xFF) == ESP_OK,
              "write ERDPTL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERDPTH, (addr & 0xFF00) >> 8) == ESP_OK,
              "write ERDPTH failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_do_memory_read(emac, packet, len) == ESP_OK,
              "read memory failed", out, ESP_FAIL);
out:
    return ret;
}

/**
 * @brief Write ENC28J60 internal PHY register
 */
static esp_err_t emac_enc28j60_write_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr,
        uint32_t phy_reg, uint32_t reg_value)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    uint8_t mii_status;

    /* check if phy access is in progress */
    MAC_CHECK(enc28j60_register_read(emac, ENC28J60_MISTAT, &mii_status) == ESP_OK,
              "read MISTAT failed", out, ESP_FAIL);
    MAC_CHECK(!(mii_status & MISTAT_BUSY), "phy is busy", out, ESP_ERR_INVALID_STATE);

    /* tell the PHY address to write */
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MIREGADR, phy_reg & 0xFF) == ESP_OK,
              "write MIREGADR failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MIWRL, reg_value & 0xFF) == ESP_OK,
              "write MIWRL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MIWRH, (reg_value & 0xFF00) >> 8) == ESP_OK,
              "write MIWRH failed", out, ESP_FAIL);

    /* polling the busy flag */
    uint32_t to = 0;
    do {
        esp_rom_delay_us(15);
        MAC_CHECK(enc28j60_register_read(emac, ENC28J60_MISTAT, &mii_status) == ESP_OK,
                  "read MISTAT failed", out, ESP_FAIL);
        to += 15;
    } while ((mii_status & MISTAT_BUSY) && to < ENC28J60_PHY_OPERATION_TIMEOUT_US);
    MAC_CHECK(!(mii_status & MISTAT_BUSY), "phy is busy", out, ESP_ERR_TIMEOUT);
out:
    return ret;
}

/**
 * @brief Read ENC28J60 internal PHY register
 */
static esp_err_t emac_enc28j60_read_phy_reg(esp_eth_mac_t *mac, uint32_t phy_addr,
        uint32_t phy_reg, uint32_t *reg_value)
{
    esp_err_t ret = ESP_OK;
    MAC_CHECK(reg_value, "can't set reg_value to null", out, ESP_ERR_INVALID_ARG);
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    uint8_t mii_status;
    uint8_t mii_cmd;

    /* check if phy access is in progress */
    MAC_CHECK(enc28j60_register_read(emac, ENC28J60_MISTAT, &mii_status) == ESP_OK,
              "read MISTAT failed", out, ESP_FAIL);
    MAC_CHECK(!(mii_status & MISTAT_BUSY), "phy is busy", out, ESP_ERR_INVALID_STATE);

    /* tell the PHY address to read */
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MIREGADR, phy_reg & 0xFF) == ESP_OK,
              "write MIREGADR failed", out, ESP_FAIL);
    mii_cmd = MICMD_MIIRD;
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MICMD, mii_cmd) == ESP_OK,
              "write MICMD failed", out, ESP_FAIL);

    /* polling the busy flag */
    uint32_t to = 0;
    do {
        esp_rom_delay_us(15);
        MAC_CHECK(enc28j60_register_read(emac, ENC28J60_MISTAT, &mii_status) == ESP_OK,
                  "read MISTAT failed", out, ESP_FAIL);
        to += 15;
    } while ((mii_status & MISTAT_BUSY) && to < ENC28J60_PHY_OPERATION_TIMEOUT_US);
    MAC_CHECK(!(mii_status & MISTAT_BUSY), "phy is busy", out, ESP_ERR_TIMEOUT);

    mii_cmd &= (~MICMD_MIIRD);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MICMD, mii_cmd) == ESP_OK,
              "write MICMD failed", out, ESP_FAIL);

    uint8_t value_l = 0;
    uint8_t value_h = 0;
    MAC_CHECK(enc28j60_register_read(emac, ENC28J60_MIRDL, &value_l) == ESP_OK,
              "read MIRDL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_read(emac, ENC28J60_MIRDH, &value_h) == ESP_OK,
              "read MIRDH failed", out, ESP_FAIL);
    *reg_value = (value_h << 8) | value_l;
out:
    return ret;
}

/**
 * @brief Set mediator for Ethernet MAC
 */
static esp_err_t emac_enc28j60_set_mediator(esp_eth_mac_t *mac, esp_eth_mediator_t *eth)
{
    esp_err_t ret = ESP_OK;
    MAC_CHECK(eth, "can't set mac's mediator to null", out, ESP_ERR_INVALID_ARG);
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    emac->eth = eth;
out:
    return ret;
}

/**
 * @brief Verify chip revision ID
 */
static esp_err_t enc28j60_verify_id(emac_enc28j60_t *emac)
{
    esp_err_t ret = ESP_OK;
    MAC_CHECK(enc28j60_register_read(emac, ENC28J60_EREVID, (uint8_t *)&emac->revision) == ESP_OK,
              "read EREVID failed", out, ESP_FAIL);
    ESP_LOGI(TAG, "revision: %d", emac->revision);
    MAC_CHECK(emac->revision >= ENC28J60_REV_B1 && emac->revision <= ENC28J60_REV_B7, "wrong chip ID", out, ESP_ERR_INVALID_VERSION);
out:
    return ret;
}

/**
 * @brief Write mac address to internal registers
 */
static esp_err_t enc28j60_set_mac_addr(emac_enc28j60_t *emac)
{
    esp_err_t ret = ESP_OK;

    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAADR6, emac->addr[5]) == ESP_OK,
              "write MAADR6 failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAADR5, emac->addr[4]) == ESP_OK,
              "write MAADR5 failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAADR4, emac->addr[3]) == ESP_OK,
              "write MAADR4 failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAADR3, emac->addr[2]) == ESP_OK,
              "write MAADR3 failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAADR2, emac->addr[1]) == ESP_OK,
              "write MAADR2 failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAADR1, emac->addr[0]) == ESP_OK,
              "write MAADR1 failed", out, ESP_FAIL);
out:
    return ret;
}

/**
 * @brief   Clear multicast hash table
 */
static esp_err_t enc28j60_clear_multicast_table(emac_enc28j60_t *emac)
{
    esp_err_t ret = ESP_OK;

    for (int i = 0; i < 7; i++) {
        MAC_CHECK(enc28j60_register_write(emac, ENC28J60_EHT0 + i, 0x00) == ESP_OK,
                  "write ENC28J60_EHT%d failed", out, ESP_FAIL, i);
    }
out:
    return ret;
}

/**
 * @brief Default setup for ENC28J60 internal registers
 */
static esp_err_t enc28j60_setup_default(emac_enc28j60_t *emac)
{
    esp_err_t ret = ESP_OK;

    // set up receive buffer start + end
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXSTL, ENC28J60_BUF_RX_START & 0xFF) == ESP_OK,
              "write ERXSTL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXSTH, (ENC28J60_BUF_RX_START & 0xFF00) >> 8) == ESP_OK,
              "write ERXSTH failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXNDL, ENC28J60_BUF_RX_END & 0xFF) == ESP_OK,
              "write ERXNDL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXNDH, (ENC28J60_BUF_RX_END & 0xFF00) >> 8) == ESP_OK,
              "write ERXNDH failed", out, ESP_FAIL);
    uint32_t erxrdpt = enc28j60_next_ptr_align_odd(ENC28J60_BUF_RX_START, ENC28J60_BUF_RX_START, ENC28J60_BUF_RX_END);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXRDPTL, erxrdpt & 0xFF) == ESP_OK,
              "write ERXRDPTL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXRDPTH, (erxrdpt & 0xFF00) >> 8) == ESP_OK,
              "write ERXRDPTH failed", out, ESP_FAIL);

    // set up transmit buffer start + end
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ETXSTL, ENC28J60_BUF_TX_START & 0xFF) == ESP_OK,
              "write ETXSTL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ETXSTH, (ENC28J60_BUF_TX_START & 0xFF00) >> 8) == ESP_OK,
              "write ETXSTH failed", out, ESP_FAIL);

    // set up default filter mode: (unicast OR broadcast) AND crc valid
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN) == ESP_OK,
              "write ERXFCON failed", out, ESP_FAIL);

    // enable MAC receive, enable pause control frame on Tx and Rx path
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MACON1, MACON1_MARXEN | MACON1_RXPAUS | MACON1_TXPAUS) == ESP_OK,
              "write MACON1 failed", out, ESP_FAIL);
    // enable automatic padding, append CRC, check frame length, half duplex by default (can update at runtime)
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN) == ESP_OK, "write MACON3 failed", out, ESP_FAIL);
    // enable defer transmission (effective only in half duplex)
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MACON4, MACON4_DEFER) == ESP_OK,
              "write MACON4 failed", out, ESP_FAIL);
    // set inter-frame gap (back-to-back)
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MABBIPG, 0x12) == ESP_OK,
              "write MABBIPG failed", out, ESP_FAIL);
    // set inter-frame gap (non-back-to-back)
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAIPGL, 0x12) == ESP_OK,
              "write MAIPGL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MAIPGH, 0x0C) == ESP_OK,
              "write MAIPGH failed", out, ESP_FAIL);

out:
    return ret;
}

/**
 * @brief Start enc28j60: enable interrupt and start receive
 */
static esp_err_t emac_enc28j60_start(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    /* enable interrupt */
    MAC_CHECK(enc28j60_do_bitwise_clr(emac, ENC28J60_EIR, 0xFF) == ESP_OK,
              "clear EIR failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_do_bitwise_set(emac, ENC28J60_EIE, EIE_PKTIE | EIE_INTIE | EIE_TXERIE) == ESP_OK,
              "set EIE.[PKTIE|INTIE] failed", out, ESP_FAIL);
    /* enable rx logic */
    MAC_CHECK(enc28j60_do_bitwise_set(emac, ENC28J60_ECON1, ECON1_RXEN) == ESP_OK,
              "set ECON1.RXEN failed", out, ESP_FAIL);

    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERDPTL, 0x00) == ESP_OK,
              "write ERDPTL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERDPTH, 0x00) == ESP_OK,
              "write ERDPTH failed", out, ESP_FAIL);
out:
    return ret;
}

/**
 * @brief   Stop enc28j60: disable interrupt and stop receiving packets
 */
static esp_err_t emac_enc28j60_stop(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    /* disable interrupt */
    MAC_CHECK(enc28j60_do_bitwise_clr(emac, ENC28J60_EIE, 0xFF) == ESP_OK,
              "clear EIE failed", out, ESP_FAIL);
    /* disable rx */
    MAC_CHECK(enc28j60_do_bitwise_clr(emac, ENC28J60_ECON1, ECON1_RXEN) == ESP_OK,
              "clear ECON1.RXEN failed", out, ESP_FAIL);
out:
    return ret;
}

static esp_err_t emac_enc28j60_set_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    MAC_CHECK(addr, "can't set mac addr to null", out, ESP_ERR_INVALID_ARG);
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    memcpy(emac->addr, addr, 6);
    MAC_CHECK(enc28j60_set_mac_addr(emac) == ESP_OK, "set mac address failed", out, ESP_FAIL);
out:
    return ret;
}

static esp_err_t emac_enc28j60_get_addr(esp_eth_mac_t *mac, uint8_t *addr)
{
    esp_err_t ret = ESP_OK;
    MAC_CHECK(addr, "can't set mac addr to null", out, ESP_ERR_INVALID_ARG);
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    memcpy(addr, emac->addr, 6);
out:
    return ret;
}

static inline esp_err_t emac_enc28j60_get_tsv(emac_enc28j60_t *emac, enc28j60_tsv_t *tsv)
{
    return enc28j60_read_packet(emac, emac->last_tsv_addr, (uint8_t *)tsv, ENC28J60_TSV_SIZE);
}

static void enc28j60_isr_handler(void *arg)
{
    emac_enc28j60_t *emac = (emac_enc28j60_t *)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    /* notify enc28j60 task */
    vTaskNotifyGiveFromISR(emac->rx_task_hdl, &high_task_wakeup);
    if (high_task_wakeup != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Main ENC28J60 Task. Mainly used for Rx processing. However, it also handles other interrupts.
 *
 */
static void emac_enc28j60_task(void *arg)
{
    emac_enc28j60_t *emac = (emac_enc28j60_t *)arg;
    uint8_t status = 0;
    uint8_t mask = 0;
    uint8_t *buffer = NULL;
    uint32_t length = 0;

    while (1) {
loop_start:
        // block until some task notifies me or check the gpio by myself
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0 &&    // if no notification ...
            gpio_get_level(emac->int_gpio_num) != 0) {               // ...and no interrupt asserted
            continue;                                                // -> just continue to check again
        }
        // the host controller should clear the global enable bit for the interrupt pin before servicing the interrupt
        MAC_CHECK_NO_RET(enc28j60_do_bitwise_clr(emac, ENC28J60_EIR, EIE_INTIE) == ESP_OK,
                        "clear EIE_INTIE failed", loop_start);
        // read interrupt status
        MAC_CHECK_NO_RET(enc28j60_do_register_read(emac, true, ENC28J60_EIR, &status) == ESP_OK,
                        "read EIR failed", loop_end);
        MAC_CHECK_NO_RET(enc28j60_do_register_read(emac, true, ENC28J60_EIE, &mask) == ESP_OK,
                        "read EIE failed", loop_end);
        status &= mask;

        // When source of interrupt is unknown, try to check if there is packet waiting (Errata #6 workaround)
        if (status == 0) {
            uint8_t pk_counter;
            MAC_CHECK_NO_RET(enc28j60_register_read(emac, ENC28J60_EPKTCNT, &pk_counter) == ESP_OK,
                            "read EPKTCNT failed", loop_end);
            if (pk_counter > 0) {
                status = EIR_PKTIF;
            } else {
                goto loop_end;
            }
        }

        // packet received
        if (status & EIR_PKTIF) {
            do {
                length = ETH_MAX_PACKET_SIZE;
                buffer = heap_caps_malloc(length, MALLOC_CAP_DMA);
                if (!buffer) {
                    ESP_LOGE(TAG, "no mem for receive buffer");
                } else if (emac->parent.receive(&emac->parent, buffer, &length) == ESP_OK) {
                    /* pass the buffer to stack (e.g. TCP/IP layer) */
                    if (length) {
                        emac->eth->stack_input(emac->eth, buffer, length);
                    } else {
                        free(buffer);
                    }
                } else {
                    free(buffer);
                }
            } while (emac->packets_remain);
        }

        // transmit error
        if (status & EIR_TXERIF) {
            // Errata #12/#13 workaround - reset Tx state machine
            MAC_CHECK_NO_RET(enc28j60_do_bitwise_set(emac, ENC28J60_ECON1, ECON1_TXRST) == ESP_OK,
                            "set TXRST failed", loop_end);
            MAC_CHECK_NO_RET(enc28j60_do_bitwise_clr(emac, ENC28J60_ECON1, ECON1_TXRST) == ESP_OK,
                            "clear TXRST failed", loop_end);

            // Clear Tx Error Interrupt Flag
            MAC_CHECK_NO_RET(enc28j60_do_bitwise_clr(emac, ENC28J60_EIR, EIR_TXERIF) == ESP_OK,
                            "clear TXERIF failed", loop_end);

            // Errata #13 workaround (applicable only to B5 and B7 revisions)
            if (emac->revision == ENC28J60_REV_B5 || emac->revision == ENC28J60_REV_B7) {
                __attribute__((aligned(4))) enc28j60_tsv_t tx_status; // SPI driver needs the rx buffer 4 byte align
                MAC_CHECK_NO_RET(emac_enc28j60_get_tsv(emac, &tx_status) == ESP_OK,
                            "get Tx Status Vector failed", loop_end);
                // Try to retransmit when late collision is indicated
                if (tx_status.late_collision) {
                    // Clear Tx Interrupt status Flag (it was set along with the error)
                    MAC_CHECK_NO_RET(enc28j60_do_bitwise_clr(emac, ENC28J60_EIR, EIR_TXIF) == ESP_OK,
                                    "clear TXIF failed", loop_end);
                    // Enable global interrupt flag and try to retransmit
                    MAC_CHECK_NO_RET(enc28j60_do_bitwise_set(emac, ENC28J60_EIE, EIE_INTIE) == ESP_OK,
                                    "set INTIE failed", loop_end);
                    MAC_CHECK_NO_RET(enc28j60_do_bitwise_set(emac, ENC28J60_ECON1, ECON1_TXRTS) == ESP_OK,
                                    "set TXRTS failed", loop_end);
                    continue; // no need to handle Tx ready interrupt nor to enable global interrupt at this point
                }
            }
        }

        // transmit ready
        if (status & EIR_TXIF) {
            MAC_CHECK_NO_RET(enc28j60_do_bitwise_clr(emac, ENC28J60_EIR, EIR_TXIF) == ESP_OK,
                            "clear TXIF failed", loop_end);
            MAC_CHECK_NO_RET(enc28j60_do_bitwise_clr(emac, ENC28J60_EIE, EIE_TXIE) == ESP_OK,
                            "clear TXIE failed", loop_end);

            xSemaphoreGive(emac->tx_ready_sem);
        }
loop_end:
        // restore global enable interrupt bit
        MAC_CHECK_NO_RET(enc28j60_do_bitwise_set(emac, ENC28J60_EIE, EIE_INTIE) == ESP_OK,
                            "clear INTIE failed", loop_start);
        // Note: Interrupt flag PKTIF is cleared when PKTDEC is set (in receive function)
    }
    vTaskDelete(NULL);
}

static esp_err_t emac_enc28j60_set_link(esp_eth_mac_t *mac, eth_link_t link)
{
    esp_err_t ret = ESP_OK;
    switch (link) {
    case ETH_LINK_UP:
        MAC_CHECK(mac->start(mac) == ESP_OK, "enc28j60 start failed", out, ESP_FAIL);
        break;
    case ETH_LINK_DOWN:
        MAC_CHECK(mac->stop(mac) == ESP_OK, "enc28j60 stop failed", out, ESP_FAIL);
        break;
    default:
        MAC_CHECK(false, "unknown link status", out, ESP_ERR_INVALID_ARG);
        break;
    }
out:
    return ret;
}

static esp_err_t emac_enc28j60_set_speed(esp_eth_mac_t *mac, eth_speed_t speed)
{
    esp_err_t ret = ESP_OK;
    switch (speed) {
    case ETH_SPEED_10M:
        ESP_LOGI(TAG, "working in 10Mbps");
        break;
    default:
        MAC_CHECK(false, "100Mbps unsupported", out, ESP_ERR_NOT_SUPPORTED);
        break;
    }
out:
    return ret;
}

static esp_err_t emac_enc28j60_set_duplex(esp_eth_mac_t *mac, eth_duplex_t duplex)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    uint8_t mac3 = 0;
    MAC_CHECK(enc28j60_register_read(emac, ENC28J60_MACON3, &mac3) == ESP_OK,
              "read MACON3 failed", out, ESP_FAIL);
    switch (duplex) {
    case ETH_DUPLEX_HALF:
        mac3 &= ~MACON3_FULDPX;
        MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MABBIPG, 0x12) == ESP_OK,
                  "write MABBIPG failed", out, ESP_FAIL);
        ESP_LOGI(TAG, "working in half duplex");
        break;
    case ETH_DUPLEX_FULL:
        mac3 |= MACON3_FULDPX;
        MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MABBIPG, 0x15) == ESP_OK,
                  "write MABBIPG failed", out, ESP_FAIL);
        ESP_LOGI(TAG, "working in full duplex");
        break;
    default:
        MAC_CHECK(false, "unknown duplex", out, ESP_ERR_INVALID_ARG);
        break;
    }
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_MACON3, mac3) == ESP_OK,
              "write MACON3 failed", out, ESP_FAIL);
out:
    return ret;
}

static esp_err_t emac_enc28j60_set_promiscuous(esp_eth_mac_t *mac, bool enable)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    if (enable) {
        MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXFCON, 0x00) == ESP_OK,
                  "write ERXFCON failed", out, ESP_FAIL);
    }
out:
    return ret;
}

static esp_err_t emac_enc28j60_transmit(esp_eth_mac_t *mac, uint8_t *buf, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    uint8_t econ1 = 0;

    /* ENC28J60 may be a bottle neck in Eth communication. Hence we need to check if it is ready. */
    if (xSemaphoreTake(emac->tx_ready_sem, pdMS_TO_TICKS(ENC28J60_TX_READY_TIMEOUT_MS)) == pdFALSE) {
        ESP_LOGW(TAG, "tx_ready_sem expired");
    }
    MAC_CHECK(enc28j60_do_register_read(emac, true, ENC28J60_ECON1, &econ1) == ESP_OK,
            "read ECON1 failed", out, ESP_FAIL);
    MAC_CHECK(!(econ1 & ECON1_TXRTS), "last transmit still in progress", out, ESP_ERR_INVALID_STATE);

    /* Set the write pointer to start of transmit buffer area */
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_EWRPTL, ENC28J60_BUF_TX_START & 0xFF) == ESP_OK,
              "write EWRPTL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_EWRPTH, (ENC28J60_BUF_TX_START & 0xFF00) >> 8) == ESP_OK,
              "write EWRPTH failed", out, ESP_FAIL);

    /* Set the end pointer to correspond to the packet size given */
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ETXNDL, (ENC28J60_BUF_TX_START + length) & 0xFF) == ESP_OK,
              "write ETXNDL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ETXNDH, ((ENC28J60_BUF_TX_START + length) & 0xFF00) >> 8) == ESP_OK,
              "write ETXNDH failed", out, ESP_FAIL);

    /* copy data to tx memory */
    uint8_t per_pkt_control = 0; // MACON3 will be used to determine how the packet will be transmitted
    MAC_CHECK(enc28j60_do_memory_write(emac, &per_pkt_control, 1) == ESP_OK,
              "write packet control byte failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_do_memory_write(emac, buf, length) == ESP_OK,
              "buffer memory write failed", out, ESP_FAIL);
    emac->last_tsv_addr = ENC28J60_BUF_TX_START + length + 1;

    /* enable Tx Interrupt to indicate next Tx ready state */
    MAC_CHECK(enc28j60_do_bitwise_clr(emac, ENC28J60_EIR, EIR_TXIF) == ESP_OK,
                "set EIR_TXIF failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_do_bitwise_set(emac, ENC28J60_EIE, EIE_TXIE) == ESP_OK,
                "set EIE_TXIE failed", out, ESP_FAIL);

    /* issue tx polling command */
    MAC_CHECK(enc28j60_do_bitwise_set(emac, ENC28J60_ECON1, ECON1_TXRTS) == ESP_OK,
              "set ECON1.TXRTS failed", out, ESP_FAIL);
out:
    return ret;
}

static esp_err_t emac_enc28j60_receive(esp_eth_mac_t *mac, uint8_t *buf, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    uint8_t pk_counter = 0;
    uint16_t rx_len = 0;
    uint32_t next_packet_addr = 0;
    __attribute__((aligned(4))) enc28j60_rx_header_t header; // SPI driver needs the rx buffer 4 byte align

    // read packet header
    MAC_CHECK(enc28j60_read_packet(emac, emac->next_packet_ptr, (uint8_t *)&header, sizeof(header)) == ESP_OK,
              "read header failed", out, ESP_FAIL);

    // get packets' length, address
    rx_len = header.length_low + (header.length_high << 8);
    MAC_CHECK(rx_len <= *length,
              "read packet size %d would overlap given buffer size of %"PRIu32"", out, ESP_FAIL, rx_len, *length);

    next_packet_addr = header.next_packet_low + (header.next_packet_high << 8);

    // read packet content
    MAC_CHECK(enc28j60_read_packet(emac, enc28j60_rx_packet_start(emac->next_packet_ptr, ENC28J60_RSV_SIZE), buf, rx_len) == ESP_OK,
              "read packet content failed, wanted to read %d bytes, total %"PRIu32"", out, ESP_FAIL, rx_len, *length);

    // free receive buffer space
    uint32_t erxrdpt = enc28j60_next_ptr_align_odd(next_packet_addr, ENC28J60_BUF_RX_START, ENC28J60_BUF_RX_END);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXRDPTL, (erxrdpt & 0xFF)) == ESP_OK,
              "write ERXRDPTL failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_write(emac, ENC28J60_ERXRDPTH, (erxrdpt & 0xFF00) >> 8) == ESP_OK,
              "write ERXRDPTH failed", out, ESP_FAIL);
    emac->next_packet_ptr = next_packet_addr;

    MAC_CHECK(enc28j60_do_bitwise_set(emac, ENC28J60_ECON2, ECON2_PKTDEC) == ESP_OK,
              "set ECON2.PKTDEC failed", out, ESP_FAIL);
    MAC_CHECK(enc28j60_register_read(emac, ENC28J60_EPKTCNT, &pk_counter) == ESP_OK,
              "read EPKTCNT failed", out, ESP_FAIL);

    *length = rx_len - 4; // substract the CRC length
    emac->packets_remain = pk_counter > 0;
out:
    if (ret != ESP_OK)
        vTaskDelay(pdMS_TO_TICKS(100));
    return ret;
}

/**
 * @brief Get chip info
 */
int emac_enc28j60_get_chip_info(esp_eth_mac_t *mac)
{
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    return emac->revision;
}

static esp_err_t emac_enc28j60_init(esp_eth_mac_t *mac)
{
    esp_err_t ret = ESP_OK;
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    esp_eth_mediator_t *eth = emac->eth;

    /* init gpio used for reporting enc28j60 interrupt */
    gpio_reset_pin(emac->int_gpio_num);
    gpio_set_direction(emac->int_gpio_num, GPIO_MODE_INPUT);
    gpio_set_pull_mode(emac->int_gpio_num, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(emac->int_gpio_num, GPIO_INTR_NEGEDGE);
    gpio_intr_enable(emac->int_gpio_num);
    gpio_isr_handler_add(emac->int_gpio_num, enc28j60_isr_handler, emac);
    MAC_CHECK(eth->on_state_changed(eth, ETH_STATE_LLINIT, NULL) == ESP_OK,
              "lowlevel init failed", out, ESP_FAIL);

    /* reset enc28j60 */
    MAC_CHECK(enc28j60_do_reset(emac) == ESP_OK, "reset enc28j60 failed", out, ESP_FAIL);
    /* verify chip id */
    MAC_CHECK(enc28j60_verify_id(emac) == ESP_OK, "vefiry chip ID failed", out, ESP_FAIL);
    /* default setup of internal registers */
    MAC_CHECK(enc28j60_setup_default(emac) == ESP_OK, "enc28j60 default setup failed", out, ESP_FAIL);
    /* clear multicast hash table */
    MAC_CHECK(enc28j60_clear_multicast_table(emac) == ESP_OK, "clear multicast table failed", out, ESP_FAIL);

    return ESP_OK;
out:
    gpio_isr_handler_remove(emac->int_gpio_num);
    gpio_reset_pin(emac->int_gpio_num);
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ret;
}

static esp_err_t emac_enc28j60_deinit(esp_eth_mac_t *mac)
{
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    esp_eth_mediator_t *eth = emac->eth;
    mac->stop(mac);
    gpio_isr_handler_remove(emac->int_gpio_num);
    gpio_reset_pin(emac->int_gpio_num);
    eth->on_state_changed(eth, ETH_STATE_DEINIT, NULL);
    return ESP_OK;
}

static esp_err_t emac_enc28j60_del(esp_eth_mac_t *mac)
{
    emac_enc28j60_t *emac = __containerof(mac, emac_enc28j60_t, parent);
    vTaskDelete(emac->rx_task_hdl);
    vSemaphoreDelete(emac->spi_lock);
    vSemaphoreDelete(emac->reg_trans_lock);
    vSemaphoreDelete(emac->tx_ready_sem);
    free(emac);
    return ESP_OK;
}
esp_eth_mac_t *esp_eth_mac_new_enc28j60(const spi_device_handle_t spi_hdl, const int int_gpio, const eth_mac_config_t *mac_config)
{
    esp_eth_mac_t *ret = NULL;
    emac_enc28j60_t *emac = NULL;
    MAC_CHECK(spi_hdl, "can't set spi_hdl to null", err, NULL);
    /* enc28j60 driver is interrupt driven */
    MAC_CHECK(int_gpio >= 0, "error interrupt gpio number", err, NULL);

    MAC_CHECK(mac_config, "can't set mac config to null", err, NULL);
    emac = calloc(1, sizeof(emac_enc28j60_t));
    MAC_CHECK(emac, "calloc emac failed", err, NULL);
    emac->last_bank = 0xFF;
    emac->next_packet_ptr = ENC28J60_BUF_RX_START;
    /* bind methods and attributes */
    emac->sw_reset_timeout_ms = mac_config->sw_reset_timeout_ms;
    emac->int_gpio_num = int_gpio;
    emac->spi_hdl = spi_hdl;
    emac->parent.set_mediator = emac_enc28j60_set_mediator;
    emac->parent.init = emac_enc28j60_init;
    emac->parent.deinit = emac_enc28j60_deinit;
    emac->parent.start = emac_enc28j60_start;
    emac->parent.stop = emac_enc28j60_stop;
    emac->parent.del = emac_enc28j60_del;
    emac->parent.write_phy_reg = emac_enc28j60_write_phy_reg;
    emac->parent.read_phy_reg = emac_enc28j60_read_phy_reg;
    emac->parent.set_addr = emac_enc28j60_set_addr;
    emac->parent.get_addr = emac_enc28j60_get_addr;
    emac->parent.set_speed = emac_enc28j60_set_speed;
    emac->parent.set_duplex = emac_enc28j60_set_duplex;
    emac->parent.set_link = emac_enc28j60_set_link;
    emac->parent.set_promiscuous = emac_enc28j60_set_promiscuous;
    emac->parent.transmit = emac_enc28j60_transmit;
    emac->parent.receive = emac_enc28j60_receive;
    /* create mutex */
    emac->spi_lock = xSemaphoreCreateMutex();
    MAC_CHECK(emac->spi_lock, "create spi lock failed", err, NULL);
    emac->reg_trans_lock = xSemaphoreCreateMutex();
    MAC_CHECK(emac->reg_trans_lock, "create register transaction lock failed", err, NULL);
    emac->tx_ready_sem = xSemaphoreCreateBinary();
    MAC_CHECK(emac->tx_ready_sem, "create pkt transmit ready semaphore failed", err, NULL);
    xSemaphoreGive(emac->tx_ready_sem); // ensures the first transmit is performed without waiting
    /* create enc28j60 task */
    BaseType_t core_num = tskNO_AFFINITY;
    if (mac_config->flags & ETH_MAC_FLAG_PIN_TO_CORE) {
#if USE_ESP_IDF && (ESP_IDF_VERSION_MAJOR >= 5)
        core_num = esp_cpu_get_core_id();
#else
        core_num = cpu_hal_get_core_id();
#endif
    }
    BaseType_t xReturned = xTaskCreatePinnedToCore(emac_enc28j60_task, "enc28j60_tsk", mac_config->rx_task_stack_size, emac,
                           mac_config->rx_task_prio, &emac->rx_task_hdl, core_num);
    MAC_CHECK(xReturned == pdPASS, "create enc28j60 task failed", err, NULL);

    return &(emac->parent);
err:
    if (emac) {
        if (emac->rx_task_hdl) {
            vTaskDelete(emac->rx_task_hdl);
        }
        if (emac->spi_lock) {
            vSemaphoreDelete(emac->spi_lock);
        }
        if (emac->reg_trans_lock) {
            vSemaphoreDelete(emac->reg_trans_lock);
        }
        if (emac->tx_ready_sem) {
            vSemaphoreDelete(emac->tx_ready_sem);
        }
        free(emac);
    }

    return ret;
}

#endif
