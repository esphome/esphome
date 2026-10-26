#include "led_strip.h"

#ifdef USE_BK72XX

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

extern "C" {
#include "rtos_pub.h"
// rtos_pub.h must be included before the rest of the includes

#include "arm_arch.h"
#include "general_dma_pub.h"
#include "gpio_pub.h"
#include "icu_pub.h"
#include "spi.h"
#undef SPI_DAT
#undef SPI_BASE
};

static const uint32_t SPI_TX_DMA_CHANNEL = GDMA_CHANNEL_3;

static const uint32_t SPI_PERI_CLK_26M = 26000000;

#if defined(USE_LIBRETINY_VARIANT_BK7251)
static const uint32_t SPI_PERI_CLK_DCO = 180000000;
#else
static const uint32_t SPI_PERI_CLK_DCO = 120000000;
#endif

// The beken378 SDK has two SPI register layouts:
//   N-like (BK7231N, BK7238, BK7252N): CTRL +0x00, CONFIG +0x04, STAT +0x08, DAT +0x0C
//   T-like (BK7231T, BK7251):          CTRL +0x00, STAT +0x04, DAT +0x08
// The N-like variants also have TX enable / transfer length / TX finish bits in CONFIG.
// The T-like variants have no CONFIG address.
#if defined(USE_LIBRETINY_VARIANT_BK7231N) || defined(USE_LIBRETINY_VARIANT_BK7238)
#define BEKEN_SPI_IS_N_LIKE
#endif

static const uint32_t SPI_BASE = 0x00802700;
#ifdef BEKEN_SPI_IS_N_LIKE
static const uint32_t SPI_DAT = SPI_BASE + 3 * 4;
static const uint32_t SPI_CONFIG = SPI_BASE + 1 * 4;
static const uint32_t SPI_TX_EN = 1 << 0;
static const uint32_t SPI_TX_FINISH_EN = 1 << 2;
static const uint32_t SPI_RX_FINISH_EN = 1 << 3;
#else
static const uint32_t SPI_DAT = SPI_BASE + 2 * 4;
#endif

static const uint32_t CTRL_NSSMD_3 = 1 << 17;

namespace esphome::beken_spi_led_strip {

ESPHOME_LOG_TAG(TAG, "beken_spi_led_strip");

struct SpiData {
  SemaphoreHandle_t dma_tx_semaphore;
  volatile bool tx_in_progress;
  bool first_run;
};

static SpiData *spi_data = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// BDK 3.0.78 makes REG_WRITE a statement expression that yields the volatile assignment,
// a deprecated volatile read under clang
static inline void reg_write(uint32_t addr, uint32_t value) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr) - MMIO register at a fixed address
  *reinterpret_cast<volatile uint32_t *>(addr) = value;
}

static void set_spi_ctrl_register(uint32_t bit, bool val) {
  uint32_t value = REG_READ(SPI_CTRL);
  if (val == 0) {
    value &= ~bit;
  } else if (val == 1) {
    value |= bit;
  }
  reg_write(SPI_CTRL, value);
}

#ifdef BEKEN_SPI_IS_N_LIKE
static void set_spi_config_register(uint32_t bit, bool val) {
  uint32_t value = REG_READ(SPI_CONFIG);
  if (val == 0) {
    value &= ~bit;
  } else if (val == 1) {
    value |= bit;
  }
  reg_write(SPI_CONFIG, value);
}
#endif

void spi_dma_tx_enable(bool enable) {
  GDMA_CFG_ST en_cfg;
#ifdef BEKEN_SPI_IS_N_LIKE
  set_spi_config_register(SPI_TX_EN, enable);
#endif
  en_cfg.channel = SPI_TX_DMA_CHANNEL;
  en_cfg.param = enable ? 1 : 0;
  sddev_control(GDMA_DEV_NAME, CMD_GDMA_SET_DMA_ENABLE, &en_cfg);
}

static void spi_set_clock(uint32_t max_hz) {
  int source_clk = 0;
  int spi_clk = 0;
  int div = 0;
  uint32_t param = PWD_SPI_CLK_BIT;
  if (max_hz > 4333000) {
    if (max_hz > 30000000) {
      spi_clk = 30000000;
    } else {
      spi_clk = max_hz;
    }
    sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_DOWN, &param);
    source_clk = SPI_PERI_CLK_DCO;
    param = PCLK_POSI_SPI;
    sddev_control(ICU_DEV_NAME, CMD_CONF_PCLK_DCO, &param);
    param = PWD_SPI_CLK_BIT;
    sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_UP, &param);
  } else {
    spi_clk = max_hz;
#if CFG_XTAL_FREQUENCE
    source_clk = CFG_XTAL_FREQUENCE;
#else
    source_clk = SPI_PERI_CLK_26M;
#endif
    param = PCLK_POSI_SPI;
    sddev_control(ICU_DEV_NAME, CMD_CONF_PCLK_26M, &param);
  }
#ifdef BEKEN_SPI_IS_N_LIKE
  // N-like variants: spi_clk = source_clk / (2 * div)
  div = ((source_clk >> 1) / spi_clk);
