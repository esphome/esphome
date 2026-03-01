// Copyright (c) 2019-2022 Valentin Roland (github.com/vroland)
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Original source: https://github.com/vroland/epdiy
// Modified by Xinyuan-LilyGO (GPL-3.0): https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
// Modified for ESPHome: added LCD clock source for ESP-IDF 5.x compatibility

/******************************************************************************/
/***        include files                                                   ***/
/******************************************************************************/

#include "i2s_data_bus.h"

#include <driver/periph_ctrl.h>
#include <esp_heap_caps.h>
#include <rom/lldesc.h>
#include <soc/i2s_reg.h>
#include <soc/i2s_struct.h>
#include <soc/rtc.h>
#include "esp_lcd_panel_io.h"
#include "hal/lcd_types.h"
#include "esp_err.h"
#include "esp_log.h"

/******************************************************************************/
/***        macro definitions                                               ***/
/******************************************************************************/

#define USER_I2S_REG 0

/******************************************************************************/
/***        type definitions                                                ***/
/******************************************************************************/

static const char *TAG = "I80";

/// DMA descriptors for front and back line buffer.
/// We use two buffers, so one can be filled while the other
/// is transmitted.
typedef struct {
  volatile lldesc_t *dma_desc_a;
  volatile lldesc_t *dma_desc_b;

  /// Front and back line buffer.
  uint8_t *buf_a;
  uint8_t *buf_b;
} i2s_parallel_state_t;

static esp_lcd_panel_io_handle_t io_handle = NULL;

/******************************************************************************/
/***        local function prototypes                                       ***/
/******************************************************************************/
#if USER_I2S_REG

/**
 * @brief Initializes a DMA descriptor.
 */
static void fill_dma_desc(volatile lldesc_t *dmadesc, uint8_t *buf, i2s_bus_config *cfg);

/**
 * @brief Address of the currently front DMA descriptor, which uses only the
 *        lower 20bits (according to TRM)
 */
static uint32_t dma_desc_addr();

/**
 * @brief Set up a GPIO as output and route it to a signal.
 */
static void gpio_setup_out(int32_t gpio, int32_t sig, bool invert);
#endif

#if USER_I2S_REG
/**
 * @brief Resets "Start Pulse" signal when the current row output is done.
 */
static void IRAM_ATTR i2s_int_hdl(void *arg);
#endif

/******************************************************************************/
/***        exported variables                                              ***/
/******************************************************************************/

/******************************************************************************/
/***        local variables                                                 ***/
/******************************************************************************/
#if USER_I2S_REG
/**
 * @brief Indicates which line buffer is currently back / front.
 */
static int32_t current_buffer = 0;
#endif

/**
 * @brief The I2S state instance.
 */
static i2s_parallel_state_t i2s_state;

static intr_handle_t gI2S_intr_handle = NULL;

/**
 * @brief Indicates the device has finished its transmission and is ready again.
 */
static volatile bool output_done = true;

/**
 * @brief The start pulse pin extracted from the configuration for use in
 *        the "done" interrupt.
 */
// static gpio_num_t start_pulse_pin;

static uint8_t buffer[(960 + 32) / 4] = {0};

/******************************************************************************/
/***        exported functions                                              ***/
/******************************************************************************/

#if USER_I2S_REG
volatile uint8_t IRAM_ATTR *i2s_get_current_buffer() {
  return current_buffer ? i2s_state.dma_desc_a->buf : i2s_state.dma_desc_b->buf;
}
#else
volatile uint8_t IRAM_ATTR *i2s_get_current_buffer() { return buffer; }
#endif

#if USER_I2S_REG
bool IRAM_ATTR i2s_is_busy() {
  // DMA and FIFO must be done
#ifdef CONFIG_IDF_TARGET_ESP32
  return !output_done || !I2S1.state.tx_idle;
#elif CONFIG_IDF_TARGET_ESP32S3
  // i2s_dev_t *dev = &I2S0;
  return !output_done || !I2S1.state.tx_idle;
#else
#error "Unknown SOC"
#endif
}
#else
bool IRAM_ATTR i2s_is_busy() { return !output_done; }
#endif

