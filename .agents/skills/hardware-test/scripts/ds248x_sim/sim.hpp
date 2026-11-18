// DS2482-800 + DS18B20 simulator on an RP2040/RP2350 I2C slave (i2c0, GP4 SDA, GP5 SCL, address 0x18).
// Fault injections are armed from the USB serial console, one character per command:
//   c  NACK the write that follows the next channel select (the read pointer restore)
//   z  after the next 0xBE (read scratchpad) byte, NACK the next 9 writes (every read8 fails)
//   b  stuck busy on the next 1-Wire command, and the next device reset reports no RST bit
//   k  stuck busy on the next 1-Wire command, and NACK the config write after the next device reset
//   x  dead (address NACK)   X  alive again   s  print counters now
// inj_* count a fault when the DUT meets it, not when it is armed; pend=1 while one has not been met yet.
// nack_miss counts writes that delivered data while a NACK was armed, including one followed by a repeated
// start into a read; that NACK stays pending.
// A NACKed write and an address-only write look the same at STOP (no data), so empty_wr counts address-only
// writes outside a NACK window; dspd.yaml turns the bus scan off, so it stays 0 unless the DUT sends them.
// dropped counts writes thrown away by a repeated start into another write before any read or STOP.
// arm_lost counts arms merged into one of the same fault that the DUT had not met yet.
#pragma once
#include <Arduino.h>
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "esphome/core/helpers.h"

