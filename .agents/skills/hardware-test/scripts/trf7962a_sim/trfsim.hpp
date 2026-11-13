#pragma once
// Fake TRF7962A behind the SPI delegates: a FakeBus replaces the SPI component's bus before the reader's setup(),
// so both of the driver's SPI clients (MODE0 writes, MODE1 reads) talk to one register/FIFO/IRQ model.
#include <driver/gpio.h>
#include <soc/gpio_reg.h>
#include "esphome/components/spi/spi.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace trfsim {
static constexpr const char *const TAG = "trfsim";

// Scenario of the tag in the field
enum Tag : uint8_t { NONE, TAG_A, TAG_B, SLOW_A, FLAKY_A, COLLIDE, CRC_ERR, ERR_FLAG, STUCK_IRQ, BAD_MSB, TX_ONLY };
static const uint8_t UID_A[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0xE0};  // LSB first: E0-04-01-00-00-00-00-01
static const uint8_t UID_B[8] = {0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0x07, 0xE0};  // E0-07-12-34-56-78-9A-BC

struct Chip {
  uint8_t reg[32]{};
  uint8_t fifo[12]{};
  uint8_t fifo_n{0}, fifo_rd{0};
  uint8_t irq{0};
  // pending events in micros(); 0 = none
  uint32_t t_tx_end{0}, t_rx{0}, t_nr{0};
  uint8_t rx_irq{0};
  uint8_t rx_data[10]{};
  bool tx_armed{false};
  uint16_t tx_len{0};
  uint8_t tx_got{0};
  uint8_t tx_buf[8]{};
  // frame parser
  bool in_frame{false};
  bool have_addr{false};
  uint8_t addr{0};
  bool rd{false}, cont{false};
  uint32_t field_on_us{0};
  // scenario and stats
  Tag tag{NONE};
  uint32_t polls{0}, cs_bad{0}, tx_field_off{0}, tx_early{0}, bad_tx{0}, max_field_ms{0}, flaky_n{0};
  int irq_gpio{-1}, cs_gpio{-1};
} C;

inline void soft_init() {
  memset(C.reg, 0, sizeof(C.reg));
  C.reg[0x01] = 0x21;  // ISO control reset value
  C.reg[0x07] = 0x0E;
  C.reg[0x08] = 0x1F;
  C.reg[0x09] = 0x91;
  C.reg[0x0D] = 0x3E;  // IRQ mask reset value: no-response masked
  C.fifo_n = C.fifo_rd = 0;
  C.irq = 0;
  C.t_tx_end = C.t_rx = C.t_nr = 0;
}

inline void set_irq_pin() {
  static int last = -1;  // tick() runs every millisecond; write the pad only when the level changes
  const int level = C.irq != 0;
  if (C.irq_gpio >= 0 && level != last) {
    gpio_set_level((gpio_num_t) C.irq_gpio, level);
    last = level;
  }
}

inline bool due(uint32_t t, uint32_t now) { return t != 0 && (int32_t) (now - t) >= 0; }

inline void tick() {
  const uint32_t now = esphome::micros();
  if (due(C.t_tx_end, now)) {
    C.t_tx_end = 0;
    C.irq |= 0x80;
  }
  if (due(C.t_rx, now)) {
    C.t_rx = 0;
    C.irq |= C.rx_irq;
    if (C.rx_irq & 0x40) {
      memcpy(C.fifo, C.rx_data, 10);
      C.fifo_n = 10;
      C.fifo_rd = 0;
    }
  }
  if (due(C.t_nr, now)) {
    C.t_nr = 0;
    if (C.reg[0x0D] & 0x01)
      C.irq |= 0x01;
  }
  set_irq_pin();
}

