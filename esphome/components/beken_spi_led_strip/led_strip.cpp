#include "led_strip.h"

#ifdef USE_BK72XX

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

extern "C" {
#include "rtos_pub.h"
#include "spi.h"
#include "arm_arch.h"
#include "general_dma_pub.h"
#include "gpio_pub.h"
#include "icu_pub.h"
#undef SPI_DAT
#undef SPI_BASE
};

static const uint32_t SPI_TX_DMA_CHANNEL = GDMA_CHANNEL_3;

// TODO: Check if SPI_PERI_CLK_DCO depends on the chip variant
static const uint32_t SPI_PERI_CLK_26M = 26000000;
static const uint32_t SPI_PERI_CLK_DCO = 120000000;

static const uint32_t SPI_BASE = 0x00802700;
static const uint32_t SPI_DAT = SPI_BASE + 3 * 4;
static const uint32_t SPI_CONFIG = SPI_BASE + 1 * 4;

static const uint32_t SPI_TX_EN = 1 << 0;
static const uint32_t CTRL_NSSMD_3 = 1 << 17;
static const uint32_t SPI_TX_FINISH_EN = 1 << 2;
static const uint32_t SPI_RX_FINISH_EN = 1 << 3;

namespace esphome {
namespace beken_spi_led_strip {

static const char *const TAG = "beken_spi_led_strip";

struct spi_data_t {
  SemaphoreHandle_t dma_tx_semaphore;
  volatile bool tx_in_progress;
  bool first_run;
};

static spi_data_t *spi_data = nullptr;

static void set_spi_ctrl_register(unsigned long bit, bool val) {
  uint32_t value = REG_READ(SPI_CTRL);
  if (val == 0) {
    value &= ~bit;
  } else if (val == 1) {
    value |= bit;
  }
  REG_WRITE(SPI_CTRL, value);
}

static void set_spi_config_register(unsigned long bit, bool val) {
  uint32_t value = REG_READ(SPI_CONFIG);
  if (val == 0) {
    value &= ~bit;
  } else if (val == 1) {
    value |= bit;
  }
  REG_WRITE(SPI_CONFIG, value);
}

void spi_dma_tx_enable(bool enable) {
  GDMA_CFG_ST en_cfg;
  set_spi_config_register(SPI_TX_EN, enable ? 1 : 0);
  en_cfg.channel = SPI_TX_DMA_CHANNEL;
  en_cfg.param = enable ? 1 : 0;
  sddev_control(GDMA_DEV_NAME, CMD_GDMA_SET_DMA_ENABLE, &en_cfg);
}

static void spi_set_clock(uint32_t max_hz) {
  int source_clk = 0;
  int spi_clk = 0;
  int div = 0;
  uint32_t param;
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
  div = ((source_clk >> 1) / spi_clk);
  if (div < 2) {
    div = 2;
  } else if (div >= 255) {
    div = 255;
  }
  param = REG_READ(SPI_CTRL);
  param &= ~(SPI_CKR_MASK << SPI_CKR_POSI);
  param |= (div << SPI_CKR_POSI);
  REG_WRITE(SPI_CTRL, param);
  ESP_LOGD(TAG, "target frequency: %d, actual frequency: %d", max_hz, source_clk / 2 / div);
}

void spi_dma_tx_finish_callback(unsigned int param) {
  spi_data->tx_in_progress = false;
  xSemaphoreGive(spi_data->dma_tx_semaphore);
  spi_dma_tx_enable(0);
}

void BekenSPILEDStripLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Beken SPI LED Strip...");

  size_t buffer_size = this->get_buffer_size_();
  size_t dma_buffer_size = (buffer_size * 8) + (2 * 64);

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
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

  spi_data = (spi_data_t *) calloc(1, sizeof(spi_data_t));
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

  set_spi_ctrl_register(MSTEN, 0);
  set_spi_ctrl_register(BIT_WDTH, 0);
  spi_set_clock(this->spi_frequency_);
  set_spi_ctrl_register(CKPOL, 0);
  set_spi_ctrl_register(CKPHA, 0);
  set_spi_ctrl_register(MSTEN, 1);
  set_spi_ctrl_register(SPIEN, 1);

  set_spi_ctrl_register(TXINT_EN, 0);
  set_spi_ctrl_register(RXINT_EN, 0);
  set_spi_config_register(SPI_TX_FINISH_EN, 1);
  set_spi_config_register(SPI_RX_FINISH_EN, 1);
  set_spi_ctrl_register(RXOVR_EN, 0);
  set_spi_ctrl_register(TXOVR_EN, 0);

