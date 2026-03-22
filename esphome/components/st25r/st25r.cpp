#include "st25r.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/nfc/nfc_tag.h"
#include "esphome/components/nfc/nfc_helpers.h"
#include <cinttypes>
#include <algorithm>
#include <cstring>

/*
 * Mifare Classic support.
 *
 * Protocol flow adapted from mf1.c — MIT licence:
 *   https://github.com/suut/rfal-mifare-classic/blob/master/mf1/mf1.c
 *
 * ST25R3916 9-bit parity interleaving (mf1_encode/decode_parity_st25r3916):
 *   Each byte is stored as 9 bits in the FIFO: 8 data bits then 1 parity bit.
 *   CRC and parity are both handled manually; ISO14443A_CONF bits no_tx_par
 *   (bit6) and no_rx_par (bit7) must be set before transmitting/receiving.
 */

// ── Mifare CRC-A ────────────────────────────────────────────────────────────
// Inline from mf1.h (MIT, suut/rfal-mifare-classic)
static uint16_t mifare_crc_a(const uint8_t *data, size_t len) {
  uint16_t crc = 0x6363;
  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i] ^ (uint8_t)(crc & 0xFF);
    b ^= b << 4;
    crc = (crc >> 8) ^ ((uint16_t) b << 8) ^ ((uint16_t) b << 3) ^ ((uint16_t) b >> 4);
  }
  return crc;
}

// ── Odd parity lookup ────────────────────────────────────────────────────────
static const uint8_t ODD_PARITY[256] = {
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
  1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
  0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
};

// ── 9-bit parity pack/unpack ─────────────────────────────────────────────────
// Each byte → 9 bits in the buffer: data bits 0..7 then parity bit.
// Adapted from mf1_encode/decode_parity_st25r3916 (MIT, suut/rfal-mifare-classic)

static void mifare_pack_parity(const uint8_t *in, const uint8_t *par,
                                uint8_t *out, uint8_t nbytes,
                                uint16_t *out_bits) {
  uint16_t total = 9u * nbytes;
  memset(out, 0, (total + 7u) / 8u);
  for (uint8_t i = 0; i < nbytes; i++) {
    for (uint8_t j = 0; j < 8; j++) {
      uint32_t p = j + 9u * i;
      out[p / 8] |= (uint8_t)(((in[i] >> j) & 1u) << (p % 8));
    }
    uint32_t p = 8u + 9u * i;
    out[p / 8] |= (uint8_t)((par[i] & 1u) << (p % 8));
  }
  *out_bits = total;
}

static uint8_t mifare_unpack_parity(const uint8_t *in, uint8_t *out,
                                     uint8_t *par, uint16_t in_bits) {
  uint8_t nbytes = (uint8_t)(in_bits / 9u);
  memset(out, 0, nbytes);
  memset(par, 0, nbytes);
  for (uint8_t i = 0; i < nbytes; i++) {
    for (uint8_t j = 0; j < 8; j++) {
      uint32_t p = j + 9u * i;
      out[i] |= (uint8_t)(((in[p / 8] >> (p % 8)) & 1u) << j);
    }
    uint32_t p = 8u + 9u * i;
    par[i] = (in[p / 8] >> (p % 8)) & 1u;
  }
  return nbytes;
}

namespace esphome {
namespace st25r {

static const char *const TAG = "st25r";

void ST25R::isr(ST25R *arg) {
  arg->irq_triggered_ = true;
}

void ST25R::setup() {
  ESP_LOGI(TAG, "Setting up ST25R...");
  if (this->reset_pin_ != nullptr) {
    ESP_LOGI(TAG, "Resetting ST25R via pin...");
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(10);
    this->reset_pin_->digital_write(false); 
    delay(10);
  }
  
  if (this->irq_pin_ != nullptr) {
    ESP_LOGI(TAG, "Configuring IRQ pin...");
    this->irq_pin_->setup();
    this->irq_pin_->attach_interrupt(ST25R::isr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  if (this->status_binary_sensor_ != nullptr) {
    this->status_binary_sensor_->publish_initial_state(false);
  }
  ESP_LOGI(TAG, "Starting reset_chip_()...");
  if (!this->reset_chip_()) {
    ESP_LOGE(TAG, "Failed to reset chip");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "ST25R initialized successfully.");
}

void ST25R::update() {
  if (this->is_failed() || this->state_ != STATE_IDLE) return;

  // Health check: verify chip identity at a separate (typically slower) interval.
  // This decouples liveness checks from the tag scan rate — e.g. scan every 500ms
  // but only verify IC identity every 60s.
  if (this->health_check_enabled_) {
    uint32_t now = millis();
    if (now - this->last_health_check_ms_ >= this->health_check_interval_ms_) {
      this->last_health_check_ms_ = now;
      uint8_t ic_identity = this->read_register(IC_IDENTITY);
      uint8_t chip_type = ic_identity & 0xF8;
      if (chip_type != 0x28 && chip_type != 0x30) {
        ESP_LOGW(TAG, "Health check failed: IC identity 0x%02X (failures: %u/%u)",
                 ic_identity, this->health_check_failures_ + 1, this->max_failed_checks_);
        this->health_check_failures_++;
        if (this->status_binary_sensor_ != nullptr)
          this->status_binary_sensor_->publish_state(false);
        if (this->health_check_failures_ >= this->max_failed_checks_) {
          if (this->auto_reset_on_failure_) {
            ESP_LOGW(TAG, "Health check: max failures reached, triggering reinit");
            this->state_ = STATE_REINITIALIZING;
          } else {
            ESP_LOGE(TAG, "Health check: max failures reached, auto-reset disabled");
          }
        }
        return;
      }
      this->health_check_failures_ = 0;
      if (this->status_binary_sensor_ != nullptr)
        this->status_binary_sensor_->publish_state(true);
    }
  }

  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->write_command(ST25R_CMD_RESET_RX_GAIN);  // reset AGC/squelch to initial state per datasheet

  if (this->rf_field_enabled_) {
    this->write_register(OP_CONTROL, 0xC8); // en=1, rx_en=1, tx_en=1
  }

  if (this->rf_field_enabled_ && this->field_strength_sensor_ != nullptr) {
    this->write_command(ST25R_CMD_MEASURE_AMPLITUDE);
    uint8_t amplitude = this->read_register(AD_CONV_RESULT);
    this->field_strength_sensor_->publish_state(amplitude);
  }

  // NFC-V (ISO 15693) blocking inventory — runs before NFC-A scan
  if (this->nfcv_enabled_)
    this->nfcv_scan_();

  // NFC-B (ISO 14443B) blocking SENSB_REQ — runs before NFC-A scan
  if (this->nfcb_enabled_)
    this->nfcb_scan_();

  // If previous scan activated ISO-DEP (RATS), send S-Block DESELECT to return
  // the card to IDLE state. Without this, ISO-DEP cards ignore subsequent WUPAs.
  if (this->last_sak_ & 0x20) {
    uint8_t deselect[] = {0xC2};  // S-Block DESELECT, no DID
    uint8_t dsl_resp[4];
    uint8_t dsl_len = 0;
    this->transceive_(deselect, 1, dsl_resp, dsl_len, 10);
    this->last_sak_ = 0;  // Clear so we don't deselect again if tag is gone
  }

  this->saved_anticol_valid_ = false;
  this->anticol_resume_ = false;

  this->irq_triggered_ = false;
  this->write_command(ST25R_CMD_TRANSMIT_WUPA);
  ESP_LOGI(TAG, "Sent WUPA");
  delay(1);
  this->state_ = STATE_WUPA;
  this->last_state_change_ = millis();
}

bool ST25R::transceive_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms) {
  return this->transceive_ex_(data, len, resp, resp_len, true, timeout_ms);
}

bool ST25R::transceive_no_crc_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, uint32_t timeout_ms) {
  return this->transceive_ex_(data, len, resp, resp_len, false, timeout_ms);
}

bool ST25R::transceive_ex_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len, bool with_crc, uint32_t timeout_ms) {
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->write_command(ST25R_CMD_RESET_RX_GAIN);  // reset AGC/squelch per datasheet transceive sequence
  // Clear ALL IRQ registers so IRQ pin goes low — required for ISR rising-edge to fire
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);

  this->write_register(NUM_TX_BYTES1, (len >> 8) & 0xFF);
  if (with_crc) {
    this->write_register(NUM_TX_BYTES2, (len & 0x1F) << 3);
  } else {
    this->write_register(NUM_TX_BYTES2, 0x00); // Whole bytes
  }

  this->write_fifo(data, len);

  this->irq_triggered_ = false;
  if (with_crc) {
    this->write_command(ST25R_CMD_TRANSMIT_WITH_CRC);
    ESP_LOGV(TAG, "  transceive_: Transmitting %d bytes with CRC: %02X %02X", len, data[0], data[1]);
  } else {
    this->write_command(ST25R_CMD_TRANSMIT_WITHOUT_CRC);
    ESP_LOGV(TAG, "  transceive_: Transmitting %d bytes without CRC: %02X %02X", len, data[0], data[1]);
  }

  uint32_t start = millis();
  resp_len = 0;
  bool tx_done = false;

  while (millis() - start < timeout_ms) {
    uint8_t irq;
    if (this->irq_triggered_) {
      this->irq_triggered_ = false;
      irq = this->read_register(IRQ_MAIN);
    } else {
      // Fallback: poll directly in case ISR missed the rising edge (pin was already high)
      irq = this->read_register(IRQ_MAIN);
    }
    this->irq_status_ = irq;

    if (irq & IRQ_TXE) tx_done = true;

    if (tx_done) {
      uint8_t f1 = this->read_register(FIFO_STATUS1);
      if (f1 > 0) {
        uint8_t to_read = std::min((uint8_t)(64 - resp_len), f1);
        this->read_fifo(resp + resp_len, to_read);
        resp_len += to_read;
        start = millis();
      }
      if (irq & IRQ_RXE) {
        return resp_len > 0;
      }
    }
    delay(1);
  }
  return resp_len > 0;
}

