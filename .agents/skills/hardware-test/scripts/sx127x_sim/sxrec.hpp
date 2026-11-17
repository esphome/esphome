#pragma once
// SX127x SPI recorder on an ESP32 (esphome#22776). The same register file as the host harness (burst auto-increment,
// FIFO address fixed, instant mode changes, version 0x12), fed either by
//  - SXREC_SW: a software SPI bus whose pins and the radio's CS are fake (swap at on_boot 1100), or
//  - SXREC_HW: the real ESP-IDF hardware bus. A tee delegate (installed at on_boot 800, before the radio's setup)
//    sends every byte on the wire through the real SPIDelegateHw and gives the model the same bytes; the model
//    answers reads. SPI3 as a slave sniffer (CLK, MOSI and the radio's CS pad routed in through the GPIO matrix,
//    nominal CS GPIO14 held high) captures each chip select window from the wire, up to 64 bytes, and the wire bytes
//    are compared with what the driver sent.
// DIO0 (a real pad, input/output after setup) is high while the model is in TX, so a send completes at once.
// Records are printed as "REC CASE/T/REGS/FIFO/END" lines for compare_spi.py (strip the log prefix first).
#include <driver/gpio.h>
#include <vector>
#include "esphome/components/spi/spi.h"
#include "esphome/components/sx127x/sx127x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#ifdef SXREC_HW
#include "driver/spi_slave.h"
#include "esp_rom_gpio.h"
#include "soc/spi_periph.h"
#endif

namespace sxrec {
using namespace esphome;
static const char *const TAG = "rec";

struct Chip {
  uint8_t regs[128]{};
  std::vector<uint8_t> fifo;
  std::vector<uint8_t> cur;
  std::vector<std::vector<uint8_t>> log;
  uint8_t addr{0};
  uint16_t pos{0};
  int dio{-1};
  bool drive{false};
  Chip() { regs[0x01] = 0x01; }
  uint8_t reply(uint16_t p) {
    if (p == 0 || (addr & 0x80))
      return 0;
    const uint8_t a = addr & 0x7F;
    if (a == 0x42)
      return 0x12;
    if (a == 0x3B)
      return regs[a] & ~0x20;
    return regs[a];
  }
  void byte(uint8_t v) {
    cur.push_back(v);
    if (pos++ == 0) {
      addr = v;
      return;
    }
    if (!(addr & 0x80))
      return;
    const uint8_t a = addr & 0x7F;
    if (a == 0) {
      fifo.push_back(v);
      return;
    }
    regs[a] = v;
    addr = 0x80 | ((a + 1) & 0x7F);
  }
  void begin() {
    cur.clear();
    pos = 0;
  }
  void end() {
    log.push_back(cur);
    if (drive)
      gpio_set_level((gpio_num_t) dio, (regs[0x01] & 7) == 3);
  }
} CHIP;

// ---------------------------------------------------------------- software bus
class NopPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_NONE; }
  bool digital_read() override { return false; }
  void digital_write(bool) override {}
};
static bool MOSI_LEVEL = false;
static uint8_t BIT = 0, IN = 0, OUT = 0;
class MosiPin : public NopPin {
 public:
  void digital_write(bool v) override { MOSI_LEVEL = v; }
};
class CsPin : public NopPin {
 public:
  void digital_write(bool high) override {
    if (!high) {
      CHIP.begin();
      BIT = IN = 0;
    } else {
      CHIP.end();
    }
  }
};
class MisoPin : public NopPin {
 public:
  bool digital_read() override {
    if (BIT == 0)
      OUT = CHIP.reply(CHIP.pos);
    const bool b = (OUT >> (7 - BIT)) & 1;
    IN = (IN << 1) | (MOSI_LEVEL ? 1 : 0);
    if (++BIT == 8) {
      CHIP.byte(IN);
      BIT = 0;
      IN = 0;
    }
    return b;
  }
};
static NopPin CLK;
static MosiPin MOSI;
static MisoPin MISO;
static CsPin CS;

inline void install_sw(spi::SPIComponent *bus, sx127x::SX127x *radio, int dio) {
  bus->set_clk(&CLK);
  bus->set_mosi(&MOSI);
  bus->set_miso(&MISO);
  radio->set_cs_pin(&CS);
  CHIP.dio = dio;
  ESP_LOGI(TAG, "software bus recorder installed");
}

// ---------------------------------------------------------------- hardware bus
#ifdef SXREC_HW
template<typename T, typename T::type M> struct Rob {
  friend typename T::type get(T) { return M; }
};
struct DelegateTag {
  using type = spi::SPIDelegate *spi::SPIClient::*;
  friend type get(DelegateTag);
};
template struct Rob<DelegateTag, &spi::SPIClient::delegate_>;
struct CsTag {
  using type = GPIOPin *spi::SPIClient::*;
  friend type get(CsTag);
};
template struct Rob<CsTag, &spi::SPIClient::cs_>;