  value = REG_READ(SPI_CTRL);
  value &= ~CTRL_NSSMD_3;
  value |= (1 << 17);
  REG_WRITE(SPI_CTRL, value);

  value = GFUNC_MODE_SPI_DMA;
  sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &value);
  set_spi_ctrl_register(SPI_S_CS_UP_INT_EN, 0);

  GDMA_CFG_ST en_cfg;
  GDMACFG_TPYES_ST init_cfg;
  memset(&init_cfg, 0, sizeof(GDMACFG_TPYES_ST));

  init_cfg.dstdat_width = 8;
  init_cfg.srcdat_width = 32;
  init_cfg.dstptr_incr = 0;
  init_cfg.srcptr_incr = 1;
  init_cfg.src_start_addr = this->dma_buf_;
  init_cfg.dst_start_addr = (void *) SPI_DAT;  // SPI_DMA_REG4_TXFIFO
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

  spi_dma_tx_enable(0);

  value = REG_READ(SPI_CONFIG);
  value &= ~(0xFFF << 8);
  value |= ((dma_buffer_size & 0xFFF) << 8);
  REG_WRITE(SPI_CONFIG, value);
}

void BekenSPILEDStripLightOutput::set_led_params(uint8_t bit0, uint8_t bit1, uint32_t spi_frequency) {
  this->bit0_ = bit0;
  this->bit1_ = bit1;
  this->spi_frequency_ = spi_frequency;
}

void BekenSPILEDStripLightOutput::write_state(light::LightState *state) {
  // protect from refreshing too often
  uint32_t now = micros();
  if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
    // try again next loop iteration, so that this change won't get lost
    this->schedule_show();
    return;
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  ESP_LOGVV(TAG, "Writing RGB values to bus...");

  if (spi_data == nullptr) {
    ESP_LOGE(TAG, "SPI not initialized");
    this->status_set_warning();
    return;
  }

  if (!spi_data->first_run && !xSemaphoreTake(spi_data->dma_tx_semaphore, 10 / portTICK_PERIOD_MS)) {
    ESP_LOGE(TAG, "Timed out waiting for semaphore");
    return;
  }

  if (spi_data->tx_in_progress) {
    ESP_LOGE(TAG, "tx_in_progress is set");
    this->status_set_warning();
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
  spi_dma_tx_enable(1);

  this->status_clear_warning();
}

light::ESPColorView BekenSPILEDStripLightOutput::get_view_internal(int32_t index) const {
  int32_t r = 0, g = 0, b = 0;
  switch (this->rgb_order_) {
    case ORDER_RGB:
      r = 0;
      g = 1;
      b = 2;
      break;
    case ORDER_RBG:
      r = 0;
      g = 2;
      b = 1;
      break;
    case ORDER_GRB:
      r = 1;
      g = 0;
      b = 2;
      break;
    case ORDER_GBR:
      r = 2;
      g = 0;
      b = 1;
      break;
    case ORDER_BGR:
      r = 2;
      g = 1;
      b = 0;
      break;
    case ORDER_BRG:
      r = 1;
      g = 2;
      b = 0;
      break;
  }
  uint8_t multiplier = this->is_rgbw_ || this->is_wrgb_ ? 4 : 3;
  uint8_t white = this->is_wrgb_ ? 0 : 3;

  return {this->buf_ + (index * multiplier) + r + this->is_wrgb_,
          this->buf_ + (index * multiplier) + g + this->is_wrgb_,
          this->buf_ + (index * multiplier) + b + this->is_wrgb_,
          this->is_rgbw_ || this->is_wrgb_ ? this->buf_ + (index * multiplier) + white : nullptr,
          &this->effect_data_[index],
          &this->correction_};
}

void BekenSPILEDStripLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Beken SPI LED Strip:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  const char *rgb_order;
  switch (this->rgb_order_) {
    case ORDER_RGB:
      rgb_order = "RGB";
      break;
    case ORDER_RBG:
      rgb_order = "RBG";
      break;
    case ORDER_GRB:
      rgb_order = "GRB";
      break;
    case ORDER_GBR:
      rgb_order = "GBR";
      break;
    case ORDER_BGR:
      rgb_order = "BGR";
      break;
    case ORDER_BRG:
      rgb_order = "BRG";
      break;
    default:
      rgb_order = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  RGB Order: %s", rgb_order);
  ESP_LOGCONFIG(TAG, "  Max refresh rate: %" PRIu32, *this->max_refresh_rate_);
  ESP_LOGCONFIG(TAG, "  Number of LEDs: %u", this->num_leds_);
}

float BekenSPILEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace beken_spi_led_strip
}  // namespace esphome

#endif  // USE_BK72XX