// ── transceive_mifare_ ───────────────────────────────────────────────────────
// Send/receive with manual CRC and parity (ISO14443A_CONF no_tx_par + no_rx_par).
// data/parity: len plaintext bytes + precomputed parity bits.
// resp/resp_parity: decoded response bytes and their parity bits.
// Adapted from mf1_send_receive_raw (MIT, suut/rfal-mifare-classic).
bool ST25R::transceive_mifare_(const uint8_t *data, const uint8_t *parity,
                                uint8_t len,
                                uint8_t *resp, uint8_t *resp_parity,
                                uint8_t &resp_len,
                                uint32_t timeout_ms) {
  // Max encoded size: ceil(9*64/8) = 72 bytes
  uint8_t encoded[72];
  uint16_t tx_bits = 0;
  mifare_pack_parity(data, parity, encoded, len, &tx_bits);

  uint8_t ntx_n   = (uint8_t)(tx_bits >> 3);
  uint8_t ntx_b   = (uint8_t)(tx_bits & 7);
  uint8_t fifo_bytes = (uint8_t)((tx_bits + 7) / 8);

  // Set manual parity mode: no_tx_par (bit6) + no_rx_par (bit7)
  this->write_register(ISO14443A_CONF, 0xC0);
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->write_command(ST25R_CMD_RESET_RX_GAIN);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->irq_triggered_ = false;

  this->write_register(NUM_TX_BYTES1, ntx_n >> 5);
  this->write_register(NUM_TX_BYTES2, (uint8_t)(((ntx_n & 0x1F) << 3) | (ntx_b & 7)));
  this->write_fifo(encoded, fifo_bytes);
  this->write_command(ST25R_CMD_TRANSMIT_WITHOUT_CRC);

  // Wait for RXE (end of receive)
  uint32_t start = millis();
  resp_len = 0;
  bool tx_done = false;
  while (millis() - start < timeout_ms) {
    uint8_t irq = this->read_register(IRQ_MAIN);
    if (irq & IRQ_TXE) tx_done = true;
    if (tx_done) {
      uint8_t f1 = this->read_register(FIFO_STATUS1);
      if (irq & IRQ_RXE) {
        // Read all FIFO bytes; FIFO holds 9*n bits packed
        uint8_t rx_fifo[72] = {};
        uint8_t rx_bytes = std::min(f1, (uint8_t) 72);
        if (rx_bytes > 0)
          this->read_fifo(rx_fifo, rx_bytes);

        // We need to know how many bits arrived; use FIFO_STATUS2 fifo_lb
        uint8_t fs2 = this->read_register(FIFO_STATUS2);
        uint8_t last_bits = (fs2 >> 1) & 0x07;  // fifo_lb: bits in last byte (0 = full byte)
        uint16_t rx_bits = (uint16_t)(rx_bytes * 8) - (last_bits ? (uint8_t)(8 - last_bits) : 0);

        resp_len = mifare_unpack_parity(rx_fifo, resp, resp_parity, rx_bits);

        this->write_register(ISO14443A_CONF, 0x00);
        return resp_len > 0;
      }
    }
    delay(1);
  }

  this->write_register(ISO14443A_CONF, 0x00);
  return false;
}

// ── mifare_authenticate_ ─────────────────────────────────────────────────────
// Three-pass mutual authentication per ISO 14443-3 / NXP AN10609.
// Protocol flow from mf1_authenticate() (MIT, suut/rfal-mifare-classic).
bool ST25R::mifare_authenticate_(uint8_t block, bool key_b, uint64_t key,
                                  const uint8_t *uid, uint8_t uid_len,
                                  struct Crypto1State *cs) {
  // ── Step 1: send AUTHENT command (plain text, with CRC) ──────────────────
  uint8_t auth_cmd[2] = {(uint8_t)(key_b ? 0x61 : 0x60), block};
  uint8_t nt_raw[4] = {};
  uint8_t nt_len = 0;
  if (!this->transceive_(auth_cmd, 2, nt_raw, nt_len, 20) || nt_len < 4) {
    ESP_LOGW(TAG, "Mifare auth: no NT from tag (block %u)", block);
    return false;
  }

  // ── Step 2: Crypto1 challenge-response ───────────────────────────────────
  uint8_t uid_offset = (uid_len > 4) ? (uint8_t)(uid_len - 4) : 0;
  uint32_t uid_u32 = ((uint32_t) uid[uid_offset]     << 24) |
                     ((uint32_t) uid[uid_offset + 1] << 16) |
                     ((uint32_t) uid[uid_offset + 2] <<  8) |
                      (uint32_t) uid[uid_offset + 3];
  uint32_t nt = ((uint32_t) nt_raw[0] << 24) | ((uint32_t) nt_raw[1] << 16) |
                ((uint32_t) nt_raw[2] <<  8) |  (uint32_t) nt_raw[3];
  ESP_LOGD(TAG, "Mifare auth: NT=%08X UID=%08X", nt, uid_u32);

  crypto1_init(cs, key);
  crypto1_word(cs, nt ^ uid_u32, 0);

  // Choose a fixed nr (reader nonce); any value works for normal auth
  const uint8_t nr[4] = {0x12, 0x34, 0x56, 0x78};
  uint8_t nr_ar[8], nr_ar_par[8];

  // Encrypt NR: feed plaintext NR into LFSR (is_encrypted=0), advance parity bit with crypto1_bit
  for (int i = 0; i < 4; i++) {
    nr_ar[i]     = crypto1_byte(cs, nr[i], 0) ^ nr[i];
    nr_ar_par[i] = crypto1_bit(cs, 0, 0) ^ ODD_PARITY[nr[i]];
  }

  // AR = 4 bytes of prng_successor(NT, 64) MSB-first
  uint32_t ar_plain = prng_successor(nt, 64);
  for (int i = 0; i < 4; i++) {
    uint8_t b = (uint8_t)((ar_plain >> (24 - 8 * i)) & 0xFF);
    nr_ar[4 + i]     = crypto1_byte(cs, 0, 0) ^ b;
    nr_ar_par[4 + i] = crypto1_bit(cs, 0, 0) ^ ODD_PARITY[b];
  }

  // ── Step 3: send nr+ar (encrypted, manual parity, no CRC) ────────────────
  uint8_t at[4] = {}, at_par[4] = {};
  uint8_t at_len = 0;
  if (!this->transceive_mifare_(nr_ar, nr_ar_par, 8, at, at_par, at_len) || at_len < 4) {
    ESP_LOGW(TAG, "Mifare auth: no AT from tag (block %u)", block);
    return false;
  }

  // ── Step 4: verify tag answer ─────────────────────────────────────────────
  uint32_t at_expected = prng_successor(ar_plain, 32) ^ crypto1_word(cs, 0, 0);
  uint32_t at_got = ((uint32_t) at[0] << 24) | ((uint32_t) at[1] << 16) |
                    ((uint32_t) at[2] <<  8) |  (uint32_t) at[3];
  if (at_got != at_expected) {
    ESP_LOGW(TAG, "Mifare auth: AT mismatch (got %08" PRIx32 " expected %08" PRIx32 ")", at_got, at_expected);
    return false;
  }

  ESP_LOGI(TAG, "Mifare auth OK (block %u, key %s)", block, key_b ? "B" : "A");
  return true;
}

// ── mifare_read_block_ ───────────────────────────────────────────────────────
// Read one 16-byte block after a successful mifare_authenticate_().
// Protocol flow from mf1_send_receive_encrypted (MIT, suut/rfal-mifare-classic).
bool ST25R::mifare_read_block_(uint8_t block, uint8_t *data,
                                struct Crypto1State *cs) {
  // Build: READ(0x30) + block + CRC_A — then encrypt all 4 bytes + CRC
  uint8_t cmd[4];
  cmd[0] = 0x30;
  cmd[1] = block;
  uint16_t crc = mifare_crc_a(cmd, 2);
  cmd[2] = (uint8_t)(crc & 0xFF);
  cmd[3] = (uint8_t)(crc >> 8);

  uint8_t enc[4], enc_par[4];
  for (int i = 0; i < 4; i++) {
    enc[i]     = crypto1_byte(cs, 0, 0) ^ cmd[i];
    enc_par[i] = (uint8_t)(crypto1_bit(cs, 0, 0) ^ ODD_PARITY[cmd[i]]);
  }

  // Response: 16 data bytes + 2 CRC bytes = 18 bytes, all encrypted
  uint8_t rx_enc[18] = {}, rx_par[18] = {};
  uint8_t rx_len = 0;
  if (!this->transceive_mifare_(enc, enc_par, 4, rx_enc, rx_par, rx_len) || rx_len < 18) {
    ESP_LOGW(TAG, "Mifare read block %u failed (got %u bytes)", block, rx_len);
    return false;
  }

  // Decrypt and verify parity + CRC
  uint8_t plain[18];
  for (int i = 0; i < 18; i++) {
    plain[i] = crypto1_byte(cs, 0, 0) ^ rx_enc[i];
    uint8_t exp_par = (uint8_t)(crypto1_bit(cs, 0, 0) ^ ODD_PARITY[plain[i]]);
    if (rx_par[i] != exp_par) {
      ESP_LOGW(TAG, "Mifare read block %u: parity error at byte %d", block, i);
      return false;
    }
  }
  uint16_t rx_crc = mifare_crc_a(plain, 16);
  if ((rx_crc & 0xFF) != plain[16] || (rx_crc >> 8) != plain[17]) {
    ESP_LOGW(TAG, "Mifare read block %u: CRC error", block);
    return false;
  }

  memcpy(data, plain, 16);
  return true;
}

