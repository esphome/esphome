#pragma once
#include <cstdint>

// ST25R300 Register Map (AN6313, DS - 88 addressable registers 0x00-0x57)
// SPI protocol: write = addr & 0x7F (bit7=0), read = addr | 0x80 (bit7=1)
// FIFO write: 0x5F + data bytes; FIFO read: 0xDF + read bytes
// Commands: 0x60-0x7F range + 0xE2-0xEF range

namespace esphome {
namespace st25r300 {

// ── Register Addresses ──────────────────────────────────────────────────────
enum ST25R300Register : uint8_t {
  // Core operational registers
  ST25R300_REG_OPERATION = 0x00,       // en(b3), vdddr_en(b4), rx_en(b5), tx_en(b6)
  ST25R300_REG_GENERAL_CONF = 0x01,    // single(b5), rfo2(b4), nfc_n[1:0]
  ST25R300_REG_VDDDR_CONF = 0x02,      // reg_s(b7), rege[6:0]
  ST25R300_REG_TX_DRIVER_CONF = 0x03,  // regd[6:4], d_res[3:0]
  ST25R300_REG_TX_MOD1 = 0x04,         // am_mod[7:4](def=0x7=20%), mod_state(b3), rgs_am(b2), res_am(b1)

  // Rx analog settings
  ST25R300_REG_RX_ANALOG1 = 0x09,  // dig_clk_dly[7:4](rec=0x7), gain_boost(b2), hpf_ctrl[1:0]

  // Rx digital settings
  ST25R300_REG_RX_DIGITAL1 = 0x0D,  // agc_en(b7)=1 by default

  // Protocol registers
  ST25R300_REG_PROTOCOL1 = 0x14,     // om[3:0]: 0x1=NFC-A, 0x2=NFC-B, 0x3=NFC-F, 0x5=NFC-V
  ST25R300_REG_TX_PROTOCOL1 = 0x15,  // a_tx_par(b6)=1, tx_crc(b5)=1, tr_am(b4), p_len[3:0]
  ST25R300_REG_TX_PROTOCOL2 = 0x16,
  ST25R300_REG_RX_PROTOCOL1 = 0x17,  // a_rx_par(b3)=1, rx_crc(b2)=1, antcl(b0) for anticollision

  // No-response timer (NRT) config
  ST25R300_REG_NRT_CONF1 = 0x22,  // nrt_emv(b1), nrt_step(b0)
  ST25R300_REG_NRT_CONF2 = 0x23,  // nrt[15:8]
  ST25R300_REG_NRT_CONF3 = 0x24,  // nrt[7:0]

  // TX frame configuration
  ST25R300_REG_TX_FRAME1 = 0x34,  // ntx[12:5]
  ST25R300_REG_TX_FRAME2 = 0x35,  // ntx[4:0] in bits[7:3], nbtx[2:0] in bits[2:0]

  // FIFO status
  ST25R300_REG_FIFO_STATUS1 = 0x36,  // fifo_b[7:0] byte count LSB
  ST25R300_REG_FIFO_STATUS2 = 0x37,  // fifo_b[8](b6), fifo_unf(b5), fifo_ovr(b4), fifo_lb[2:0](b3:1), np_lb(b0)

  // Collision display
  ST25R300_REG_COLLISION_DISPLAY = 0x38,  // c_byte[7:4], c_bit[3:1], c_pb[0]

  // IRQ mask registers
  ST25R300_REG_IRQ_MASK1 = 0x39,  // M_col(b6), M_rxe(b3), M_rxs(b2), M_txe(b1), M_rx_err(b0)
  ST25R300_REG_IRQ_MASK2 = 0x3A,  // M_nre(b6)
  ST25R300_REG_IRQ_MASK3 = 0x3B,  // M_dct(b4), M_osc(b0)