#if USER_I2S_REG
void IRAM_ATTR i2s_switch_buffer() {
  // either device is done transmitting or the switch must be away from the
  // buffer currently used by the DMA engine.
#if CONFIG_IDF_TARGET_ESP32
  while (i2s_is_busy() && dma_desc_addr() != I2S1.out_link.addr)
    ;
#elif CONFIG_IDF_TARGET_ESP32S3
  i2s_dev_t *dev = &I2S0;
#else
#error "Unknown SOC"
#endif
  current_buffer = !current_buffer;
}
#else
void IRAM_ATTR i2s_switch_buffer() {}
#endif

#if USER_I2S_REG
void IRAM_ATTR i2s_start_line_output() {
  output_done = false;

#ifdef CONFIG_IDF_TARGET_ESP32
  i2s_dev_t *dev = &I2S1;
#elif CONFIG_IDF_TARGET_ESP32S3
  i2s_dev_t *dev = &I2S0;
#else
#error "Unknown SOC"
#endif
  dev->conf.tx_start = 0;
  dev->conf.tx_reset = 1;
  dev->conf.tx_fifo_reset = 1;
  dev->conf.rx_fifo_reset = 1;
  dev->conf.tx_reset = 0;
  dev->conf.tx_fifo_reset = 0;
  dev->conf.rx_fifo_reset = 0;
  dev->out_link.addr = dma_desc_addr();
  dev->out_link.start = 1;

  // sth is pulled up through peripheral interrupt
  gpio_set_level(start_pulse_pin, 0);
  dev->conf.tx_start = 1;
}
#else
void IRAM_ATTR i2s_start_line_output() {
  output_done = false;

  esp_lcd_panel_io_tx_color(io_handle, 0, buffer, (960 + 32) / 4);
}
#endif

#if !USER_I2S_REG
static bool notify_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                              void *user_ctx) {
  // gpio_set_level(start_pulse_pin, 1);
  output_done = true;
  return output_done;
}
#endif