std::unique_ptr<nfc::NfcTag> ST25R::read_tag_(std::vector<uint8_t> &uid) {
  uint8_t type = nfc::guess_tag_type(uid.size());
  ESP_LOGI(TAG, "read_tag_: UID length=%zu, guessed type=%d, SAK=0x%02X", uid.size(), type, this->last_sak_);

  // ISO-DEP capable (SAK bit 5 set) → try Type 4 NDEF read
  if (this->last_sak_ & 0x20) {
    ESP_LOGI(TAG, "ISO-DEP capable tag (SAK & 0x20) — trying Type 4 NDEF");
    return this->read_tag_type4_(uid);
  }

  if (type == nfc::TAG_TYPE_MIFARE_CLASSIC) {
    ESP_LOGI(TAG, "Mifare Classic detected - attempting authentication");
    struct Crypto1State cs = {};
    bool auth_ok = this->mifare_authenticate_(0, false, this->mifare_key_a_,
                                              uid.data(), (uint8_t) uid.size(), &cs);
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    if (!auth_ok) {
      ESP_LOGW(TAG, "Mifare Classic: sector 0 auth failed (wrong key or clone card)");
      return make_unique<nfc::NfcTag>(nfc_uid, nfc::MIFARE_CLASSIC);
    }

    ESP_LOGI(TAG, "Mifare Classic: Auth successful, reading blocks 1 and 2");
    // Read blocks 1 and 2 (block 0 is manufacturer data; block 3 is sector trailer)
    uint8_t block1[16] = {}, block2[16] = {};
    bool b1 = this->mifare_read_block_(1, block1, &cs);
    bool b2 = b1 && this->mifare_read_block_(2, block2, &cs);

    if (!b1) {
      ESP_LOGW(TAG, "Mifare Classic: block read failed");
      return make_unique<nfc::NfcTag>(nfc_uid, nfc::MIFARE_CLASSIC);
    }

    ESP_LOGI(TAG, "Block 1: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
             block1[0],block1[1],block1[2],block1[3],block1[4],block1[5],block1[6],block1[7],
             block1[8],block1[9],block1[10],block1[11],block1[12],block1[13],block1[14],block1[15]);
    ESP_LOGI(TAG, "Block 2: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
             block2[0],block2[1],block2[2],block2[3],block2[4],block2[5],block2[6],block2[7],
             block2[8],block2[9],block2[10],block2[11],block2[12],block2[13],block2[14],block2[15]);

    // Look for NFC Forum Type 2 NDEF TLV (0x03) in the data area
    // On Mifare Classic the NDEF data starts at block 1 byte 0 when
    // the card is formatted as NFC Forum Type 2 / Mifare Classic NDEF.
    std::vector<uint8_t> raw;
    raw.insert(raw.end(), block1, block1 + 16);
    if (b2) raw.insert(raw.end(), block2, block2 + 16);

    size_t idx = 0;
    while (idx < raw.size()) {
      uint8_t tlv = raw[idx++];
      if (tlv == 0xFE) break;      // terminator
      if (tlv == 0x00) continue;   // null
      if (idx >= raw.size()) break;
      uint8_t tlen = raw[idx++];
      if (tlv == 0x03 && tlen > 0 && (idx + tlen) <= raw.size()) {
        std::vector<uint8_t> ndef_data(raw.begin() + (int) idx, raw.begin() + (int) idx + tlen);
        ESP_LOGI(TAG, "Mifare Classic: NDEF found (%u bytes)", tlen);
        return make_unique<nfc::NfcTag>(nfc_uid, nfc::MIFARE_CLASSIC, ndef_data);
      }
      idx += tlen;
    }

    ESP_LOGD(TAG, "Mifare Classic: no NDEF TLV in sector 0 data blocks");
    return make_unique<nfc::NfcTag>(nfc_uid, nfc::MIFARE_CLASSIC);
  }

  if (type == nfc::TAG_TYPE_2) {
    std::vector<uint8_t> data;
    uint8_t buffer[16];
    uint8_t len;

    uint8_t read_cmd[2] = {0x30, 0x00}; 
    if (this->transceive_(read_cmd, 2, buffer, len) && len >= 16) {
      ESP_LOGD(TAG, "  Read page 0-3 success");
      data.insert(data.end(), buffer, buffer + 16); // Only keep data, skip CRC
      
      size_t tlv_index = 0;
      bool found = false;
      bool terminator_found = false;

      for (size_t i = 0; i < 16; i++) { 
        if (data[i] == 0x03) {
          tlv_index = i;
          found = true;
          ESP_LOGD(TAG, "  Found NDEF TLV at index %zu", i);
          break;
        }
        if (data[i] == 0xFE) {
          terminator_found = true;
          break;
        }
      }

      if (!found && !terminator_found) {
        for (uint8_t p = 4; p < 16; p += 4) {
          delay(10);
          read_cmd[1] = p;
          if (this->transceive_(read_cmd, 2, buffer, len) && len >= 16) {
            data.insert(data.end(), buffer, buffer + 16);
            for (size_t i = data.size() - 16; i < data.size(); i++) {
              if (data[i] == 0x03) {
                tlv_index = i;
                found = true;
                ESP_LOGD(TAG, "  Found NDEF TLV at index %zu", i);
                break;
              }
              if (data[i] == 0xFE) {
                terminator_found = true;
                ESP_LOGD(TAG, "  Found Terminator TLV (0xFE) at index %zu", i);
                break;
              }
            }
          }
          if (found || terminator_found) break;
        }
      }

      if (found) {
        // Ensure we have enough bytes to read the full TLV length field
        // (3-byte length needs tlv_index + 3 to be valid)
        while (data.size() <= tlv_index + 3) {
          read_cmd[1] = (uint8_t)(data.size() / 4);
          delay(10);
          if (!this->transceive_(read_cmd, 2, buffer, len) || len < 16) break;
          data.insert(data.end(), buffer, buffer + 16);
        }

        if (tlv_index + 1 < data.size()) {
          size_t msg_len;
          size_t msg_start_idx;
          if (data[tlv_index + 1] == 0xFF && tlv_index + 3 < data.size()) {
            // 3-byte TLV length (BER-TLV extended form) for payloads ≥255 bytes
            msg_len = ((size_t) data[tlv_index + 2] << 8) | data[tlv_index + 3];
            msg_start_idx = tlv_index + 4;
          } else {
            msg_len = data[tlv_index + 1];
            msg_start_idx = tlv_index + 2;
          }
          ESP_LOGD(TAG, "  NDEF message length: %zu", msg_len);

          while (data.size() < msg_start_idx + msg_len) {
            uint8_t next_page = (uint8_t)(data.size() / 4);
            read_cmd[1] = next_page;
            delay(10);
            if (!this->transceive_(read_cmd, 2, buffer, len) || len < 16) {
              ESP_LOGW(TAG, "  Failed to read page %d during NDEF fetch", next_page);
              break;
            }
            data.insert(data.end(), buffer, buffer + 16);
          }

          if (data.size() >= msg_start_idx + msg_len) {
            std::vector<uint8_t> ndef_data(data.begin() + (int) msg_start_idx,
                                           data.begin() + (int) msg_start_idx + (int) msg_len);
            ESP_LOGI(TAG, "  Successfully read NDEF message of %zu bytes", msg_len);
            nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
            if (msg_len > 0) {
              return make_unique<nfc::NfcTag>(nfc_uid, nfc::NFC_FORUM_TYPE_2, ndef_data);
            } else {
              return make_unique<nfc::NfcTag>(nfc_uid, nfc::NFC_FORUM_TYPE_2);
            }
          }
        }
      } else {
        ESP_LOGD(TAG, "  No NDEF TLV (0x03) found in searched pages");
      }
    } else {
      ESP_LOGW(TAG, "  Failed to read page 0, len=%d", len);
    }
  }

  nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
  return make_unique<nfc::NfcTag>(nfc_uid);
}

void ST25R::loop() {
  if (this->is_failed()) return;

  if (this->irq_triggered_) {
    this->irq_triggered_ = false;
    this->irq_status_ = this->read_register(IRQ_MAIN);
    ESP_LOGV(TAG, "IRQ triggered, status: 0x%02X, state: %d", this->irq_status_, this->state_);
  } else if (this->state_ == STATE_WUPA || this->state_ == STATE_ANTICOL || this->state_ == STATE_SELECT) {
    // Fallback polling — ISR rising edge may not fire if IRQ pin was already high
    this->irq_status_ = this->read_register(IRQ_MAIN);
    if (this->irq_status_ != 0) {
      ESP_LOGV(TAG, "IRQ polled, status: 0x%02X, state: %d", this->irq_status_, this->state_);
    }
  } else {
    this->irq_status_ = 0;
  }

  this->process_state_();
}

void ST25R::process_state_() {
  switch (this->state_) {
    case STATE_IDLE:
      break;

    case STATE_WUPA: {
      if (this->irq_status_ & (IRQ_RXE | IRQ_COL)) {
          this->cascade_level_ = 0;
          this->current_uid_ = "";
          if (!this->anticol_resume_) {
            // Fresh scan: start anticol from beginning
            this->anticol_prefix_full_ = 0;
            this->anticol_prefix_bits_ = 0;
            this->anticol_col_pos_ = 0;
            this->anticol_prefix_val_ = 0;
          }
          // anticol_resume_ is cleared inside: use saved prefix for this one anticol round
          this->anticol_resume_ = false;

          this->send_anticol_frame_();
          this->state_ = STATE_ANTICOL;
          this->last_state_change_ = millis();
      } else if (this->irq_status_ & IRQ_NRE) {
          // NRT expired: no tag response — fast path to idle (set by derived class read_chip_irq_())
          ESP_LOGD(TAG, "WUPA NRE (no tag): IRQ=0x%02X", this->irq_status_);
          this->irq_status_ = 0;
          this->state_ = STATE_IDLE;
          this->finalize_scan_();
      } else if (millis() - this->last_state_change_ > 100) {
          // No tag responded and NRE fast-path didn't fire (real ST25R3916:
          // NRE is in IRQ_TIMER, not IRQ_MAIN).  Read ALL IRQ registers so the
          // IRQ pin goes LOW before the next scan — otherwise the pin stays HIGH
          // from the pending NRE and the ISR rising-edge never fires again.
          uint8_t irq_t = this->read_register(IRQ_TIMER);
          uint8_t irq_e = this->read_register(IRQ_ERROR);
          ESP_LOGD(TAG, "WUPA timeout: IRQ=0x%02X IRQ_T=0x%02X IRQ_E=0x%02X FIFO=%u",
                   this->irq_status_, irq_t, irq_e, this->read_fifo_status1_());
          this->irq_status_ = 0;
          this->irq_triggered_ = false;
          this->state_ = STATE_IDLE;
          this->finalize_scan_();
      }
      break;
    }

    case STATE_ANTICOL: {
      if (millis() - this->last_state_change_ > 20) {
        uint8_t max_prefix_val = (1 << (this->anticol_col_pos_ + 1)) - 1;
        if (this->anticol_col_pos_ > 0 && this->anticol_prefix_val_ < max_prefix_val) {
          this->anticol_prefix_val_++;
          this->apply_anticol_prefix_();
          // Send WUPA before each new prefix attempt — some cards (e.g. Mifare Classic) leave
          // the READY state quickly after responding to an anticol they don't match.
          this->anticol_resume_ = true;
          this->start_wupa_();
          this->state_ = STATE_WUPA;
          this->last_state_change_ = millis();
          return;
        }
        this->state_ = STATE_IDLE;
        this->finalize_scan_();
        return;
      }

      if (this->irq_status_ != 0 && (this->irq_status_ & (IRQ_RXE | IRQ_COL | IRQ_TXE))) {
        delay(5);
        uint8_t f1 = this->read_fifo_status1_();
        bool has_collision = (this->irq_status_ & IRQ_COL) != 0;

        if (has_collision) {
          uint8_t col_raw = this->read_collision_display_();
          uint8_t c_byte = (col_raw >> 4) & 0x0F;
          uint8_t c_bit  = (col_raw >> 1) & 0x07;
          // col_pos_abs is from start of TX frame (SEL + NVB = 2 bytes = 16 bits)
          int uid_col_pos = (int)(c_byte * 8 + c_bit) - 16;
          if (uid_col_pos < 0) uid_col_pos = 0;
          // Drain any garbage FIFO bytes
          if (f1 > 0) { uint8_t tmp[8]; this->read_fifo(tmp, std::min(f1, (uint8_t)8)); }

          // FIFO bytes during collision are unreliable — brute-force all 2^(col_pos+1) prefixes
          this->anticol_col_pos_ = uid_col_pos;
          this->anticol_prefix_val_ = 0;
          this->apply_anticol_prefix_();

          this->send_anticol_frame_();
          this->last_state_change_ = millis();

        } else if (f1 >= 5) {
          // Clean response — full UID received
          uint8_t resp[5];
          this->read_fifo(resp, 5);

          // The tag only sends bits NOT covered by the prefix. The FIFO stores the
          // tag's response bits with zeros in the first anticol_prefix_bits_ positions.
          // Reconstruct the full UID by OR-ing the prefix bits back in.
          uint8_t full_uid[4];
          memcpy(full_uid, resp, 4);
          for (int k = 0; k < (int) this->anticol_prefix_full_; k++) {
            full_uid[k] = this->anticol_prefix_[k];
          }
          if (this->anticol_prefix_bits_ > 0) {
            uint8_t mask = (uint8_t)((1 << this->anticol_prefix_bits_) - 1);
            full_uid[this->anticol_prefix_full_] =
                (this->anticol_prefix_[this->anticol_prefix_full_] & mask) |
                (resp[this->anticol_prefix_full_] & (uint8_t)(~mask));
          }
          uint8_t bcc = full_uid[0] ^ full_uid[1] ^ full_uid[2] ^ full_uid[3];

          uint8_t sel_cmds[] = {0x93, 0x95, 0x97};
          uint8_t sel_pk[7] = {sel_cmds[this->cascade_level_], 0x70,
                               full_uid[0], full_uid[1], full_uid[2], full_uid[3], bcc};

          if (full_uid[0] == 0x88) {
            for (int i = 1; i < 4; i++) {
              char buf[3]; snprintf(buf, sizeof(buf), "%02X", full_uid[i]); this->current_uid_ += buf;
            }
          } else {
            for (int i = 0; i < 4; i++) {
              char buf[3]; snprintf(buf, sizeof(buf), "%02X", full_uid[i]); this->current_uid_ += buf;
            }
          }

          // Clear anticollision mode before SELECT (ST25R3916: ISO14443A_CONF=0x00;
          // ST25R300: no-op here — handled via RX_PROTOCOL1 inside transceive_ex_()).
          this->pre_select_();

          uint8_t sak_buf[3];
          uint8_t sak_len = 0;
          if (!this->transceive_(sel_pk, 7, sak_buf, sak_len) || sak_len == 0) {
            ESP_LOGW(TAG, "SELECT failed (no SAK)");
            this->state_ = STATE_IDLE;
            this->finalize_scan_();
            return;
          }
          uint8_t sak = sak_buf[0];
          this->last_sak_ = sak;

          if (sak & 0x04) {  // Cascade bit — need another anticollision level
            // Save CL1 collision state before overwriting for CL2
            if (this->cascade_level_ == 0) {
              this->saved_col_pos_ = this->anticol_col_pos_;
              this->saved_prefix_val_ = this->anticol_prefix_val_;
              this->saved_anticol_valid_ = (this->anticol_col_pos_ > 0 || this->anticol_prefix_bits_ > 0);
            }
            this->cascade_level_++;
            if (this->cascade_level_ > 2) {
              ESP_LOGE(TAG, "Too many cascade levels");
              this->state_ = STATE_IDLE;
              this->finalize_scan_();
              return;
            }
            this->anticol_prefix_full_ = 0;
            this->anticol_prefix_bits_ = 0;
            this->anticol_col_pos_ = 0;
            this->anticol_prefix_val_ = 0;
            this->send_anticol_frame_();
            this->state_ = STATE_ANTICOL;
            this->last_state_change_ = millis();
          } else {
            // Tag fully selected — validate UID length (must be 4 or 7 bytes; 3-byte = CL1 glitch)
            size_t uid_bytes_len = this->current_uid_.length() / 2;
            if (uid_bytes_len != 4 && uid_bytes_len != 7) {
              ESP_LOGW(TAG, "Discarding invalid UID len=%zu (%s)", uid_bytes_len, this->current_uid_.c_str());
              this->state_ = STATE_IDLE;
              this->finalize_scan_();
              return;
            }

            ESP_LOGI(TAG, "Tag selected: %s", this->current_uid_.c_str());

            // Read tag data on first detection only (auth + NDEF read if Mifare)
            if (!this->present_tags_.count(this->current_uid_)) {
              std::vector<uint8_t> uid_bytes;
              for (size_t i = 0; i < this->current_uid_.length(); i += 2)
                uid_bytes.push_back((uint8_t) strtol(this->current_uid_.substr(i, 2).c_str(), nullptr, 16));
              this->tags_data_[this->current_uid_] = this->read_tag_(uid_bytes);
            }

            this->tags_this_scan_.insert(this->current_uid_);

            // HALT: send [0x50, 0x00] + CRC via chip-specific send_halt_()
            this->send_halt_();

            // Determine the CL1 collision state so we can resume the multi-tag tree traversal.
            // If we went through cascade (CL2), restore the saved CL1 state.
            // Otherwise use the current CL1 state directly.
            uint8_t resume_col_pos;
            uint8_t resume_prefix_val;
            bool can_resume;
            if (this->saved_anticol_valid_) {
              resume_col_pos = this->saved_col_pos_;
              resume_prefix_val = this->saved_prefix_val_;
              can_resume = true;
              this->saved_anticol_valid_ = false;
            } else {
              resume_col_pos = this->anticol_col_pos_;
              resume_prefix_val = this->anticol_prefix_val_;
              can_resume = (this->anticol_col_pos_ > 0 || this->anticol_prefix_bits_ > 0);
            }

            if (can_resume) {
              // Advance to the next branch in the collision tree
              this->cascade_level_ = 0;
              this->current_uid_ = "";
              this->anticol_col_pos_ = resume_col_pos;
              this->anticol_prefix_val_ = resume_prefix_val + 1;
              this->apply_anticol_prefix_();

              uint8_t max_val = (1 << (resume_col_pos + 1)) - 1;
              if (this->anticol_prefix_val_ > max_val) {
                // All branches at this collision level exhausted — done
                this->state_ = STATE_IDLE;
                this->finalize_scan_();
                return;
              }
              // Send WUPA (not REQA) so all tags — including those in HALT — wake up.
              // Some cards (e.g. Mifare Classic) return to HALT after a non-matching SELECT,
              // so REQA would not wake them.
              this->anticol_resume_ = true;
              this->start_wupa_();
            } else {
              // No prior collision: this was the only tag — scan complete
              this->state_ = STATE_IDLE;
              this->finalize_scan_();
              return;
            }
            this->state_ = STATE_WUPA;
            this->last_state_change_ = millis();
          }
        }
      }
      break;
    }

    case STATE_REINITIALIZING:
      this->reinitialize_();
      this->state_ = STATE_IDLE;
      break;
  }
}