  // IRQ status registers (read-to-clear)
  ST25R300_REG_IRQ_STATUS1 = 0x3C,  // I_col(b6), I_rxe(b3), I_rxs(b2), I_txe(b1), I_rx_err(b0)
  ST25R300_REG_IRQ_STATUS2 = 0x3D,  // I_nre(b6)
  ST25R300_REG_IRQ_STATUS3 = 0x3E,  // I_dct(b4), I_osc(b0)

  // IC identity
  ST25R300_REG_IC_IDENTITY = 0x3F,  // ic_type[7:3]=10110b for ST25R300 → (val & 0xF8)==0xB0

  // Status registers
  ST25R300_REG_STATUS1 = 0x40,  // osc_ok(b4), agd_ok(b5), efd_out(b1), efd_on(b0)
  ST25R300_REG_STATUS2 = 0x41,  // subc_on(b7), gpt_on(b6), nrt_on(b5), mrt_on(b4), rx_act(b3), rx_on(b2), tx_on(b1)
  ST25R300_REG_STATIC_STATUS1 = 0x42,  // s_eon(b5), s_eof(b6), s_dct(b4)
  ST25R300_REG_STATIC_STATUS2 = 0x43,  // s_rx_err(b5), s_rxe(b4), s_rx_rest(b3), s_col(b2), s_rxs(b1)
  ST25R300_REG_STATIC_STATUS3 = 0x44,  // s_crc(b7), s_par(b6), s_hfe(b5), s_sfe(b4)