inline void start_tx() {
  const uint32_t now = esphome::micros();
  C.polls++;
  if (!(C.reg[0x00] & 0x20))
    C.tx_field_off++;
  else if (now - C.field_on_us < 5000)
    C.tx_early++;  // field on for less than 5 ms
  if (!(C.tx_len == 3 && C.tx_buf[0] == 0x26 && C.tx_buf[1] == 0x01 && C.tx_buf[2] == 0x00))
    C.bad_tx++;
  C.t_tx_end = now + 1500;
  const uint8_t *uid = nullptr;
  uint8_t flags = 0x00;
  uint32_t rx_delay = 4500;
  C.rx_irq = 0x40;
  switch (C.tag) {
    case TAG_A:
      uid = UID_A;
      break;
    case TAG_B:
      uid = UID_B;
      break;
    case SLOW_A:
      uid = UID_A;
      rx_delay = 22000;
      break;  // TX IRQ is read alone first
    case FLAKY_A:
      uid = (++C.flaky_n % 3 == 0) ? nullptr : UID_A;
      break;  // 2 of every 3 polls answer
    case COLLIDE:
      uid = UID_A;
      C.rx_irq = 0x40 | 0x02;
      break;
    case CRC_ERR:
      uid = UID_A;
      C.rx_irq = 0x40 | 0x10;
      break;
    case ERR_FLAG:
      uid = UID_A;
      flags = 0x01;
      break;
    case BAD_MSB:
      uid = UID_A;
      break;
    default:
      break;
  }
  if (C.tag == STUCK_IRQ || C.tag == TX_ONLY) {
    // only the TX interrupt; STUCK_IRQ also never clears the IRQ line
    return;
  }
  if (uid == nullptr) {
    C.t_nr = C.t_tx_end + (uint32_t) C.reg[0x07] * 38;
    return;
  }
  C.rx_data[0] = flags;
  C.rx_data[1] = 0x00;
  memcpy(C.rx_data + 2, uid, 8);
  if (C.tag == BAD_MSB)
    C.rx_data[9] = 0x60;
  C.t_rx = C.t_tx_end + rx_delay;
}

inline void command(uint8_t cmd) {
  switch (cmd) {
    case 0x00:
      break;
    case 0x03:
      soft_init();
      break;
    case 0x0F:
      C.fifo_n = C.fifo_rd = 0;
      break;
    case 0x11:
      C.tx_armed = true;
      C.tx_got = 0;
      C.fifo_n = C.fifo_rd = 0;
      break;
    default:
      ESP_LOGW(TAG, "unexpected command 0x%02X", cmd);
      break;
  }
}

inline void write_reg(uint8_t r, uint8_t v) {
  if (r == 0x1F) {  // FIFO
    if (C.tx_armed && C.tx_got < sizeof(C.tx_buf)) {
      C.tx_buf[C.tx_got++] = v;
      C.tx_len = ((uint16_t) C.reg[0x1D] << 4) | (C.reg[0x1E] >> 4);
      if (C.tx_got == C.tx_len) {
        C.tx_armed = false;
        start_tx();
      }
    } else {
      // a FIFO write with no transmit armed, or past the 8 byte TX buffer: a malformed transmission
      C.bad_tx++;
      ESP_LOGW(TAG, "dropped FIFO byte 0x%02X (armed=%d got=%u len=%u)", v, C.tx_armed, C.tx_got, C.tx_len);
    }
    return;
  }
  if (r == 0x00) {
    const bool was = C.reg[0] & 0x20, on = v & 0x20;
    if (on && !was)
      C.field_on_us = esphome::micros();
    if (!on && was) {
      uint32_t ms = (esphome::micros() - C.field_on_us) / 1000;
      if (ms > C.max_field_ms)
        C.max_field_ms = ms;
    }
  }
  if (r == 0x01) {  // writing ISO control reloads the wait time presets
    C.reg[0x07] = 0x0E;
    C.reg[0x08] = 0x1F;
  }
  C.reg[r] = v;
}

inline uint8_t read_reg(uint8_t r) {
  if (r == 0x1F)
    return C.fifo_rd < C.fifo_n ? C.fifo[C.fifo_rd++] : 0;
  if (r == 0x1C)
    return C.fifo_n > C.fifo_rd ? (uint8_t) (C.fifo_n - C.fifo_rd - 1) : 0;
  if (r == 0x0C) {
    uint8_t v = C.irq;
    if (C.tag != STUCK_IRQ)
      C.irq = 0;
    return v;
  }
  return C.reg[r];
}

inline bool cs_low() {
  if (C.cs_gpio < 0)
    return true;
  return !((C.cs_gpio < 32 ? REG_READ(GPIO_OUT_REG) >> C.cs_gpio : REG_READ(GPIO_OUT1_REG) >> (C.cs_gpio - 32)) & 1);
}

class FakeDelegate : public esphome::spi::SPIDelegate {
 public:
  FakeDelegate(bool reader)
      : SPIDelegate(2000000, esphome::spi::BIT_ORDER_MSB_FIRST, reader ? esphome::spi::MODE1 : esphome::spi::MODE0,
                    nullptr),
        reader_(reader) {}
  void begin_transaction() override {
    tick();
    if (!cs_low())
      C.cs_bad++;
    if (!this->reader_) {  // every CS window starts with a write
      C.in_frame = true;
      C.have_addr = false;
    }
  }
  void end_transaction() override {
    if (this->reader_)
      C.in_frame = false;
    tick();
  }
  uint8_t transfer(uint8_t d) override {
    tick();
    if (this->reader_) {
      if (!C.have_addr || !C.rd)
        return 0xFF;
      uint8_t v = read_reg(C.addr);
      if (C.cont && C.addr != 0x1F && C.addr != 0x0C)
        C.addr++;
      return v;
    }
    if (!C.have_addr) {
      this->addr_byte_(d);
      return 0;
    }
    if (C.rd)
      return 0;  // writing while reading: ignored
    write_reg(C.addr, d);
    if (C.cont) {
      if (C.addr != 0x1F)
        C.addr++;
    } else {
      C.have_addr = false;  // single write done; the next byte is a new address or command
    }
    return 0;
  }