void ST25R::finalize_scan_() {
  ESP_LOGD(TAG, "finalize_scan_: this_scan=%zu present=%zu", this->tags_this_scan_.size(), this->present_tags_.size());
  // Increment miss counters for tags not seen this scan; fire on_tag_removed when threshold reached
  std::vector<std::string> to_remove;
  for (auto &kv : this->present_tags_) {
    if (this->tags_this_scan_.count(kv.first)) {
      kv.second = 0;  // seen this scan — reset miss counter
    } else {
      kv.second++;
      if (kv.second >= this->miss_threshold_) {
        to_remove.push_back(kv.first);
      }
    }
  }
  for (const auto &uid : to_remove) {
    ESP_LOGI(TAG, "Tag Removed: %s", uid.c_str());

    std::vector<uint8_t> uid_bytes;
    for (size_t i = 0; i < uid.length(); i += 2) {
      uid_bytes.push_back((uint8_t) strtol(uid.substr(i, 2).c_str(), nullptr, 16));
    }
    nfc::NfcTagUid nfc_uid(uid_bytes.begin(), uid_bytes.end());
    nfc::NfcTag nfc_tag(nfc_uid);
    for (auto *listener : this->tag_listeners_) {
      listener->tag_off(nfc_tag);
    }
    for (auto *trigger : this->on_tag_removed_triggers_) {
      trigger->trigger(uid);
    }
    this->tags_data_.erase(uid);
    this->present_tags_.erase(uid);
  }

  // Fire on_tag for newly seen UIDs
  for (const auto &uid : this->tags_this_scan_) {
    if (!this->present_tags_.count(uid)) {
      ESP_LOGD(TAG, "finalize_scan_: NEW tag %s, firing %zu on_tag triggers", uid.c_str(), this->on_tag_triggers_.size());
      this->present_tags_[uid] = 0;
      for (auto *trigger : this->on_tag_triggers_) {
        trigger->trigger(uid);
      }
      // Fire tag_on for NFC listeners (e.g. ndef_write action)
      if (this->tags_data_.count(uid) && this->tags_data_[uid]) {
        for (auto *listener : this->tag_listeners_) {
          listener->tag_on(*this->tags_data_[uid]);
        }
      }
    }
  }

  // Update binary sensors
  for (auto *obj : this->binary_sensors_) {
    for (const auto &uid : this->tags_this_scan_) {
      obj->process(uid);
    }
    obj->on_scan_end();
  }

  this->tags_this_scan_.clear();
}

bool ST25R::wait_for_irq_(uint8_t mask, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (this->irq_triggered_) {
       return true;
    }
    delay(1);
  }
  return false;
}

bool ST25R::reset_chip_() {
  // Verify IC identity BEFORE SET_DEFAULT — if bus is down, don't clear registers.
  // IC_IDENTITY is read-only and unaffected by SET_DEFAULT.
  uint8_t ic_identity = this->read_register(IC_IDENTITY);
  ESP_LOGD(TAG, "  reset_: IC identity read: 0x%02X", ic_identity);
  uint8_t chip_type = ic_identity & 0xF8;
  if (chip_type != 0x28 && chip_type != 0x30) {
    ESP_LOGE(TAG, "  reset_: IC identity mismatch! Expected 0x28/0x30, got 0x%02X", chip_type);
    return false;
  }

  ESP_LOGV(TAG, "  reset_: Sending SET_DEFAULT");
  this->write_command(ST25R_CMD_SET_DEFAULT);
  delay(10);
  bool is_b_version = (chip_type == 0x30);
  this->is_b_version_ = is_b_version;
  // ST25R3916 (0x28) and ST25R3916B (0x30) both have Automatic Antenna Tuning (AAT)
  // with varicap DAC outputs on ANT_TUNE_A/B (0x26/0x27).  Variants without AAT
  // (e.g. ST25R300/500/501 — see feature matrix) use their own reset_chip_() override and
  // never reach this code, but set the flag explicitly to represent the capability.
  this->has_aat_ = true;
  ESP_LOGI(TAG, "IC identity match: 0x%02X (ST25R3916%s)", ic_identity, is_b_version ? "B" : "");

  // RFAL sequence: oscillator on → measure VDD → configure registers
  ESP_LOGV(TAG, "  reset_: Enabling oscillator");
  this->write_register(OP_CONTROL, 0x80); // en=1: enable oscillator and regulators
  delay(10); // Wait for oscillator to stabilize (~700µs typical)

  // Measure VDD and auto-detect supply voltage (RFAL: st25r3916MeasureVoltage)
  // REGULATOR_CONTROL mpsv bits[2:0] select measurement source; 0 = VDD
  uint8_t reg_ctrl = this->read_register(REGULATOR_CONTROL);
  this->write_register(REGULATOR_CONTROL, (reg_ctrl & ~0x07) | 0x00); // mpsv=VDD
  this->write_command(ST25R_CMD_MEASURE_VDD);  // 0xDF
  delay(5);
  uint8_t vdd_raw = this->read_register(AD_CONV_RESULT);
  uint16_t vdd_mv = (uint16_t)vdd_raw * 23U + (((uint16_t)vdd_raw * 4U + 5U) / 10U);
  bool sup3v = (vdd_mv < 3600);
  ESP_LOGI(TAG, "  reset_: VDD measured: %u mV (raw=0x%02X) → sup3V=%s",
           vdd_mv, vdd_raw, sup3v ? "3.3V" : "5V");

  ESP_LOGV(TAG, "  reset_: Configuring registers");
  this->write_register(IO_CONF1, 0x07);  // Disable MCU_CLK + LF clock (RFAL default)
  uint8_t io_conf2 = sup3v ? 0x80 : 0x00;  // sup3V based on measured VDD
  io_conf2 |= 0x18;  // SPI pull-downs (miso_pd1 | miso_pd2)
  if (this->has_aat_)
    io_conf2 |= 0x20;  // aat_en: enable AAT module so ANT_TUNE_A/B drive varicaps
  this->write_register(IO_CONF2, io_conf2);
  this->write_register(MODE, 0x08);
  this->write_register(BIT_RATE, 0x00);
  // RFAL NFC-A 106 kbps RX analog profile
  this->write_register(RX_CONF1, 0x08);  // AM path squelch, HPF=60-400kHz
  this->write_register(RX_CONF2, 0x2D);  // Mixer demod, AGC on, 61ns pulse
  this->write_register(RX_CONF3, 0x00);  // HF mode, no LF routing (same for B and non-B)
  this->write_register(RX_CONF4, 0x00);
  this->write_register(MASK_MAIN, 0x00);   // unmask all main IRQs
  this->write_register(MASK_TIMER, 0x00);  // unmask all timer IRQs (NRE etc)
  this->write_register(ISO14443A_CONF, 0x00);

  uint8_t d_res = (15 - this->rf_power_) & 0x0F;
  // am_mod (bits[7:4]) MUST be 0 for ISO14443A — 100% ASK (OOK) required.
  this->write_register(TX_DRIVER_CONF, d_res);
  // ANT_TUNE_A/B (0x26/0x27): varicap DAC outputs for antenna resonance tuning (AAT).
  // Only write on chips that have AAT hardware — ST25R3916/3916B do; ST25R300/500/501 do not.
  if (this->has_aat_) {
    this->write_register(ANT_TUNE_A, this->ant_tune_a_);
    this->write_register(ANT_TUNE_B, this->ant_tune_b_);
    ESP_LOGI(TAG, "  reset_: ANT_TUNE_A=0x%02X ANT_TUNE_B=0x%02X",
             this->ant_tune_a_, this->ant_tune_b_);
  }

  // RFAL chip-init registers
  this->write_register(AUX_MOD, 0x10);           // lm_ext=0, lm_dri=1 (external load mod)
  this->write_register(RES_AM_MOD, 0x80);        // Minimum non-overlap
  this->write_register(FIELD_THRESHOLD_ACTV, 0x11);   // trg=105mV, rfe=105mV
  this->write_register(FIELD_THRESHOLD_DEACTV, 0x00); // trg=75mV, rfe=75mV
  this->write_register(PASSIVE_TARGET, 0x50);    // fdel=5 (FDT aligned to bitgrid)
  this->write_register(PT_MOD, 0x51);            // Reduce RFO resistance in modulated state
  this->write_register(EMD_SUP_CONF, 0x40);      // rx_start_emv: start RX on first 4 bits

  // RFAL NFC-A 106 TX: overshoot/undershoot protection
  this->write_register(OVERSHOOT_CONF1, 0x40);
  this->write_register(OVERSHOOT_CONF2, 0x03);
  this->write_register(UNDERSHOOT_CONF1, 0x40);
  this->write_register(UNDERSHOOT_CONF2, 0x03);
  // RFAL NFC-A 106 RX: correlator receiver + thresholds
  uint8_t aux_val = this->read_register(AUX);
  aux_val &= ~0x04;  // Clear dis_corr → enable correlator receiver
  this->write_register(AUX, aux_val);
  this->write_register(RX_CONF4, 0x00);
  this->write_register(CORR_CONF1, 0x51);
  this->write_register(CORR_CONF2, 0x00);

  if (this->rf_field_enabled_) {
    ESP_LOGV(TAG, "  reset_: Enabling RF field");
    this->field_on_();
  }
  delay(10);

  // AAT hill-climbing: optimize ANT_TUNE_A/B for maximum field amplitude.
  // Confirmed +10mm range on STEVAL-MB17149B (varicap-equipped board).
  // Boards without varicaps see no effect (harmless). Disable via YAML: aat_enabled: false
  if (this->aat_enabled_)
    this->aat_tune_();

  ESP_LOGV(TAG, "  reset_: Complete");
  return true;
}