namespace sim {

struct Dev {
  uint8_t ch;
  uint8_t rom[8];
  uint8_t sp[9];
};

static Dev devs[4];
static const int NDEV = 4;

static void make_dev(int i, uint8_t ch, uint8_t serial, float temp) {
  Dev &d = devs[i];
  d.ch = ch;
  uint8_t r[8] = {0x28, serial, 0x22, 0x68, 0x00, 0x00, 0x00, 0};
  r[7] = esphome::crc8(r, 7);
  memcpy(d.rom, r, 8);
  int16_t t = (int16_t) (temp * 16);
  uint8_t s[9] = {(uint8_t) (t & 0xFF), (uint8_t) (t >> 8), 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0};
  s[8] = esphome::crc8(s, 8);
  memcpy(d.sp, s, 9);
}

// --- chip state (touched from the IRQ only, counters read from the main loop) ---
enum Rp : uint8_t { RP_STATUS, RP_DATA, RP_CONFIG, RP_CHANNEL };
static volatile uint8_t rp = RP_STATUS, status = 0x18, data = 0, config = 0, channel = 0;
static const uint8_t CH_READ[8] = {0xB8, 0xB1, 0xAA, 0xA3, 0x9C, 0x95, 0x8E, 0x87};
static const uint8_t CH_WRITE[8] = {0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87};

// 1-Wire state
enum Ow : uint8_t { OW_IDLE, OW_ROM, OW_MATCH, OW_FUNC, OW_READSP, OW_WRITESP, OW_SEARCH };
static uint8_t ow = OW_IDLE, mcount = 0, mrom[8], spidx = 0, sbit = 0;
static uint8_t selected = 0, part = 0;  // device bitmasks

// faults
static volatile bool arm_c = false, arm_z = false, arm_b = false, arm_k = false;
static volatile bool stuck = false, bad_reset = false, nack_cfg_after_reset = false;
static volatile uint8_t nack_left = 0;  // writes still to NACK
static volatile bool nack_on = false, z_pending = false, dead = false;
static volatile char nack_src = 0;  // fault whose NACK is pending: 'c', 'z' or 'k'

// counters
struct Cnt {
  uint32_t txn, drst, chsl, owreset, wbyte, rbyte, trip, match, wrong_ch, nacked, cfg_writes, ops_no_apu, convert,
      readsp, inject_c, inject_z, inject_b, inject_k;
  uint32_t cfg_bad, bad_cmd;  // config writes with a wrong complement; unknown, short or overlong commands
  uint32_t tx_abrt;           // read replies the controller aborted, so the DUT did not get them
  uint32_t nack_miss;         // writes that ended with data while a NACK was armed, so the NACK never reached the wire
  uint32_t empty_wr;          // address-only writes while no NACK was armed
  uint32_t dropped;           // writes cut off by a repeated start into another write, never processed
  uint32_t arm_lost;          // arms merged into a pending one of the same fault
};
static volatile Cnt cnt;
static volatile uint8_t last_wrong_rom1 = 0, last_wrong_on = 0;

// trace of the commands after an injection: cmd, arg, channel
static volatile uint8_t tr[24][3];
static volatile uint8_t trn = 24, trlen = 0;
static uint8_t rx[4];  // no command is longer than 2 bytes, so a full rx is an overlong write
static uint8_t rxn = 0;
static bool had_rd = false;

static inline i2c_hw_t *hw() { return i2c_get_hw(i2c0); }

static void ow_cmd_started() {
  // every 1-Wire command; with active_pullup configured the APU bit must be set
  if (!(config & 0x01))
    cnt.ops_no_apu++;
  if (arm_b) {
    arm_b = false;
    stuck = true;
    bad_reset = true;
  } else if (arm_k) {
    arm_k = false;
    stuck = true;
    nack_cfg_after_reset = true;
  }
}

static uint8_t dev_mask_on_channel() {
  uint8_t m = 0;
  for (int i = 0; i < NDEV; i++)
    if (devs[i].ch == channel)
      m |= 1 << i;
  return m;
}

static void ow_write(uint8_t b) {
  cnt.wbyte++;
  switch (ow) {
    case OW_ROM:
      if (b == 0x55) {
        ow = OW_MATCH;
        mcount = 0;
      } else if (b == 0xCC) {
        selected = dev_mask_on_channel();
        ow = OW_FUNC;
      } else if (b == 0xF0) {
        part = dev_mask_on_channel();
        sbit = 0;
        ow = OW_SEARCH;
      } else {
        ow = OW_IDLE;
      }
      break;
    case OW_MATCH:
      mrom[mcount++] = b;
      if (mcount == 8) {
        cnt.match++;
        selected = 0;
        for (int i = 0; i < NDEV; i++) {
          if (memcmp(devs[i].rom, mrom, 8) == 0) {
            if (devs[i].ch == channel) {
              selected |= 1 << i;
            } else {
              cnt.wrong_ch++;
              last_wrong_rom1 = mrom[1];
              last_wrong_on = channel;
            }
          }
        }
        ow = OW_FUNC;
      }
      break;
    case OW_FUNC:
      if (b == 0xBE) {
        cnt.readsp++;
        ow = OW_READSP;
        spidx = 0;
        if (arm_z) {
          arm_z = false;
          if (z_pending || nack_left)
            cnt.arm_lost++;
          z_pending = true;
        }
      } else if (b == 0x4E) {
        ow = OW_WRITESP;
        spidx = 0;
      } else {
        if (b == 0x44)
          cnt.convert++;
        ow = OW_IDLE;
      }
      break;
    case OW_WRITESP:
      for (int i = 0; i < NDEV; i++)
        if (selected & (1 << i)) {
          devs[i].sp[2 + spidx] = b;
          devs[i].sp[8] = esphome::crc8(devs[i].sp, 8);
        }
      if (++spidx == 3)
        ow = OW_IDLE;
      break;
    default:
      break;
  }
}

static uint8_t ow_read() {
  cnt.rbyte++;
  if (ow == OW_READSP && spidx < 9) {
    uint8_t v = 0xFF;
    for (int i = 0; i < NDEV; i++)
      if (selected & (1 << i))
        v &= devs[i].sp[spidx];
    spidx++;
    return v;
  }
  return 0xFF;
}

static uint8_t ow_triplet(bool dir) {
  cnt.trip++;
  uint8_t st = 0;
  if (ow != OW_SEARCH || sbit >= 64) {
    return 0x20 | 0x40 | 0x80;
  }
  bool b = true, c = true;
  for (int i = 0; i < NDEV; i++)
    if (part & (1 << i)) {
      bool bit = devs[i].rom[sbit / 8] & (1 << (sbit % 8));
      b = b && bit;
      c = c && !bit;
    }
  bool d;
  if (b != c)
    d = b;
  else if (!b && !c)
    d = dir;
  else
    d = true;
  for (int i = 0; i < NDEV; i++)
    if (part & (1 << i)) {
      bool bit = devs[i].rom[sbit / 8] & (1 << (sbit % 8));
      if (bit != d)
        part &= ~(1 << i);
    }
  sbit++;
  if (b)
    st |= 0x20;
  if (c)
    st |= 0x40;
  if (d)
    st |= 0x80;
  return st;
}

static void set_status_idle(uint8_t extra) { status = (uint8_t) (0x08 | extra | (stuck ? 0x01 : 0)); }

static void process(const uint8_t *b, uint8_t n) {
  cnt.txn++;
  bool no_arg = b[0] == 0xF0 || b[0] == 0xB4 || b[0] == 0x96;  // device reset, 1-Wire reset, read byte
  if (n > 2 || (no_arg && n != 1)) {
    cnt.bad_cmd++;
    return;
  }
  if (trn < 24 && b[0] != 0xE1) {
    tr[trn][0] = b[0];
    tr[trn][1] = n > 1 ? b[1] : 0;
    tr[trn][2] = channel;
    trn++;
    trlen = trn;
  }
  uint8_t keep = status & 0xE2;  // SBR TSB DIR PPD survive between commands
  switch (b[0]) {
    case 0xF0:  // device reset
      cnt.drst++;
      stuck = false;
      config = 0;
      channel = 0;
      ow = OW_IDLE;
      rp = RP_STATUS;
      if (bad_reset) {
        bad_reset = false;
        status = 0x08;
        cnt.inject_b++;
      } else {
        status = 0x18;
      }
      if (nack_cfg_after_reset) {
        nack_cfg_after_reset = false;
        nack_left = 1;
        nack_src = 'k';
      }
      break;
    case 0xE1:  // set read pointer
      if (n >= 2) {
        switch (b[1]) {
          case 0xF0:
            rp = RP_STATUS;
            break;
          case 0xE1:
            rp = RP_DATA;
            break;
          case 0xC3:
            rp = RP_CONFIG;
            break;
          case 0xD2:
            rp = RP_CHANNEL;
            break;
          default:
            cnt.bad_cmd++;
        }
      } else {
        cnt.bad_cmd++;
      }
      break;
    case 0xD2:  // write config
      if (n >= 2 && ((b[1] >> 4) ^ 0x0F) == (b[1] & 0x0F)) {
        config = b[1] & 0x0F;
        cnt.cfg_writes++;
        status = status & ~0x10;
        rp = RP_CONFIG;
      } else {
        cnt.cfg_bad++;
      }
      break;
    case 0xC3:  // channel select
      if (n >= 2) {
        uint32_t before = cnt.chsl;
        for (int i = 0; i < 8; i++)
          if (CH_WRITE[i] == b[1]) {
            channel = i;
            cnt.chsl++;
          }
        if (cnt.chsl == before)
          cnt.bad_cmd++;
        rp = RP_CHANNEL;
        if (arm_c) {
          arm_c = false;
          if (nack_left)
            cnt.arm_lost++;
          nack_left = 1;
          nack_src = 'c';
          trn = 0;
        }
      } else {
        cnt.bad_cmd++;
      }
      break;
    case 0xB4:  // 1-Wire reset
      cnt.owreset++;
      ow_cmd_started();
      ow = OW_ROM;
      selected = 0;
      set_status_idle(dev_mask_on_channel() ? 0x02 : 0x00);
      rp = RP_STATUS;
      break;
    case 0xA5:  // write byte
      if (n >= 2) {
        ow_cmd_started();
        ow_write(b[1]);
        set_status_idle(keep & 0x02);
        rp = RP_STATUS;
      } else {
        cnt.bad_cmd++;
      }
      break;
    case 0x96:  // read byte
      ow_cmd_started();
      data = ow_read();
      set_status_idle(keep & 0x02);
      rp = RP_STATUS;
      break;
    case 0x78:  // triplet
      if (n >= 2) {
        ow_cmd_started();
        set_status_idle(ow_triplet(b[1] & 0x80));
        rp = RP_STATUS;
      } else {
        cnt.bad_cmd++;
      }
      break;
    default:
      cnt.bad_cmd++;
  }
}

static uint8_t read_reg() {
  switch (rp) {
    case RP_DATA:
      return data;
    case RP_CONFIG:
      return config;
    case RP_CHANNEL:
      return CH_READ[channel & 7];
    default:
      return status;
  }
}

static void drain_rx(i2c_hw_t *h) {
  while (h->rxflr) {
    uint32_t d = h->data_cmd;
    if (d & I2C_IC_DATA_CMD_FIRST_DATA_BYTE_BITS) {
      if (rxn)
        cnt.dropped++;
      rxn = 0;
    }
    if (rxn < sizeof(rx))
      rx[rxn++] = d & 0xFF;
  }
}

static void irq() {
  i2c_hw_t *h = hw();
  uint32_t st = h->intr_stat;
  if (st & I2C_IC_INTR_STAT_R_RX_FULL_BITS)
    drain_rx(h);
  if (st & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
    if (rxn) {
      if (nack_on)  // a write before a repeated start got through the NACK; STOP sees only the read
        cnt.nack_miss++;
      process(rx, rxn);
      rxn = 0;
    }
    had_rd = true;
    h->data_cmd = read_reg();
    (void) h->clr_rd_req;
  }
  if (st & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
    (void) h->clr_tx_abrt;
    cnt.tx_abrt++;
  }
  if (st & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
    drain_rx(h);
    (void) h->clr_stop_det;
    bool was_write = !had_rd;
    bool got_data = rxn > 0;
    if (rxn) {
      process(rx, rxn);
      rxn = 0;
    }
    // a write that got NACKed carries no data; one that delivered data missed the NACK, which stays pending
    if (nack_on && was_write && got_data) {
      cnt.nack_miss++;
    } else if (nack_on && was_write) {
      if (nack_left)
        nack_left--;
      cnt.nacked++;
      if (nack_src == 'c')
        cnt.inject_c++;
      else if (nack_src == 'z')
        cnt.inject_z++;
      else if (nack_src == 'k')
        cnt.inject_k++;
      nack_src = 0;
    } else if (was_write && !got_data) {
      cnt.empty_wr++;
    }
    if (z_pending) {  // armed by 0xBE: start NACKing after that write
      z_pending = false;
      nack_left = 9;
      nack_src = 'z';
    }
    bool want = nack_left > 0;
    if (want != nack_on) {
      nack_on = want;
      h->slv_data_nack_only = want ? 1 : 0;
    }
    had_rd = false;
  }
}

inline void begin() {
  make_dev(0, 0, 0x10, 21.5f);
  make_dev(1, 1, 0x11, 33.0f);
  make_dev(2, 2, 0x12, -5.25f);
  make_dev(3, 2, 0x13, 40.0f);
  i2c_init(i2c0, 100000);
  i2c_set_slave_mode(i2c0, true, 0x18);
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);
  i2c_hw_t *h = hw();
  // STOP only after our address, so a bus recovery STOP from the DUT is not counted as an empty write
  h->enable = 0;
  hw_set_bits(&h->con, I2C_IC_CON_STOP_DET_IFADDRESSED_BITS);
  h->enable = 1;
  h->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS | I2C_IC_INTR_MASK_M_RD_REQ_BITS | I2C_IC_INTR_MASK_M_STOP_DET_BITS |
                 I2C_IC_INTR_MASK_M_TX_ABRT_BITS;
  irq_set_exclusive_handler(I2C0_IRQ, irq);
  irq_set_enabled(I2C0_IRQ, true);
}