#else
  // T-like variants: spi_clk = source_clk / (2 * (div + 1))
  div = ((source_clk >> 1) / spi_clk) - 1;
#endif
  if (div < 2) {
    div = 2;
  } else if (div >= 255) {
    div = 255;
  }
  param = REG_READ(SPI_CTRL);
  param &= ~(SPI_CKR_MASK << SPI_CKR_POSI);
  param |= (div << SPI_CKR_POSI);
  reg_write(SPI_CTRL, param);
#ifdef BEKEN_SPI_IS_N_LIKE
  const int divisor = div;
#else
  const int divisor = div + 1;
#endif
  ESP_LOGD(TAG, "target frequency: %" PRIu32 ", actual frequency: %d", max_hz, source_clk / 2 / divisor);
}

void spi_dma_tx_finish_callback(unsigned int param) {
  spi_data->tx_in_progress = false;
  xSemaphoreGive(spi_data->dma_tx_semaphore);
  spi_dma_tx_enable(false);
}

void BekenSPILEDStripLightOutput::setup() {
  size_t buffer_size = this->get_buffer_size_();
  size_t dma_buffer_size = (buffer_size * 8) + (2 * 64);

  RAMAllocator<uint8_t> allocator;
  this->buf_ = allocator.allocate(buffer_size);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate LED buffer!");
    this->mark_failed();
    return;
  }

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate effect data!");
    this->mark_failed();
    return;
  }

  this->dma_buf_ = allocator.allocate(dma_buffer_size);
  if (this->dma_buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate DMA buffer!");
    this->mark_failed();
    return;
  }

  memset(this->buf_, 0, buffer_size);
  memset(this->effect_data_, 0, this->num_leds_);
  memset(this->dma_buf_, 0, dma_buffer_size);

  uint32_t value = PCLK_POSI_SPI;
  sddev_control(ICU_DEV_NAME, CMD_CONF_PCLK_26M, &value);

  value = PWD_SPI_CLK_BIT;
  sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_UP, &value);

  if (spi_data != nullptr) {
    ESP_LOGE(TAG, "SPI device already initialized!");
    this->mark_failed();
    return;
  }

  spi_data = (SpiData *) calloc(1, sizeof(SpiData));  // NOLINT(cppcoreguidelines-no-malloc)
  if (spi_data == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate spi_data!");
    this->mark_failed();
    return;
  }

  spi_data->dma_tx_semaphore = xSemaphoreCreateBinary();
  if (spi_data->dma_tx_semaphore == nullptr) {
    ESP_LOGE(TAG, "TX Semaphore init faild!");
    this->mark_failed();
    return;
  }

  spi_data->first_run = true;

  set_spi_ctrl_register(MSTEN, false);
  set_spi_ctrl_register(BIT_WDTH, false);
  spi_set_clock(this->spi_frequency_);
  set_spi_ctrl_register(CKPOL, false);
  set_spi_ctrl_register(CKPHA, false);
  set_spi_ctrl_register(MSTEN, true);
  set_spi_ctrl_register(SPIEN, true);

  set_spi_ctrl_register(TXINT_EN, false);
  set_spi_ctrl_register(RXINT_EN, false);
#ifdef BEKEN_SPI_IS_N_LIKE
  set_spi_config_register(SPI_TX_FINISH_EN, true);
  set_spi_config_register(SPI_RX_FINISH_EN, true);
#endif
  set_spi_ctrl_register(RXOVR_EN, false);
  set_spi_ctrl_register(TXOVR_EN, false);

  value = REG_READ(SPI_CTRL);
  value &= ~CTRL_NSSMD_3;
  value |= (1 << 17);
  reg_write(SPI_CTRL, value);

#ifdef BEKEN_SPI_IS_N_LIKE
  value = GFUNC_MODE_SPI_DMA;
#else
  // For T-like variants, GFUNC_MODE_SPI_DMA identifies an SPI-DMA peripheral
  // with a different base address. We want the regular SPI controller here.
  value = GFUNC_MODE_SPI;
#endif
  sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &value);
#ifdef BEKEN_SPI_IS_N_LIKE
  set_spi_ctrl_register(SPI_S_CS_UP_INT_EN, false);