void ST25R::reinitialize_() {
  this->reinitialization_attempts_++;
  ESP_LOGW(TAG, "Reinitializing ST25R (attempt %u)...", this->reinitialization_attempts_);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(10);
    this->reset_pin_->digital_write(false);
    delay(10);
  }
  if (this->reset_chip_()) {
    ESP_LOGI(TAG, "Reinitialize succeeded after %u attempt(s)", this->reinitialization_attempts_);
    this->health_check_failures_ = 0;
    this->reinitialization_attempts_ = 0;
    // Force an immediate health check on the next update() so the status sensor
    // reflects recovery without waiting the full health_check_interval.
    this->last_health_check_ms_ = 0;
  } else {
    ESP_LOGE(TAG, "Reinitialize attempt %u failed", this->reinitialization_attempts_);
    if (this->reinitialization_attempts_ >= 5) {
      ESP_LOGE(TAG, "Reinitialize: too many failures, marking component failed");
      this->mark_failed();
    }
  }
}

void ST25R::apply_anticol_prefix_() {
  // Decode anticol_prefix_val_ (bit N..0) into prefix arrays
  // anticol_col_pos_ = N: prefix covers bits 0..N (N+1 bits total)
  // bit position i of prefix = (anticol_prefix_val_ >> i) & 1
  int total_bits = this->anticol_col_pos_ + 1;
  this->anticol_prefix_full_ = total_bits >> 3;
  this->anticol_prefix_bits_ = total_bits & 7;
  memset(this->anticol_prefix_, 0, sizeof(this->anticol_prefix_));
  for (int i = 0; i < total_bits; i++) {
    int byte_idx = i >> 3;
    int bit_idx  = i & 7;
    if ((this->anticol_prefix_val_ >> i) & 1)
      this->anticol_prefix_[byte_idx] |= (1 << bit_idx);
  }
}

void ST25R::send_anticol_frame_() {
  uint8_t sel_cmds[] = {0x93, 0x95, 0x97};
  uint8_t sel = sel_cmds[this->cascade_level_];

  // NVB: high nibble = complete bytes in frame (SEL + NVB + complete UID prefix bytes only)
  //      low nibble  = partial bits (0 = full bytes only)
  // NOTE: partial byte is NOT counted in high nibble — it goes into FIFO but NVB only counts complete bytes
  uint8_t nvb_high = 2 + this->anticol_prefix_full_;
  uint8_t nvb = (nvb_high << 4) | this->anticol_prefix_bits_;

  uint8_t frame[7];
  frame[0] = sel;
  frame[1] = nvb;
  uint8_t frame_len = 2;
  for (int i = 0; i < this->anticol_prefix_full_; i++)
    frame[frame_len++] = this->anticol_prefix_[i];
  if (this->anticol_prefix_bits_ > 0)
    frame[frame_len++] = this->anticol_prefix_[this->anticol_prefix_full_];

  // NUM_TX_BYTES: N full bytes + B partial bits (B>0 means one extra partial byte is in FIFO)
  // N = SEL + NVB + complete UID prefix bytes only (NOT counting the partial byte)
  uint8_t ntx_n = 2 + this->anticol_prefix_full_;
  uint8_t ntx_b = this->anticol_prefix_bits_;

  this->write_register(ISO14443A_CONF, 0x01);  // antcl=1
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(IRQ_MAIN);    // clear all IRQ registers so IRQ pin goes low
  this->read_register(IRQ_TIMER);   // IRQ pin stays high until ALL pending bits are cleared
  this->read_register(IRQ_ERROR);
  this->irq_triggered_ = false;
  this->write_fifo(frame, frame_len);
  this->write_register(NUM_TX_BYTES1, ntx_n >> 5);
  this->write_register(NUM_TX_BYTES2, ((ntx_n & 0x1F) << 3) | (ntx_b & 0x07));
  this->write_command(ST25R_CMD_TRANSMIT_WITHOUT_CRC);

}

// ── Default virtual helpers (ST25R3916 implementations) ──────────────────────

void ST25R::pre_select_() {
  // ST25R3916: clear antcl bit in ISO14443A_CONF so SELECT uses normal (non-anticol) framing
  this->write_register(ISO14443A_CONF, 0x00);
}

uint8_t ST25R::read_fifo_status1_() {
  return this->read_register(FIFO_STATUS1);
}

uint8_t ST25R::read_collision_display_() {
  return this->read_register(COLLISION_DISPLAY);
}

void ST25R::send_halt_() {
  // HALT: send [0x50, 0x00] + CRC; tag has no response.
  // Don't use transceive_() here — it blocks waiting for a non-existent response.
  uint8_t halt_cmd[2] = {0x50, 0x00};
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->write_fifo(halt_cmd, 2);
  this->write_register(NUM_TX_BYTES1, 0x00);
  this->write_register(NUM_TX_BYTES2, 0x10);  // 2 bytes
  this->write_command(ST25R_CMD_TRANSMIT_WITH_CRC);
  delay(10);  // wait for HALT frame to be transmitted (~2ms for 4 bytes)
}

void ST25R::start_wupa_() {
  // Clear FIFO + IRQs, then transmit WUPA.
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->read_register(IRQ_ERROR);
  this->irq_triggered_ = false;
  this->irq_status_ = 0;
  this->write_command(ST25R_CMD_TRANSMIT_WUPA);
}

void ST25R::field_on_() {
  this->write_register(OP_CONTROL, 0x88); // en=1, tx_en=1
  delay(10);
  this->write_command(ST25R_CMD_FIELD_ON);
  delay(10);
  this->write_register(OP_CONTROL, 0xC8); // en=1, rx_en=1, tx_en=1
  this->write_command(ST25R_CMD_ADJUST_REGULATORS);
}

// ── AAT (Automatic Antenna Tuning) hill-climbing optimizer ───────────────────
// Based on RFAL st25r3916AatTune() (AN5322). Iteratively adjusts ANT_TUNE_A/B
// to maximize RF field amplitude (AD_CONV_RESULT). Runs once after field_on_()
// during reset_chip_() on chips with AAT hardware (ST25R3916/3916B).

void ST25R::aat_tune_() {
  if (!this->has_aat_) return;

  uint8_t a = this->ant_tune_a_;
  uint8_t b = this->ant_tune_b_;
  uint8_t step_a = 16;
  uint8_t step_b = 16;
  const uint8_t max_iter = 20;

  // Measure current amplitude
  auto measure = [this]() -> uint8_t {
    this->write_command(ST25R_CMD_MEASURE_AMPLITUDE);
    delay(3);  // VCC settling time
    return this->read_register(AD_CONV_RESULT);
  };

  this->write_register(ANT_TUNE_A, a);
  this->write_register(ANT_TUNE_B, b);
  delay(3);
  uint8_t best_amp = measure();
  uint8_t best_a = a, best_b = b;

  ESP_LOGD(TAG, "AAT: start A=0x%02X B=0x%02X amp=%u", a, b, best_amp);

  for (uint8_t iter = 0; iter < max_iter; iter++) {
    bool improved = false;

    // Try 4 directions: A±step, B±step
    struct Dir { int16_t da; int16_t db; };
    Dir dirs[] = {
      {step_a, 0}, {-step_a, 0},
      {0, step_b}, {0, -step_b}
    };

    for (auto &d : dirs) {
      int16_t new_a = (int16_t)a + d.da;
      int16_t new_b = (int16_t)b + d.db;
      if (new_a < 0 || new_a > 255 || new_b < 0 || new_b > 255) continue;

      this->write_register(ANT_TUNE_A, (uint8_t)new_a);
      this->write_register(ANT_TUNE_B, (uint8_t)new_b);
      delay(3);
      uint8_t amp = measure();

      if (amp > best_amp) {
        best_amp = amp;
        best_a = (uint8_t)new_a;
        best_b = (uint8_t)new_b;
        improved = true;
      }
    }

    if (improved) {
      a = best_a;
      b = best_b;
      this->write_register(ANT_TUNE_A, a);
      this->write_register(ANT_TUNE_B, b);
    } else {
      // No improvement — reduce step size
      step_a = (step_a > 1) ? step_a / 2 : 0;
      step_b = (step_b > 1) ? step_b / 2 : 0;
      if (step_a == 0 && step_b == 0) break;  // Converged
    }
  }

  // Store final values
  this->ant_tune_a_ = best_a;
  this->ant_tune_b_ = best_b;
  this->write_register(ANT_TUNE_A, best_a);
  this->write_register(ANT_TUNE_B, best_b);

  ESP_LOGI(TAG, "AAT: converged A=0x%02X B=0x%02X amp=%u", best_a, best_b, best_amp);
}