 protected:
  void addr_byte_(uint8_t d) {
    if (d & 0x80) {
      command(d & 0x1F);
      return;  // the next byte is a new address
    }
    C.have_addr = true;
    C.addr = d & 0x1F;
    C.rd = d & 0x40;
    C.cont = d & 0x20;
  }
  bool reader_;
};

class FakeBus : public esphome::spi::SPIBus {
 public:
  esphome::spi::SPIDelegate *get_delegate(uint32_t data_rate, esphome::spi::SPIBitOrder bit_order,
                                          esphome::spi::SPIMode mode, esphome::GPIOPin *cs_pin, bool release_device,
                                          bool write_only) override {
    ESP_LOGI(TAG, "delegate mode %u rate %u", (unsigned) mode, (unsigned) data_rate);
    return new FakeDelegate(mode == esphome::spi::MODE1);
  }
};

// Reach SPIComponent::spi_bus_ (the class is final)
template<typename T, typename T::type M> struct Rob {
  friend typename T::type get(T) { return M; }
};
struct BusTag {
  using type = esphome::spi::SPIBus *esphome::spi::SPIComponent::*;
  friend type get(BusTag);
};
template struct Rob<BusTag, &esphome::spi::SPIComponent::spi_bus_>;

inline void install(esphome::spi::SPIComponent *spi, int cs_gpio, int irq_gpio) {
  spi->*get(BusTag()) = new FakeBus();
  C.cs_gpio = cs_gpio;
  C.irq_gpio = irq_gpio;
  soft_init();
  ESP_LOGI(TAG, "fake TRF7962A installed");
}

inline void late_pin_setup() {
  // the reader configured the IRQ pad as input; also drive it so the chip model sets its level
  gpio_set_direction((gpio_num_t) C.irq_gpio, GPIO_MODE_INPUT_OUTPUT);
  set_irq_pin();
}

// Scenario sequence for the scenarios script: each phase sets the tag and lasts MS milliseconds
struct Phase {
  const char *name;
  Tag tag;
  uint16_t ms;
};
static const Phase PHASES[] = {
    {"none", NONE, 4000},   {"tagA", TAG_A, 4000},          {"slowA", SLOW_A, 4000}, {"flakyA", FLAKY_A, 6000},
    {"none2", NONE, 4000},  {"tagB", TAG_B, 3000},          {"swapA", TAG_A, 3000},  {"collision", COLLIDE, 4000},
    {"tagA2", TAG_A, 3000}, {"crc_err", CRC_ERR, 4000},     {"tagA3", TAG_A, 3000},  {"err_flag", ERR_FLAG, 4000},
    {"tagA4", TAG_A, 3000}, {"bad_msb", BAD_MSB, 4000},     {"tagA5", TAG_A, 3000},  {"tx_only", TX_ONLY, 4000},
    {"tagA6", TAG_A, 3000}, {"stuck_irq", STUCK_IRQ, 4000}, {"end", NONE, 3000},
};
static constexpr int NUM_PHASES = sizeof(PHASES) / sizeof(PHASES[0]);

inline void stats(const char *phase);

// Log the counters of the phase that ended and start phase I; returns its length in ms
inline uint32_t start_phase(int i) {
  if (i > 0) {
    stats(PHASES[i - 1].name);
    if (PHASES[i - 1].tag == STUCK_IRQ)
      C.irq = 0;
  }
  C.tag = PHASES[i].tag;
  ESP_LOGI("ev", "PHASE %s", PHASES[i].name);
  return PHASES[i].ms;
}

inline void stats(const char *phase) {
  ESP_LOGI(TAG,
           "STATS %s polls=%u cs_bad=%u tx_field_off=%u tx_early=%u bad_tx=%u max_field_ms=%u reg01=%02X reg07=%02X "
           "reg09=%02X reg0D=%02X reg00=%02X",
           phase, (unsigned) C.polls, (unsigned) C.cs_bad, (unsigned) C.tx_field_off, (unsigned) C.tx_early,
           (unsigned) C.bad_tx, (unsigned) C.max_field_ms, C.reg[1], C.reg[7], C.reg[9], C.reg[0x0D], C.reg[0]);
  C.polls = 0;
  C.max_field_ms = 0;
}
}  // namespace trfsim