inline void set_dead(bool d) {
  dead = d;
  hw()->enable = d ? 0 : 1;
}

inline void report();

// I2C0_IRQ masked, so its own arm_lost++ and arm clears cannot interleave with this one
inline void arm(volatile bool &flag) {
  irq_set_enabled(I2C0_IRQ, false);
  if (flag)
    cnt.arm_lost++;
  flag = true;
  irq_set_enabled(I2C0_IRQ, true);
}

inline void poll_serial() {
  while (Serial.available()) {
    int c = Serial.read();
    switch (c) {
      case 'c':
        arm(arm_c);
        ESP_LOGI("sim", "ARM c");
        break;
      case 'z':
        arm(arm_z);
        ESP_LOGI("sim", "ARM z");
        break;
      case 'b':
        arm(arm_b);
        ESP_LOGI("sim", "ARM b");
        break;
      case 'k':
        arm(arm_k);
        ESP_LOGI("sim", "ARM k");
        break;
      case 'x':
        set_dead(true);
        ESP_LOGI("sim", "DEAD");
        break;
      case 'X':
        set_dead(false);
        ESP_LOGI("sim", "ALIVE");
        break;
      case 's':
        report();
        break;
      default:
        break;
    }
  }
}

inline void report() {
  bool pend = arm_c || arm_z || arm_b || arm_k || stuck || bad_reset || nack_cfg_after_reset || nack_left || z_pending;
  if (trlen) {
    char buf[24 * 9 + 1];
    int o = 0;
    for (int i = 0; i < trlen; i++)
      o += snprintf(buf + o, sizeof(buf) - o, "%02X%02X/%u ", (unsigned) tr[i][0], (unsigned) tr[i][1],
                    (unsigned) tr[i][2]);
    ESP_LOGI("sim", "TRACE %s", buf);
    trlen = 0;
  }
  ESP_LOGI("sim",
           "CNT txn=%u drst=%u chsl=%u owreset=%u wbyte=%u rbyte=%u trip=%u match=%u wrong_ch=%u nacked=%u "
           "nack_miss=%u empty_wr=%u dropped=%u arm_lost=%u cfgw=%u cfg_bad=%u bad_cmd=%u ops_no_apu=%u convert=%u "
           "readsp=%u "
           "inj_c=%u inj_z=%u inj_b=%u inj_k=%u tx_abrt=%u ch=%u cfg=%u dead=%d pend=%d",
           (unsigned) cnt.txn, (unsigned) cnt.drst, (unsigned) cnt.chsl, (unsigned) cnt.owreset, (unsigned) cnt.wbyte,
           (unsigned) cnt.rbyte, (unsigned) cnt.trip, (unsigned) cnt.match, (unsigned) cnt.wrong_ch,
           (unsigned) cnt.nacked, (unsigned) cnt.nack_miss, (unsigned) cnt.empty_wr, (unsigned) cnt.dropped,
           (unsigned) cnt.arm_lost, (unsigned) cnt.cfg_writes, (unsigned) cnt.cfg_bad, (unsigned) cnt.bad_cmd,
           (unsigned) cnt.ops_no_apu, (unsigned) cnt.convert, (unsigned) cnt.readsp, (unsigned) cnt.inject_c,
           (unsigned) cnt.inject_z, (unsigned) cnt.inject_b, (unsigned) cnt.inject_k, (unsigned) cnt.tx_abrt,
           (unsigned) channel, (unsigned) config, (int) dead, (int) pend);
}

}  // namespace sim