// ── ISO 14443-4 (ISO-DEP / Type 4 tags) ─────────────────────────────────────

bool ST25R::isodep_activate_(uint8_t *ats, uint8_t &ats_len) {
  // Send RATS: 0xE0, FSDI=8 (256 bytes) | DID=0
  uint8_t rats[] = {0xE0, 0x80};
  uint8_t resp[64];
  uint8_t resp_len = 0;
  ats_len = 0;

  if (!this->transceive_(rats, sizeof(rats), resp, resp_len) || resp_len < 2) {
    ESP_LOGD(TAG, "ISO-DEP: RATS failed (resp_len=%u)", resp_len);
    return false;
  }

  // ATS: TL(1) + T0(1) + [TA] + [TB] + [TC] + [HB...]
  ats_len = resp_len;
  memcpy(ats, resp, resp_len);
  this->isodep_block_number_ = 0;

  ESP_LOGD(TAG, "ISO-DEP: ATS received, TL=%u", resp[0]);

  // PPS (Protocol and Parameter Selection) — negotiate higher bit rate if supported
  if (resp_len >= 2) {
    uint8_t t0 = resp[1];
    uint8_t fsci = t0 & 0x0F;
    bool has_ta = t0 & 0x10;
    if (has_ta && resp_len >= 3) {
      uint8_t ta = resp[2];
      // TA bits[6:4] = DS (PCD→PICC speeds), bits[2:0] = DR (PICC→PCD speeds)
      // Try 424 kbps if supported (bit 2 of DS and DR)
      uint8_t pps_dsi = 0, pps_dri = 0;
      if (ta & 0x40) pps_dsi = 2;       // 424 kbps PCD→PICC
      else if (ta & 0x20) pps_dsi = 1;  // 212 kbps
      if (ta & 0x04) pps_dri = 2;       // 424 kbps PICC→PCD
      else if (ta & 0x02) pps_dri = 1;  // 212 kbps

      if (pps_dsi > 0 || pps_dri > 0) {
        // PPS: PPSS(0xD0|CID) + PPS0(0x11) + PPS1(DSI<<2|DRI)
        uint8_t pps[] = {0xD0, 0x11, (uint8_t)((pps_dsi << 2) | pps_dri)};
        uint8_t pps_resp[4];
        uint8_t pps_len = 0;
        if (this->transceive_(pps, sizeof(pps), pps_resp, pps_len) && pps_len >= 1 && (pps_resp[0] & 0xF0) == 0xD0) {
          // Update BIT_RATE register for the negotiated speed
          uint8_t br = (pps_dsi << 4) | pps_dri;
          this->write_register(BIT_RATE, br);
          ESP_LOGI(TAG, "ISO-DEP PPS: negotiated DSI=%u DRI=%u (BR=0x%02X)", pps_dsi, pps_dri, br);
        }
      }
    }
  }

  return true;
}

bool ST25R::isodep_transceive_(const uint8_t *apdu, size_t apdu_len, uint8_t *resp, uint8_t &resp_len) {
  // Wrap APDU in I-Block: [PCB][APDU...]
  uint8_t frame[64];
  if (apdu_len + 1 > sizeof(frame)) return false;

  // PCB: I-Block, no chaining, no DID/NAD, must-be-1 bit, block number
  frame[0] = 0x02 | (this->isodep_block_number_ & 0x01);
  memcpy(frame + 1, apdu, apdu_len);

  uint8_t raw_resp[64];
  uint8_t raw_len = 0;

  if (!this->transceive_(frame, apdu_len + 1, raw_resp, raw_len) || raw_len < 3) {
    return false;
  }

  // Toggle block number for next exchange
  this->isodep_block_number_ ^= 1;

  // Strip PCB byte, check for I-Block response
  if ((raw_resp[0] & 0xC0) != 0x00) {
    // Not an I-Block — might be R-Block or S-Block
    ESP_LOGD(TAG, "ISO-DEP: non-I-Block response PCB=0x%02X", raw_resp[0]);
    return false;
  }

  // Response: [PCB][data...][SW1][SW2]  (CRC already stripped by transceive_)
  resp_len = raw_len - 1;  // strip PCB
  memcpy(resp, raw_resp + 1, resp_len);
  return true;
}

std::unique_ptr<nfc::NfcTag> ST25R::read_tag_type4_(std::vector<uint8_t> &uid) {
  uint8_t ats[64];
  uint8_t ats_len = 0;

  if (!this->isodep_activate_(ats, ats_len)) {
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    return make_unique<nfc::NfcTag>(nfc_uid);
  }

  uint8_t resp[64];
  uint8_t resp_len = 0;

  // Step 1: SELECT NDEF Application (AID = D276000085010100 for v2, try v1 as fallback)
  uint8_t select_app[] = {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
  if (!this->isodep_transceive_(select_app, sizeof(select_app), resp, resp_len) ||
      resp_len < 2 || resp[resp_len - 2] != 0x90 || resp[resp_len - 1] != 0x00) {
    // Try v1 AID
    select_app[11] = 0x00;  // D276000085010100 → D276000085010000
    resp_len = 0;
    if (!this->isodep_transceive_(select_app, sizeof(select_app), resp, resp_len) ||
        resp_len < 2 || resp[resp_len - 2] != 0x90 || resp[resp_len - 1] != 0x00) {
      ESP_LOGD(TAG, "T4T: SELECT NDEF app failed");
      nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
      return make_unique<nfc::NfcTag>(nfc_uid);
    }
  }

  // Step 2: SELECT CC File (0xE103)
  uint8_t select_cc[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03};
  resp_len = 0;
  if (!this->isodep_transceive_(select_cc, sizeof(select_cc), resp, resp_len) ||
      resp_len < 2 || resp[resp_len - 2] != 0x90) {
    ESP_LOGD(TAG, "T4T: SELECT CC failed");
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    return make_unique<nfc::NfcTag>(nfc_uid);
  }

  // Step 3: READ BINARY CC (15 bytes)
  uint8_t read_cc[] = {0x00, 0xB0, 0x00, 0x00, 0x0F};
  resp_len = 0;
  if (!this->isodep_transceive_(read_cc, sizeof(read_cc), resp, resp_len) ||
      resp_len < 17) {  // 15 data + SW1 SW2
    ESP_LOGD(TAG, "T4T: READ CC failed (resp_len=%u)", resp_len);
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    return make_unique<nfc::NfcTag>(nfc_uid);
  }

  // Parse CC: bytes 7-8 = NDEF file ID, bytes 9-10 = max NDEF size
  // CC TLV at offset 7: type(1) + len(1) + fileID(2) + maxSize(2) + readAccess(1) + writeAccess(1)
  uint16_t ndef_file_id = ((uint16_t)resp[9] << 8) | resp[10];
  uint16_t max_ndef_size = ((uint16_t)resp[11] << 8) | resp[12];
  ESP_LOGD(TAG, "T4T CC: NDEF file=0x%04X, max_size=%u", ndef_file_id, max_ndef_size);

  // Step 4: SELECT NDEF File
  uint8_t select_ndef[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, (uint8_t)(ndef_file_id >> 8), (uint8_t)(ndef_file_id & 0xFF)};
  resp_len = 0;
  if (!this->isodep_transceive_(select_ndef, sizeof(select_ndef), resp, resp_len) ||
      resp_len < 2 || resp[resp_len - 2] != 0x90) {
    ESP_LOGD(TAG, "T4T: SELECT NDEF file failed");
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    return make_unique<nfc::NfcTag>(nfc_uid);
  }

  // Step 5: READ NDEF Length (first 2 bytes)
  uint8_t read_nlen[] = {0x00, 0xB0, 0x00, 0x00, 0x02};
  resp_len = 0;
  if (!this->isodep_transceive_(read_nlen, sizeof(read_nlen), resp, resp_len) ||
      resp_len < 4) {  // 2 data + SW1 SW2
    ESP_LOGD(TAG, "T4T: READ NLEN failed");
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    return make_unique<nfc::NfcTag>(nfc_uid);
  }

  uint16_t ndef_len = ((uint16_t)resp[0] << 8) | resp[1];
  if (ndef_len == 0 || ndef_len > 255) {
    ESP_LOGD(TAG, "T4T: NDEF length %u (skipping read)", ndef_len);
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    return make_unique<nfc::NfcTag>(nfc_uid);
  }

  // Step 6: READ NDEF Data
  uint8_t read_ndef[] = {0x00, 0xB0, 0x00, 0x02, (uint8_t)ndef_len};
  resp_len = 0;
  if (!this->isodep_transceive_(read_ndef, sizeof(read_ndef), resp, resp_len) ||
      resp_len < ndef_len + 2) {
    ESP_LOGD(TAG, "T4T: READ NDEF data failed");
    nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
    return make_unique<nfc::NfcTag>(nfc_uid);
  }

  ESP_LOGI(TAG, "T4T NDEF: %u bytes read", ndef_len);
  nfc::NfcTagUid nfc_uid(uid.begin(), uid.end());
  return make_unique<nfc::NfcTag>(nfc_uid);
}

// ── NFC-B (ISO 14443B) for ST25R3916 ─────────────────────────────────────────

void ST25R::configure_nfcb_mode_() {
  // MODE: om=0x02 (ISO14443B) in bits[6:3] = 0x10, tr_am=1 (AM modulation) bit7 = 0x80
  this->write_register(MODE, 0x90);
  this->write_register(BIT_RATE, 0x00);  // 106 kbps
  // TX: 10% ASK (AM modulation) — NFC-B requires AM, not OOK
  uint8_t d_res = (15 - this->rf_power_) & 0x0F;
  this->write_register(TX_DRIVER_CONF, 0x70 | d_res);  // am_mod=7 (12% ASK)
  // RX: RFAL NFC-B analog profile
  this->write_register(RX_CONF1, 0x00);  // AM channel
  this->write_register(RX_CONF2, 0x2D);
  this->write_register(RX_CONF3, 0x00);
  this->write_register(RX_CONF4, 0x00);
  this->write_register(CORR_CONF1, 0x14);  // NFC-B correlator
  this->write_register(CORR_CONF2, 0x00);
}

void ST25R::nfcb_scan_() {
  this->configure_nfcb_mode_();

  // SENSB_REQ (ALLB_REQ): cmd=0x05, AFI=0x00 (any), PARAM=0x08 (1 slot + WUPB)
  uint8_t sensb_req[] = {0x05, 0x00, 0x08};
  uint8_t resp[16];
  uint8_t resp_len = 0;

  // Use standard CRC transceive — NFC-B uses the chip's CRC-B hardware
  this->write_command(ST25R_CMD_CLEAR_FIFO);
  this->write_command(ST25R_CMD_RESET_RX_GAIN);

  uint16_t ntx = sizeof(sensb_req);
  this->write_register(NUM_TX_BYTES1, ntx >> 5);
  this->write_register(NUM_TX_BYTES2, (ntx & 0x1F) << 3);
  this->write_fifo(sensb_req, sizeof(sensb_req));

  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->irq_triggered_ = false;
  this->write_command(ST25R_CMD_TRANSMIT_WITH_CRC);

  // Wait for ATQB response
  uint32_t start = millis();
  while (millis() - start < 20) {
    uint8_t irq = this->read_register(IRQ_MAIN);
    uint8_t irq_t = this->read_register(IRQ_TIMER);
    if (irq & 0x10) {  // RXE
      uint8_t fifo_len = this->read_register(FIFO_STATUS1);
      if (fifo_len >= 12) {  // Minimum ATQB: 0x50 + PUPI(4) + AppData(4) + ProtInfo(3)
        this->read_fifo(resp, fifo_len);
        resp_len = fifo_len;
      }
      break;
    }
    if (irq_t & 0x40) break;  // NRE
    delay(1);
  }

  // Restore NFC-A mode
  this->restore_nfca_mode_();
  // Also restore TX_DRIVER to OOK (am_mod=0)
  uint8_t d_res_restore = (15 - this->rf_power_) & 0x0F;
  this->write_register(TX_DRIVER_CONF, d_res_restore);

  if (resp_len < 12 || resp[0] != 0x50) return;

  // Parse ATQB: byte 0 = 0x50, bytes 1-4 = PUPI
  char uid_str[9];
  snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X", resp[1], resp[2], resp[3], resp[4]);
  ESP_LOGI(TAG, "NFC-B tag: %s (ATQB len=%u)", uid_str, resp_len);

  std::string uid_string(uid_str);
  this->tags_this_scan_.insert(uid_string);

  if (this->present_tags_.find(uid_string) == this->present_tags_.end()) {
    this->present_tags_[uid_string] = 0;
    std::vector<uint8_t> uid_bytes = {resp[1], resp[2], resp[3], resp[4]};
    nfc::NfcTagUid nfc_uid(uid_bytes.begin(), uid_bytes.end());
    auto tag = make_unique<nfc::NfcTag>(nfc_uid);
    for (auto *listener : this->tag_listeners_)
      listener->tag_on(*tag);
    for (auto *trigger : this->on_tag_triggers_)
      trigger->trigger(uid_string);
  }
}

// ── NFC-V (ISO 15693) streaming mode for ST25R3916 ──────────────────────────

// ISO 15693 CRC-16 CCITT (preset=0xFFFF, poly=0x8408, result inverted)
uint16_t ST25R::iso15693_crc_(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    uint8_t d = data[i] ^ (uint8_t)(crc & 0xFF);
    d ^= (d << 4);
    crc = (crc >> 8) ^ ((uint16_t)d << 8) ^ ((uint16_t)d << 3) ^ ((uint16_t)d >> 4);
  }
  return ~crc;
}