static constexpr int SCS = 14;  // nominal slave CS, a spare pad held high
static WORD_ALIGNED_ATTR uint8_t s_rx[64];
static WORD_ALIGNED_ATTR uint8_t s_tx[64];
static int CLK_PAD = 18, MOSI_PAD = 23, CS_PAD = 5;
static uint32_t WIRE_OK = 0, WIRE_BAD = 0, WIRE_LONG = 0, WIRE_FAIL = 0;
static uint32_t IDF_ARRAY_CALLS = 0, IDF_BYTE_CALLS = 0;

inline void slave_setup() {
  spi_bus_config_t b{};
  b.mosi_io_num = -1;
  b.miso_io_num = -1;
  b.sclk_io_num = -1;
  b.quadwp_io_num = -1;
  b.quadhd_io_num = -1;
  b.max_transfer_sz = 64;
  spi_slave_interface_config_t s{};
  s.mode = 0;
  s.spics_io_num = SCS;
  s.queue_size = 1;
  ESP_ERROR_CHECK(spi_slave_initialize(SPI3_HOST, &b, &s, SPI_DMA_DISABLED));
  gpio_set_direction((gpio_num_t) SCS, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level((gpio_num_t) SCS, 1);
  gpio_input_enable((gpio_num_t) CLK_PAD);
  gpio_input_enable((gpio_num_t) MOSI_PAD);
  gpio_input_enable((gpio_num_t) CS_PAD);
  esp_rom_gpio_connect_in_signal(CLK_PAD, spi_periph_signal[SPI3_HOST].spiclk_in, false);
  esp_rom_gpio_connect_in_signal(MOSI_PAD, spi_periph_signal[SPI3_HOST].spid_in, false);
}

class Tee : public spi::SPIDelegate {
 public:
  explicit Tee(spi::SPIDelegate *hw) : hw_(hw) {}
  void begin_transaction() override {
    memset(s_rx, 0, sizeof(s_rx));
    static spi_slave_transaction_t t;
    t = {};
    t.length = 64 * 8;
    t.tx_buffer = s_tx;
    t.rx_buffer = s_rx;
    this->queued_ = spi_slave_queue_trans(SPI3_HOST, &t, pdMS_TO_TICKS(100)) == ESP_OK;
    esp_rom_delay_us(300);
    esp_rom_gpio_connect_in_signal(CS_PAD, spi_periph_signal[SPI3_HOST].spics_in, false);
    CHIP.begin();
    this->hw_->begin_transaction();
  }
  void end_transaction() override {
    this->hw_->end_transaction();
    spi_slave_transaction_t *r;
    int bits = -1;
    if (this->queued_ && spi_slave_get_trans_result(SPI3_HOST, &r, pdMS_TO_TICKS(200)) == ESP_OK)
      bits = r->trans_len;
    const auto &sent = CHIP.cur;
    if (bits < 0) {
      WIRE_FAIL++;
    } else if (sent.size() > 64) {
      WIRE_LONG++;
    } else if ((size_t) bits == sent.size() * 8 && memcmp(s_rx, sent.data(), sent.size()) == 0) {
      WIRE_OK++;
    } else {
      if (WIRE_BAD++ < 10)
        ESP_LOGW(TAG, "wire differs: sent %u bytes, wire %d bits, first sent %02X wire %02X", (unsigned) sent.size(),
                 bits, sent.empty() ? 0 : sent[0], s_rx[0]);
    }
    CHIP.end();
  }
  uint8_t transfer(uint8_t data) override {
    IDF_BYTE_CALLS++;
    this->hw_->transfer(data);
    const uint8_t reply = CHIP.reply(CHIP.pos);
    CHIP.byte(data);
    return reply;
  }
  void write_array(const uint8_t *ptr, size_t length) override {
    IDF_ARRAY_CALLS++;
    this->hw_->write_array(ptr, length);  // the ESP-IDF path under test: one SPI transaction
    for (size_t i = 0; i < length; i++)
      CHIP.byte(ptr[i]);
  }
  void read_array(uint8_t *ptr, size_t length) override {
    uint8_t zeros[64]{};
    for (size_t off = 0; off < length; off += sizeof(zeros))
      this->hw_->write_array(zeros, std::min(sizeof(zeros), length - off));
    for (size_t i = 0; i < length; i++) {
      ptr[i] = CHIP.reply(CHIP.pos);
      CHIP.byte(0);
    }
  }
  bool is_ready() override { return this->hw_->is_ready(); }

 protected:
  spi::SPIDelegate *hw_;
  bool queued_{false};
};

// on_boot 800: after the bus's setup, before the radio's. The radio's own spi_setup() then finds a delegate and keeps
// it
// ("Device already registered" is logged once).
inline void install_hw(spi::SPIComponent *bus, sx127x::SX127x *radio, int dio) {
  auto *client = static_cast<spi::SPIClient *>(radio);
  static spi::SPIClient owner(spi::BIT_ORDER_MSB_FIRST, spi::MODE0, 8000000);
  GPIOPin *cs = client->*get(CsTag());
  spi::SPIDelegate *hw = bus->register_device(&owner, spi::MODE0, spi::BIT_ORDER_MSB_FIRST, 8000000, cs, false, false);
  client->*get(DelegateTag()) = new Tee(hw);
  slave_setup();
  CHIP.dio = dio;
  ESP_LOGI(TAG, "hardware bus tee and SPI3 sniffer installed");
}
#endif

inline void late_pins() {
  gpio_set_direction((gpio_num_t) CHIP.dio, GPIO_MODE_INPUT_OUTPUT);
  CHIP.drive = true;
  gpio_set_level((gpio_num_t) CHIP.dio, 0);
}

inline void dump(const char *label) {
  static int index = 0;
  ESP_LOGI(TAG, "REC CASE %d %s", index++, label);
  char line[400];
  for (auto &t : CHIP.log) {
    size_t o = snprintf(line, sizeof(line), "REC T");
    for (size_t i = 0; i < t.size(); i++) {
      if (o + 4 >= sizeof(line)) {
        ESP_LOGI(TAG, "%s", line);
        o = snprintf(line, sizeof(line), "REC +");
      }
      o += snprintf(line + o, sizeof(line) - o, " %02X", t[i]);
    }
    ESP_LOGI(TAG, "%s", line);
  }
  size_t o = snprintf(line, sizeof(line), "REC REGS");
  for (auto r : CHIP.regs)
    o += snprintf(line + o, sizeof(line) - o, " %02X", r);
  ESP_LOGI(TAG, "%s", line);
  o = snprintf(line, sizeof(line), "REC FIFO");
  for (size_t i = 0; i < CHIP.fifo.size(); i++) {
    if (o + 4 >= sizeof(line)) {
      ESP_LOGI(TAG, "%s", line);
      o = snprintf(line, sizeof(line), "REC +");
    }
    o += snprintf(line + o, sizeof(line) - o, " %02X", CHIP.fifo[i]);
  }
  ESP_LOGI(TAG, "%s", line);
  ESP_LOGI(TAG, "REC END");
  delay(30);       // let the log drain
  App.feed_wdt();  // the sweep runs in one lambda
  CHIP.log.clear();
}

inline void settings(sx127x::SX127x *r, bool lora, uint32_t freq, uint32_t br, uint32_t dev, uint16_t pre) {
  using namespace sx127x;
  r->set_modulation(lora ? MOD_LORA : MOD_FSK);
  r->set_frequency(freq);
  r->set_bandwidth(lora ? SX127X_BW_125_0 : SX127X_BW_50_0);
  r->set_bitrate(br);
  r->set_deviation(dev);
  r->set_preamble_size(pre);
  r->set_packet_mode(true);
  r->set_bitsync(true);
  r->set_payload_length(0);
  r->set_sync_value(lora ? std::vector<uint8_t>{0x12} : std::vector<uint8_t>{0x33, 0x33});
}

// Runs the whole sweep once, from a lambda after boot
inline void sweep(sx127x::SX127x *r) {
  dump("setup from yaml");
  const uint32_t freqs[] = {137000000, 433920000, 868100000, 1020000000};
  const uint32_t rates[] = {500, 4800, 38400, 300000};
  const uint32_t devs[] = {0, 5000, 100000, 600};
  const uint16_t fsk_pre[] = {0, 2, 256, 65535};
  const uint16_t lora_pre[] = {5, 8, 256, 65535};
  char label[96];
  for (bool lora : {false, true}) {
    for (int fi = 0; fi < 4; fi++) {
      for (int pi = 0; pi < 4; pi++) {
        const uint16_t pre = lora ? lora_pre[pi] : fsk_pre[pi];
        const uint32_t br = rates[(fi + pi) % 4], dv = devs[(fi * 3 + pi) % 4];
        settings(r, lora, freqs[fi], br, dv, pre);
        CHIP.log.clear();
        r->configure();
        snprintf(label, sizeof(label), "%s configure f=%u br=%u dev=%u pre=%u failed=%d", lora ? "lora" : "fsk",
                 (unsigned) freqs[fi], (unsigned) br, (unsigned) dv, pre, r->is_failed());
        dump(label);
        if (pi == 0 || pi == 3) {
          for (size_t len : {(size_t) 1, (size_t) 63}) {
            std::vector<uint8_t> pkt(len);
            for (size_t i = 0; i < len; i++)
              pkt[i] = (uint8_t) (i * 13 + len);
            CHIP.fifo.clear();
            auto e = r->transmit_packet(pkt);
            snprintf(label, sizeof(label), "%s send len=%u result=%d", lora ? "lora" : "fsk", (unsigned) len, (int) e);
            dump(label);
          }
        }
      }
    }
  }
#ifdef SXREC_HW
  ESP_LOGI(TAG, "WIRE ok=%u differ=%u over64=%u no_capture=%u write_array_calls=%u transfer_calls=%u",
           (unsigned) WIRE_OK, (unsigned) WIRE_BAD, (unsigned) WIRE_LONG, (unsigned) WIRE_FAIL,
           (unsigned) IDF_ARRAY_CALLS, (unsigned) IDF_BYTE_CALLS);
#endif
  ESP_LOGI(TAG, "SWEEP DONE");
}
}  // namespace sxrec