#if USER_I2S_REG
void i2s_bus_init(i2s_bus_config *cfg) {
  // TODO: Why?
  gpio_num_t I2S_GPIO_BUS[] = {cfg->data_6, cfg->data_7, cfg->data_4, cfg->data_5,
                               cfg->data_2, cfg->data_3, cfg->data_0, cfg->data_1};

  gpio_set_direction(cfg->start_pulse, GPIO_MODE_OUTPUT);
  gpio_set_level(cfg->start_pulse, 1);
  // store pin in global variable for use in interrupt.
  start_pulse_pin = cfg->start_pulse;

  // Use I2S1 with no signal offset (for some reason the offset seems to be
  // needed in 16-bit mode, but not in 8 bit mode.
  int32_t signal_base = I2S1O_DATA_OUT0_IDX;

  // Setup and route GPIOS
  for (int32_t x = 0; x < 8; x++) {
    gpio_setup_out(I2S_GPIO_BUS[x], signal_base + x, false);
  }
  // Invert word select signal
  gpio_setup_out(cfg->clock, I2S1O_WS_OUT_IDX, true);

  periph_module_enable(PERIPH_I2S1_MODULE);

#ifdef CONFIG_IDF_TARGET_ESP32
  i2s_dev_t *dev = &I2S1;
#elif CONFIG_IDF_TARGET_ESP32S3
  i2s_dev_t *dev = &I2S0;
#else
#error "Unknown SOC"
#endif

  // Initialize device
  dev->conf.tx_reset = 1;
  dev->conf.tx_reset = 0;

  // Reset DMA
  dev->lc_conf.in_rst = 1;
  dev->lc_conf.in_rst = 0;
  dev->lc_conf.out_rst = 1;
  dev->lc_conf.out_rst = 0;

  // Setup I2S config. See section 12 of Technical Reference Manual
  // Enable LCD mode
  dev->conf2.val = 0;
  dev->conf2.lcd_en = 1;

  // Enable FRAME1-Mode (See technical reference manual)
  dev->conf2.lcd_tx_wrx2_en = 1;
  dev->conf2.lcd_tx_sdx2_en = 0;

  // Set to 8 bit parallel output
  dev->sample_rate_conf.val = 0;
  dev->sample_rate_conf.tx_bits_mod = 8;

  // Half speed of bit clock in LCD mode.
  // (Smallest possible divider according to the spec).
  dev->sample_rate_conf.tx_bck_div_num = 2;

  //#if defined(CONFIG_EPD_DISPLAY_TYPE_ED097OC4_LQ)
  // Initialize Audio Clock (APLL) for 120 Mhz.
  rtc_clk_apll_enable(1, 0, 0, 8, 0);
  //#else
  // Initialize Audio Clock (APLL) for 80 Mhz.
  // rtc_clk_apll_enable(1, 0, 0, 8, 1);
  //#endif

  // Set Audio Clock Dividers
  dev->clkm_conf.val = 0;
  dev->clkm_conf.clka_en = 1;
  dev->clkm_conf.clkm_div_a = 1;
  dev->clkm_conf.clkm_div_b = 0;
  // 2 is the smallest possible divider according to the spec.
  dev->clkm_conf.clkm_div_num = 2;

  // Set up FIFO
  dev->fifo_conf.val = 0;
  dev->fifo_conf.tx_fifo_mod_force_en = 1;
  dev->fifo_conf.tx_fifo_mod = 1;
  dev->fifo_conf.tx_data_num = 32;
  dev->fifo_conf.dscr_en = 1;

  // Stop after transmission complete
  dev->conf1.val = 0;
  dev->conf1.tx_stop_en = 1;
  dev->conf1.tx_pcm_bypass = 1;

  // Configure TX channel
  dev->conf_chan.val = 0;
  dev->conf_chan.tx_chan_mod = 1;
  dev->conf.tx_right_first = 1;

  dev->timing.val = 0;

  // Allocate DMA descriptors
  i2s_state.buf_a = heap_caps_malloc(cfg->epd_row_width / 4, MALLOC_CAP_DMA);
  i2s_state.buf_b = heap_caps_malloc(cfg->epd_row_width / 4, MALLOC_CAP_DMA);
  i2s_state.dma_desc_a = heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
  i2s_state.dma_desc_b = heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);

  // and fill them
  fill_dma_desc(i2s_state.dma_desc_a, i2s_state.buf_a, cfg);
  fill_dma_desc(i2s_state.dma_desc_b, i2s_state.buf_b, cfg);

  // enable "done" interrupt
  SET_PERI_REG_BITS(I2S_INT_ENA_REG(1), I2S_OUT_DONE_INT_ENA_V, 1, I2S_OUT_DONE_INT_ENA_S);
  // register interrupt
  esp_intr_alloc(ETS_I2S1_INTR_SOURCE, 0, i2s_int_hdl, 0, &gI2S_intr_handle);

  // Reset FIFO/DMA
  dev->lc_conf.in_rst = 1;
  dev->lc_conf.out_rst = 1;
  dev->lc_conf.ahbm_rst = 1;
  dev->lc_conf.ahbm_fifo_rst = 1;
  dev->lc_conf.in_rst = 0;
  dev->lc_conf.out_rst = 0;
  dev->lc_conf.ahbm_rst = 0;
  dev->lc_conf.ahbm_fifo_rst = 0;
  dev->conf.tx_reset = 1;
  dev->conf.tx_fifo_reset = 1;
  dev->conf.rx_fifo_reset = 1;
  dev->conf.tx_reset = 0;
  dev->conf.tx_fifo_reset = 0;
  dev->conf.rx_fifo_reset = 0;

  // Start dma on front buffer
  dev->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;
  dev->out_link.addr = ((uint32_t) (i2s_state.dma_desc_a));
  dev->out_link.start = 1;

  dev->int_clr.val = dev->int_raw.val;

  dev->int_ena.val = 0;
  dev->int_ena.out_done = 1;

  dev->conf.tx_start = 0;
}
#else
void i2s_bus_init(i2s_bus_config *cfg) {
  // TODO: Why?
  // gpio_num_t I2S_GPIO_BUS[] = {cfg->data_6, cfg->data_7, cfg->data_4,
  //                              cfg->data_5, cfg->data_2, cfg->data_3,
  //                              cfg->data_0, cfg->data_1};

  // gpio_set_direction(cfg->start_pulse, GPIO_MODE_OUTPUT);
  // gpio_set_level(cfg->start_pulse, 1);
  // // store pin in global variable for use in interrupt.
  // start_pulse_pin = cfg->start_pulse;

  ESP_LOGI(TAG, "Initialize Intel 8080 bus");
  esp_lcd_i80_bus_handle_t i80_bus = NULL;
  esp_lcd_i80_bus_config_t bus_config = {.clk_src = LCD_CLK_SRC_DEFAULT,
                                         .dc_gpio_num = cfg->start_pulse,
                                         .wr_gpio_num = cfg->clock,
                                         .data_gpio_nums =
                                             {
                                                 cfg->data_6,
                                                 cfg->data_7,
                                                 cfg->data_4,
                                                 cfg->data_5,
                                                 cfg->data_2,
                                                 cfg->data_3,
                                                 cfg->data_0,
                                                 cfg->data_1,
                                             },
                                         .bus_width = 8,
                                         .max_transfer_bytes = (cfg->epd_row_width + 32) / 4};
  ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

  esp_lcd_panel_io_i80_config_t io_config = {
      .cs_gpio_num = -1,
      .pclk_hz = 10 * 1000 * 1000,
      .trans_queue_depth = 10,
      .dc_levels =
          {
              .dc_idle_level = 0,
              .dc_cmd_level = 1,
              .dc_dummy_level = 0,
              .dc_data_level = 0,
          },
      .on_color_trans_done = notify_trans_done,
      .user_ctx = NULL,
      .lcd_cmd_bits = 10,
      .lcd_param_bits = 0,
      // .flags.reverse_color_bits = 1
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
}
#endif

void i2s_deinit() {
  esp_intr_free(gI2S_intr_handle);

  free(i2s_state.buf_a);
  free(i2s_state.buf_b);
  free((void *) i2s_state.dma_desc_a);
  free((void *) i2s_state.dma_desc_b);

  periph_module_disable(PERIPH_I2S1_MODULE);
}

/******************************************************************************/
/***        local functions                                                 ***/
/******************************************************************************/
#if USER_I2S_REG
/// Initializes a DMA descriptor.
static void fill_dma_desc(volatile lldesc_t *dmadesc, uint8_t *buf, i2s_bus_config *cfg) {
  dmadesc->size = cfg->epd_row_width / 4;
  dmadesc->length = cfg->epd_row_width / 4;
  dmadesc->buf = buf;
  dmadesc->eof = 1;
  dmadesc->sosf = 1;
  dmadesc->owner = 1;
  dmadesc->qe.stqe_next = 0;
  dmadesc->offset = 0;
}

/// Address of the currently front DMA descriptor,
/// which uses only the lower 20bits (according to TRM)
static uint32_t dma_desc_addr() {
  return (uint32_t) (current_buffer ? i2s_state.dma_desc_a : i2s_state.dma_desc_b) & 0x000FFFFF;
}

/// Set up a GPIO as output and route it to a signal.
static void gpio_setup_out(int32_t gpio, int32_t sig, bool invert) {
  if (gpio == -1)
    return;

  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
  gpio_set_direction(gpio, GPIO_MODE_DEF_OUTPUT);
  gpio_matrix_out(gpio, sig, invert, false);
}

/// Resets "Start Pulse" signal when the current row output is done.
static void IRAM_ATTR i2s_int_hdl(void *arg) {
#ifdef CONFIG_IDF_TARGET_ESP32
  i2s_dev_t *dev = &I2S1;
#elif CONFIG_IDF_TARGET_ESP32S3
  i2s_dev_t *dev = &I2S0;
#else
#error "Unknown SOC"
#endif
  if (dev->int_st.out_done) {
    gpio_set_level(start_pulse_pin, 1);
    output_done = true;
  }
  // Clear the interrupt. Otherwise, the whole device would hang.
  dev->int_clr.val = dev->int_raw.val;
}
#endif

/******************************************************************************/
/***        END OF FILE                                                     ***/
/******************************************************************************/