// 1-of-4 VCD encoding: each byte → 4 output bytes (SOF + data + CRC + EOF)
static const uint8_t ISO15693_1OF4_SOF = 0x21;
static const uint8_t ISO15693_1OF4_EOF = 0x04;
static const uint8_t ISO15693_1OF4_MAP[4] = {0x02, 0x08, 0x20, 0x80};

size_t ST25R::iso15693_encode_1of4_(const uint8_t *data, size_t len, bool add_crc,
                                     uint8_t *out, size_t out_max) {
  size_t pos = 0;
  if (out_max < 1 + (len + 2) * 4 + 1) return 0;

  // SOF
  out[pos++] = ISO15693_1OF4_SOF;

  // Encode data bytes
  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i];
    for (int j = 0; j < 4; j++) {
      out[pos++] = ISO15693_1OF4_MAP[b & 0x03];
      b >>= 2;
    }
  }

  // Encode CRC
  if (add_crc) {
    uint16_t crc = iso15693_crc_(data, len);
    uint8_t crc_bytes[2] = {(uint8_t)(crc & 0xFF), (uint8_t)(crc >> 8)};
    for (int c = 0; c < 2; c++) {
      uint8_t b = crc_bytes[c];
      for (int j = 0; j < 4; j++) {
        out[pos++] = ISO15693_1OF4_MAP[b & 0x03];
        b >>= 2;
      }
    }
  }

  // EOF
  out[pos++] = ISO15693_1OF4_EOF;
  return pos;
}

// Manchester VICC decoding: subcarrier stream → payload bytes
size_t ST25R::iso15693_decode_manchester_(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_max) {
  if (in_len == 0 || out_max == 0) return 0;

  // Check SOF: first 5 bits should be 0x17 (10111 = 3 unmodulated + 2 modulated)
  if ((in[0] & 0x1F) != 0x17) return 0;

  memset(out, 0, out_max);
  size_t mp = 5;  // Manchester bit position (after SOF)
  size_t bp = 0;  // Output bit position

  for (; mp < (in_len * 8 - 2); mp += 2) {
    uint8_t man = (in[mp / 8] >> (mp % 8)) & 0x01;
    man |= ((in[(mp + 1) / 8] >> ((mp + 1) % 8)) & 0x01) << 1;

    if (man == 1) {
      bp++;  // bit = 0
    } else if (man == 2) {
      out[bp / 8] |= (1 << (bp % 8));  // bit = 1
      bp++;
    } else {
      // Check for EOF: 10111000 pattern
      if ((bp % 8) == 0 && (in[mp / 8] & 0xE0) == 0xA0 && in[mp / 8 + 1] == 0x03) {
        break;  // EOF found
      }
      break;  // collision or invalid
    }

    if (bp >= out_max * 8) break;
  }

  return bp / 8;  // return complete bytes
}

void ST25R::configure_nfcv_stream_mode_() {
  // RFAL NFC-V analog config: RX_CONF1=0x13, RX_CONF2=0x2D, CORR_CONF1=0x13, CORR_CONF2=0x01
  this->write_register(RX_CONF1, 0x13);
  this->write_register(RX_CONF2, 0x2D);
  this->write_register(RX_CONF3, 0x00);
  this->write_register(RX_CONF4, 0x00);
  this->write_register(CORR_CONF1, 0x13);
  this->write_register(CORR_CONF2, 0x01);

  // STREAM_MODE register: din=5 (423.75kHz subcarrier), dout=7 (105.9kHz TX), report_period=3 (8 clocks)
  // smd = ((6-din) << 5) | ((7-dout) << 0) | (report_period << 3)
  uint8_t smd = ((6 - 5) << 5) | ((7 - 7) << 0) | (3 << 3);  // = 0x38
  this->write_register(STREAM_MODE, smd);

  // MODE: om=0x0E (subcarrier_stream) → bits[6:3] = 0x70
  // Preserve other MODE bits (targ=0 for initiator, tr_am from current config)
  this->write_register(MODE, 0x70);  // om=subcarrier_stream, initiator

  // Disable overshoot/undershoot for NFC-V TX (RFAL CHIP_POLL_COMMON)
  this->write_register(OVERSHOOT_CONF1, 0x00);
  this->write_register(OVERSHOOT_CONF2, 0x00);
  this->write_register(UNDERSHOOT_CONF1, 0x00);
  this->write_register(UNDERSHOOT_CONF2, 0x00);
}

void ST25R::restore_nfca_mode_() {
  // Restore NFC-A 106 settings
  this->write_register(MODE, 0x08);
  this->write_register(RX_CONF1, 0x08);
  this->write_register(RX_CONF2, 0x2D);
  this->write_register(RX_CONF3, 0x00);
  this->write_register(RX_CONF4, 0x00);
  this->write_register(CORR_CONF1, 0x51);
  this->write_register(CORR_CONF2, 0x00);
  // Restore overshoot/undershoot protection
  this->write_register(OVERSHOOT_CONF1, 0x40);
  this->write_register(OVERSHOOT_CONF2, 0x03);
  this->write_register(UNDERSHOOT_CONF1, 0x40);
  this->write_register(UNDERSHOOT_CONF2, 0x03);
}

bool ST25R::transceive_nfcv_stream_(const uint8_t *data, size_t len, uint8_t *resp, uint8_t &resp_len,
                                     uint32_t timeout_ms) {
  // Encode command using 1-of-4 coding with CRC
  uint8_t coded[128];
  // Set high data rate flag and clear dual subcarrier
  uint8_t cmd_buf[16];
  if (len > sizeof(cmd_buf)) return false;
  memcpy(cmd_buf, data, len);
  cmd_buf[0] |= 0x02;   // high data rate
  cmd_buf[0] &= ~0x01;  // single subcarrier

  size_t coded_len = iso15693_encode_1of4_(cmd_buf, len, true, coded, sizeof(coded));
  if (coded_len == 0) return false;

  this->write_command(ST25R_CMD_STOP_ALL);
  this->write_command(ST25R_CMD_CLEAR_FIFO);

  // Set TX frame: total sub-bits (not bytes!)
  // For 1-of-4: each coded byte = 1 sub-bit in stream mode
  uint16_t subbits = coded_len;
  this->write_register(NUM_TX_BYTES1, subbits >> 5);
  this->write_register(NUM_TX_BYTES2, (subbits & 0x1F) << 3);

  this->write_fifo(coded, coded_len);

  this->read_register(IRQ_MAIN);
  this->read_register(IRQ_TIMER);
  this->irq_triggered_ = false;
  this->write_command(ST25R_CMD_TRANSMIT_WITHOUT_CRC);  // Manual CRC (already encoded)

  // Wait for response
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    uint8_t irq = this->read_register(IRQ_MAIN);
    uint8_t irq_t = this->read_register(IRQ_TIMER);
    if (irq & 0x10) {  // RXE
      uint8_t fifo_len = this->read_register(FIFO_STATUS1);
      if (fifo_len > 0 && fifo_len <= 64) {
        uint8_t raw[64];
        this->read_fifo(raw, fifo_len);
        // Decode Manchester
        uint8_t decoded[32];
        size_t dec_len = iso15693_decode_manchester_(raw, fifo_len, decoded, sizeof(decoded));
        if (dec_len >= 3) {  // At least flags + CRC(2)
          // Verify CRC
          uint16_t crc = iso15693_crc_(decoded, dec_len - 2);
          if ((crc & 0xFF) == decoded[dec_len - 2] && (crc >> 8) == decoded[dec_len - 1]) {
            // Strip CRC
            resp_len = dec_len - 2;
            memcpy(resp, decoded, resp_len);
            return true;
          }
        }
      }
      return false;
    }
    if (irq_t & 0x40) {  // NRE
      return false;
    }
    delay(1);
  }
  return false;
}