  // RSSI / Field sense
  ST25R300_REG_RSSI1 = 0x4A,     // rssi_i[6:0] (I channel)
  ST25R300_REG_RSSI2 = 0x4B,     // rssi_q[6:0] (Q channel)
  ST25R300_REG_SENSE_RF = 0x4C,  // sense_adc[7:0] — updated by CMD_SENSE_RF (0x7C/7D)
};

// ── IRQ Status 1 (0x3C) bit masks ───────────────────────────────────────────
static const uint8_t ST25R300_IRQ1_SUBC_START = 0x80;  // bit7: subcarrier start detected
static const uint8_t ST25R300_IRQ1_COL = 0x40;         // bit6: bit collision
static const uint8_t ST25R300_IRQ1_WL = 0x20;          // bit5: FIFO water level
static const uint8_t ST25R300_IRQ1_RX_REST = 0x10;     // bit4: automatic reception restart
static const uint8_t ST25R300_IRQ1_RXE = 0x08;         // bit3: end of receive
static const uint8_t ST25R300_IRQ1_RXS = 0x04;         // bit2: start of receive
static const uint8_t ST25R300_IRQ1_TXE = 0x02;         // bit1: end of transmission
static const uint8_t ST25R300_IRQ1_RX_ERR = 0x01;      // bit0: receive error

// ── IRQ Status 2 (0x3D) bit masks ───────────────────────────────────────────
static const uint8_t ST25R300_IRQ2_NRE = 0x40;  // bit6: no-response timer expired

// ── IRQ Status 3 (0x3E) bit masks ───────────────────────────────────────────
static const uint8_t ST25R300_IRQ3_DCT = 0x10;  // bit4: direct command terminated
static const uint8_t ST25R300_IRQ3_OSC = 0x01;  // bit0: oscillator stable

// ── Operation register (0x00) bit masks ─────────────────────────────────────
static const uint8_t ST25R300_OP_EN = 0x08;        // bit3: chip enable
static const uint8_t ST25R300_OP_VDDDR_EN = 0x10;  // bit4: VDDDR regulator enable
static const uint8_t ST25R300_OP_RX_EN = 0x20;     // bit5: RX enable
static const uint8_t ST25R300_OP_TX_EN = 0x40;     // bit6: TX enable
// All operational (en + vdddr + rx + tx):
static const uint8_t ST25R300_OP_ALL_ON =
    ST25R300_OP_EN | ST25R300_OP_VDDDR_EN | ST25R300_OP_RX_EN | ST25R300_OP_TX_EN;  // 0x78

// ── Tx Protocol register 1 (0x15) bit masks ──────────────────────────────────
static const uint8_t ST25R300_TX_PROT1_A_TX_PAR = 0x40;  // bit6: ISO14443A TX parity enable
static const uint8_t ST25R300_TX_PROT1_TX_CRC = 0x20;    // bit5: TX CRC enable
static const uint8_t ST25R300_TX_PROT1_TR_AM = 0x10;     // bit4: modulation type (0=OOK, 1=AM)

// ── Rx Protocol register 1 (0x17) bit masks ──────────────────────────────────
// DS14655 Table 44: factory default = 0x3C (b_rx_sof=1, b_rx_eof=1, a_rx_par=1, rx_crc=1)
static const uint8_t ST25R300_RX_PROT1_B_RX_SOF =
    0x20;  // bit5: expect SOF from PICC (default=1, MUST be set for I_rxs to fire)
static const uint8_t ST25R300_RX_PROT1_B_RX_EOF = 0x10;  // bit4: expect EOF from PICC (default=1)
static const uint8_t ST25R300_RX_PROT1_A_RX_PAR = 0x08;  // bit3: ISO14443A RX parity enable (default=1)
static const uint8_t ST25R300_RX_PROT1_RX_CRC = 0x04;    // bit2: RX CRC enable (default=1)
static const uint8_t ST25R300_RX_PROT1_ANTCL = 0x01;     // bit0: anticollision mode

// ── Direct Commands ──────────────────────────────────────────────────────────
enum ST25R300Command : uint8_t {
  ST25R300_CMD_SET_DEFAULT = 0x61,    // reset to power-up state
  ST25R300_CMD_STOP_ALL = 0x63,       // stop all activities (also clears FIFO)
  ST25R300_CMD_CLEAR_FIFO = 0x65,     // clear FIFO only
  ST25R300_CMD_CLEAR_RX_GAIN = 0x67,  // clear Rx gain (= ST25R3916's Reset Rx Gain 0xD5)
  ST25R300_CMD_ADJUST_REGS = 0x69,    // adjust regulators
  ST25R300_CMD_TRANSMIT_DATA = 0x6B,  // transmit data from FIFO
  ST25R300_CMD_FIELD_ON = 0x6F,       // NFC field on (RF collision avoidance)
  ST25R300_CMD_MASK_RX = 0x71,        // mask receive data
  ST25R300_CMD_UNMASK_RX = 0x73,      // unmask receive data
  ST25R300_CMD_START_MRT = 0xE7,      // start mask-receive timer (also starts NRT if nrt≠0)
  ST25R300_CMD_START_NRT = 0xE9,      // start no-response timer
  ST25R300_CMD_STOP_NRT = 0xEB,       // stop no-response timer
  ST25R300_CMD_SENSE_RF = 0x7D,       // measure RF field strength → SENSE_RF register
};

// ── FIFO SPI access bytes ────────────────────────────────────────────────────
static const uint8_t ST25R300_FIFO_WRITE_BYTE = 0x5F;  // SPI byte to start FIFO write
static const uint8_t ST25R300_FIFO_READ_BYTE = 0xDF;   // SPI byte to start FIFO read (0x80|0x5F)

// ── IC Identity ──────────────────────────────────────────────────────────────
static const uint8_t ST25R300_IC_TYPE_MASK = 0xF8;  // ic_type[7:3]
static const uint8_t ST25R300_IC_TYPE_VAL = 0xB0;   // 10110_000b = 0xB0 for ST25R300

// ── NFC-V (ISO 15693) protocol constants ─────────────────────────────────────
static const uint8_t NFCV_CMD_INVENTORY = 0x01;
static const uint8_t NFCV_CMD_STAY_QUIET = 0x02;
static const uint8_t NFCV_INV_FLAG_1SLOT = 0x26;  // high data rate + inventory + 1-slot
static const uint8_t NFCV_SLPV_FLAG = 0x22;       // high data rate + addressed
static const uint8_t NFCV_UID_LEN = 8;

}  // namespace st25r300
}  // namespace esphome