#endif

  GDMA_CFG_ST en_cfg;
  GDMACFG_TPYES_ST init_cfg;
  memset(&init_cfg, 0, sizeof(GDMACFG_TPYES_ST));

  init_cfg.dstdat_width = 8;
  init_cfg.srcdat_width = 32;
  init_cfg.dstptr_incr = 0;
  init_cfg.srcptr_incr = 1;
  init_cfg.src_start_addr = this->dma_buf_;
  init_cfg.dst_start_addr = (void *) SPI_DAT;  // NOLINT(performance-no-int-to-ptr) SPI_DMA_REG4_TXFIFO
  init_cfg.channel = SPI_TX_DMA_CHANNEL;
  init_cfg.prio = 0;  // 10
  init_cfg.u.type4.src_loop_start_addr = this->dma_buf_;
  init_cfg.u.type4.src_loop_end_addr = this->dma_buf_ + dma_buffer_size;
  init_cfg.half_fin_handler = nullptr;
  init_cfg.fin_handler = spi_dma_tx_finish_callback;
  init_cfg.src_module = GDMA_X_SRC_DTCM_RD_REQ;
  init_cfg.dst_module = GDMA_X_DST_GSPI_TX_REQ;  // GDMA_X_DST_HSSPI_TX_REQ
  sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_TYPE4, (void *) &init_cfg);
  en_cfg.channel = SPI_TX_DMA_CHANNEL;
  en_cfg.param = dma_buffer_size;
  sddev_control(GDMA_DEV_NAME, CMD_GDMA_SET_TRANS_LENGTH, (void *) &en_cfg);
  en_cfg.channel = SPI_TX_DMA_CHANNEL;
  en_cfg.param = 0;
  sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_WORK_MODE, (void *) &en_cfg);
  en_cfg.channel = SPI_TX_DMA_CHANNEL;
  en_cfg.param = 0;
  sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_SRCADDR_LOOP, &en_cfg);

  spi_dma_tx_enable(false);

#ifdef BEKEN_SPI_IS_N_LIKE
  // N-like variants: The SPI controller must be told how many bytes to clock out.
  value = REG_READ(SPI_CONFIG);
  value &= ~(0xFFF << 8);
  value |= ((dma_buffer_size & 0xFFF) << 8);
  reg_write(SPI_CONFIG, value);
#else
  // T-like variants: No transfer-length register. The SPI controller shifts out
  // everything the DMA writes into SPI_DAT.
#endif

  this->accessor_ =
      this->internal_accessor_.init(this->buf_, this->effect_data_, this->channel_colors_, &this->correction_);
}

void BekenSPILEDStripLightOutput::set_led_params(uint8_t bit0, uint8_t bit1, uint32_t spi_frequency) {
  this->bit0_ = bit0;
  this->bit1_ = bit1;
  this->spi_frequency_ = spi_frequency;
}

void BekenSPILEDStripLightOutput::write_state(light::LightState *state) {
  uint32_t now = micros();
  if (this->throttle_show_(now, this->last_refresh_, this->max_refresh_rate_))
    return;
  this->last_refresh_ = now;
  this->mark_shown_();

  ESP_LOGVV(TAG, "Writing RGB values to bus");

  if (spi_data == nullptr) {
    ESP_LOGE(TAG, "SPI not initialized");
    this->status_set_warning();
    return;
  }

  if (!spi_data->first_run && !xSemaphoreTake(spi_data->dma_tx_semaphore, 10 / portTICK_PERIOD_MS)) {
    // Recover so the next update can try again instead of being stuck forever.
    ESP_LOGE(TAG, "Timed out waiting for semaphore");
    spi_dma_tx_enable(false);
    spi_data->tx_in_progress = false;
    xSemaphoreGive(spi_data->dma_tx_semaphore);
    this->status_set_warning();
    this->schedule_show();
    return;
  }

#ifndef BEKEN_SPI_IS_N_LIKE
  if (!spi_data->first_run) {
    // For T-like variants, the GDMA finish interrupt only means that all bytes were written into
    // the SPI TX FIFO. We must still wait for it to drain before refilling it.
    uint32_t wait_start = micros();
    while (!(REG_READ(SPI_STAT) & TXFIFO_EMPTY)) {
      if (micros() - wait_start > 2000) {
        ESP_LOGW(TAG, "SPI TX FIFO did not drain in time");
        break;
      }
    }
  }
#endif

  if (spi_data->tx_in_progress) {
    ESP_LOGE(TAG, "tx_in_progress is set");
    xSemaphoreGive(spi_data->dma_tx_semaphore);
    this->status_set_warning();
    this->schedule_show();
    return;
  }

  spi_data->tx_in_progress = true;

  size_t buffer_size = this->get_buffer_size_();
  size_t size = 0;
  uint8_t *psrc = this->buf_;
  uint8_t *pdest = this->dma_buf_ + 64;
  // The 64 byte padding is a workaround for a SPI DMA bug where the
  // output doesn't exactly start at the beginning of dma_buf_

  while (size < buffer_size) {
    uint8_t b = *psrc;
    for (int i = 0; i < 8; i++) {
      *pdest++ = b & (1 << (7 - i)) ? this->bit1_ : this->bit0_;
    }
    size++;
    psrc++;
  }

  spi_data->first_run = false;
  spi_dma_tx_enable(true);

  this->status_clear_warning();
}

void BekenSPILEDStripLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Beken SPI LED Strip:\n"
                "  Pin: %u",
                this->pin_);
  char channel_colors[5];
  ESP_LOGCONFIG(TAG,
                "  Channel colors: %s\n"
                "  Max refresh rate: %" PRIu32 "\n"
                "  Number of LEDs: %u",
                this->channel_colors_.to_string(channel_colors), this->max_refresh_rate_, this->num_leds_);
}

float BekenSPILEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esphome::beken_spi_led_strip

#endif  // USE_BK72XX