void ST25R::nfcv_scan_() {
  this->configure_nfcv_stream_mode_();

  uint8_t inv_req[] = {0x26, 0x01, 0x00};  // flags, INVENTORY, mask_len=0
  uint8_t resp[16];
  uint8_t resp_len = 0;

  if (this->transceive_nfcv_stream_(inv_req, sizeof(inv_req), resp, resp_len, 30)) {
    if (resp_len >= 10 && !(resp[0] & 0x01)) {
      // Parse UID (bytes 2-9, LSB-first → reverse for display)
      char uid_str[17];
      for (int j = 0; j < 8; j++)
        snprintf(uid_str + j * 2, 3, "%02X", resp[9 - j]);
      uid_str[16] = '\0';
      ESP_LOGI(TAG, "NFC-V tag: %s (DSFID=0x%02X)", uid_str, resp[1]);

      // Add to tags_this_scan_ — finalize_scan_() handles on_tag/on_tag_removed
      std::string uid_string(uid_str);
      this->tags_this_scan_.insert(uid_string);

      // Try to read NDEF (Type 5 tag) via READ_SINGLE_BLOCK
      std::vector<uint8_t> uid_bytes;
      for (int j = 0; j < 8; j++)
        uid_bytes.push_back(resp[9 - j]);

      // Read block 0 (Capability Container)
      uint8_t blk_req[] = {0x02, 0x20, 0x00};  // flags, READ_SINGLE_BLOCK, block=0
      uint8_t blk_resp[8];
      uint8_t blk_len = 0;
      std::vector<uint8_t> ndef_data;

      if (this->transceive_nfcv_stream_(blk_req, sizeof(blk_req), blk_resp, blk_len, 20) &&
          blk_len >= 5 && !(blk_resp[0] & 0x01)) {
        // CC: byte1=magic(0xE1), byte2=ver+access, byte3=size(*8), byte4=features
        if (blk_resp[1] == 0xE1) {
          uint8_t cc_size = blk_resp[3];  // NDEF area size in 8-byte units
          uint8_t total_blocks = (cc_size * 8) / 4;  // 4 bytes per block
          if (total_blocks > 64) total_blocks = 64;  // safety limit

          // Read blocks 1..N for NDEF TLV data
          for (uint8_t blk = 1; blk <= total_blocks; blk++) {
            blk_req[2] = blk;
            blk_len = 0;
            if (this->transceive_nfcv_stream_(blk_req, sizeof(blk_req), blk_resp, blk_len, 20) &&
                blk_len >= 5 && !(blk_resp[0] & 0x01)) {
              for (int k = 1; k < 5 && k < blk_len; k++)
                ndef_data.push_back(blk_resp[k]);
            } else {
              break;
            }
          }
        }
      }

      // Fire on_tag immediately for new tags
      if (this->present_tags_.find(uid_string) == this->present_tags_.end()) {
        this->present_tags_[uid_string] = 0;
        nfc::NfcTagUid nfc_uid(uid_bytes.begin(), uid_bytes.end());

        if (!ndef_data.empty()) {
          // Parse NDEF TLV: search for type 0x03 (NDEF message)
          for (size_t i = 0; i < ndef_data.size(); i++) {
            if (ndef_data[i] == 0x03 && i + 1 < ndef_data.size()) {
              uint8_t ndef_len = ndef_data[i + 1];
              size_t ndef_start = i + 2;
              if (ndef_start + ndef_len <= ndef_data.size()) {
                ESP_LOGI(TAG, "NFC-V NDEF: %u bytes", ndef_len);
                auto tag = make_unique<nfc::NfcTag>(nfc_uid);
                this->tags_data_[uid_string] = std::move(tag);
              }
              break;
            }
            if (ndef_data[i] == 0xFE) break;  // Terminator TLV
          }
        }

        if (this->tags_data_.find(uid_string) == this->tags_data_.end()) {
          auto tag = make_unique<nfc::NfcTag>(nfc_uid);
          this->tags_data_[uid_string] = std::move(tag);
        }

        for (auto *listener : this->tag_listeners_)
          listener->tag_on(*this->tags_data_[uid_string]);
        for (auto *trigger : this->on_tag_triggers_)
          trigger->trigger(uid_string);
      }
    }
  }

  this->restore_nfca_mode_();
}

bool ST25R::ndef_write(nfc::NdefMessage *message, bool format) {
  // Check if the most recent tag is NFC-V (8-byte UID = 16 hex chars)
  // If so, use the NFC-V WRITE_SINGLE_BLOCK path
  if (!this->present_tags_.empty()) {
    const std::string &last_uid = this->present_tags_.rbegin()->first;
    if (last_uid.length() == 16) {
      return this->nfcv_ndef_write_(message);
    }
  }

  // NFC-A Type 2 NDEF write (NTAG / Ultralight)
  uint8_t buffer[16];
  uint8_t len;

  if (format) {
    ESP_LOGD(TAG, "Formatting tag (NTAG215 CC)...");
    uint8_t cc_cmd[6] = {0xA2, 0x03, 0xE1, 0x10, 0x3E, 0x00};
    bool cc_success = false;
    for (uint8_t i = 0; i < 3; i++) {
      delay(20);
      if (this->transceive_(cc_cmd, 6, buffer, len) && (len > 0 && (buffer[0] & 0x0F) == 0x0A)) {
        cc_success = true;
        break;
      }
    }
    if (!cc_success) {
      ESP_LOGE(TAG, "Failed to write CC page during format");
      return false;
    }
    delay(10);
  }

  if (message == nullptr) {
    // Just formatting/cleaning
    uint8_t empty_ndef[6] = {0xA2, 0x04, 0x03, 0x00, 0xFE, 0x00};
    bool empty_success = false;
    for (uint8_t i = 0; i < 3; i++) {
      delay(20);
      if (this->transceive_(empty_ndef, 6, buffer, len) && (len > 0 && (buffer[0] & 0x0F) == 0x0A)) {
        empty_success = true;
        break;
      }
    }
    return empty_success;
  }

  std::vector<uint8_t> ndef_data = message->encode();
  std::vector<uint8_t> payload;
  
  // Build TLV structure
  payload.push_back(0x03); // NDEF TLV
  if (ndef_data.size() < 255) {
    payload.push_back((uint8_t)ndef_data.size());
  } else {
    payload.push_back(0xFF);
    payload.push_back((uint8_t)((ndef_data.size() >> 8) & 0xFF));
    payload.push_back((uint8_t)(ndef_data.size() & 0xFF));
  }
  payload.insert(payload.end(), ndef_data.begin(), ndef_data.end());
  payload.push_back(0xFE); // Terminator TLV

  // Pad to 4-byte pages
  while (payload.size() % 4 != 0) payload.push_back(0);

  ESP_LOGD(TAG, "Writing NDEF message, total size with TLVs: %zu", payload.size());

  for (size_t i = 0; i < payload.size(); i += 4) {
    uint8_t page = 4 + (i / 4);
    uint8_t write_cmd[6] = {0xA2, page, payload[i], payload[i+1], payload[i+2], payload[i+3]};
    bool success = false;
    
    for (uint8_t retry = 0; retry < 3; retry++) {
      delay(20);
      if (this->transceive_(write_cmd, 6, buffer, len) && (len > 0 && (buffer[0] & 0x0F) == 0x0A)) {
        success = true;
        break;
      }
      ESP_LOGW(TAG, "NDEF write retry %d for page %d (resp_len=%d, byte0=%02X)", retry + 1, page, len, len > 0 ? buffer[0] : 0);
    }

    if (!success) {
      ESP_LOGE(TAG, "NDEF write failed at page %d after retries", page);
      return false;
    }
  }
  ESP_LOGI(TAG, "NDEF write successful!");
  return true;
}

bool ST25R::nfcv_ndef_write_(nfc::NdefMessage *message) {
  // NFC-V Type 5 NDEF write via WRITE_SINGLE_BLOCK (0x21)
  // Block 0 = CC, blocks 1+ = NDEF TLV data

  this->configure_nfcv_stream_mode_();

  std::vector<uint8_t> payload;
  if (message != nullptr) {
    std::vector<uint8_t> ndef_data = message->encode();
    payload.push_back(0x03);  // NDEF TLV type
    if (ndef_data.size() < 255) {
      payload.push_back((uint8_t)ndef_data.size());
    } else {
      payload.push_back(0xFF);
      payload.push_back((uint8_t)((ndef_data.size() >> 8) & 0xFF));
      payload.push_back((uint8_t)(ndef_data.size() & 0xFF));
    }
    payload.insert(payload.end(), ndef_data.begin(), ndef_data.end());
  }
  payload.push_back(0xFE);  // Terminator TLV
  while (payload.size() % 4 != 0) payload.push_back(0);

  ESP_LOGD(TAG, "NFC-V NDEF write: %zu bytes in %zu blocks", payload.size(), payload.size() / 4);

  // Write CC to block 0: magic=0xE1, ver=0x40 (v1.0, read/write), size, features=0x00
  uint8_t cc_size = (payload.size() + 4) / 8;  // size in 8-byte units (round up)
  if (cc_size == 0) cc_size = 1;
  uint8_t write_req[7] = {0x02, 0x21, 0x00, 0xE1, 0x40, cc_size, 0x00};  // flags, WRITE_SINGLE_BLOCK, block, data[4]
  uint8_t resp[4];
  uint8_t resp_len = 0;

  if (!this->transceive_nfcv_stream_(write_req, sizeof(write_req), resp, resp_len, 25)) {
    ESP_LOGE(TAG, "NFC-V: failed to write CC (block 0)");
    this->restore_nfca_mode_();
    return false;
  }

  // Write NDEF data blocks
  for (size_t i = 0; i < payload.size(); i += 4) {
    uint8_t blk = 1 + (i / 4);
    write_req[2] = blk;
    write_req[3] = payload[i];
    write_req[4] = payload[i + 1];
    write_req[5] = payload[i + 2];
    write_req[6] = payload[i + 3];
    resp_len = 0;

    bool success = false;
    for (uint8_t retry = 0; retry < 3; retry++) {
      if (this->transceive_nfcv_stream_(write_req, sizeof(write_req), resp, resp_len, 25)) {
        success = true;
        break;
      }
      delay(10);
    }
    if (!success) {
      ESP_LOGE(TAG, "NFC-V: failed to write block %u", blk);
      this->restore_nfca_mode_();
      return false;
    }
  }

  ESP_LOGI(TAG, "NFC-V NDEF write successful!");
  this->restore_nfca_mode_();
  return true;
}

bool ST25R::clean_tag() {
  return this->ndef_write(nullptr, true);
}

bool ST25R::send_apdu(const uint8_t *apdu, size_t apdu_len, uint8_t *resp, uint8_t &resp_len) {
  if (!(this->last_sak_ & 0x20)) {
    ESP_LOGW(TAG, "send_apdu: tag not ISO-DEP capable (SAK=0x%02X)", this->last_sak_);
    return false;
  }
  return this->isodep_transceive_(apdu, apdu_len, resp, resp_len);
}

void ST25R::dump_config() {
  ESP_LOGCONFIG(TAG, "ST25R:");
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  RF Power: %u", this->rf_power_);
  ESP_LOGCONFIG(TAG, "  RF Field Enabled: %s", YESNO(this->rf_field_enabled_));
  ESP_LOGCONFIG(TAG, "  Miss Threshold: %u", this->miss_threshold_);
  ESP_LOGCONFIG(TAG, "  NFC-V (ISO 15693): %s", YESNO(this->nfcv_enabled_));
  ESP_LOGCONFIG(TAG, "  Health Check: %s", YESNO(this->health_check_enabled_));
  if (this->health_check_enabled_) {
    ESP_LOGCONFIG(TAG, "  Health Check Interval: %u ms", this->health_check_interval_ms_);
    ESP_LOGCONFIG(TAG, "  Max Failed Checks: %u", this->max_failed_checks_);
    ESP_LOGCONFIG(TAG, "  Auto Reset on Failure: %s", YESNO(this->auto_reset_on_failure_));
  }
  LOG_UPDATE_INTERVAL(this);
  uint8_t ic_id = this->read_register(IC_IDENTITY);
  ESP_LOGCONFIG(TAG, "  IC Identity (live read): 0x%02X (chip_type=0x%02X)", ic_id, ic_id & 0xF8);
  ESP_LOGCONFIG(TAG, "  IO_CONF2: 0x%02X (sup3V=%s, aat_en=%s)",
                this->read_register(IO_CONF2),
                (this->read_register(IO_CONF2) & 0x80) ? "3.3V" : "5V",
                (this->read_register(IO_CONF2) & 0x20) ? "yes" : "no");
}

bool ST25RBinarySensor::process(const std::string &uid) {
  std::string target_uid = "";
  for (uint8_t b : this->uid_) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", b);
    target_uid += buf;
  }
  if (uid == target_uid) {
    this->publish_state(true);
    this->found_ = true;
    return true;
  }
  return false;
}

}  // namespace st25r
}  // namespace esphome
