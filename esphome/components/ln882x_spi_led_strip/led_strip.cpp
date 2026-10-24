#include "led_strip.h"

#ifdef USE_LN882X

#include <cinttypes>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

extern "C" {
// Skip the SDK register headers: their unused *_unpack inline functions copy volatile unions, which C++ rejects.
// Same as LibreTiny's sdk_private.h; the HAL headers below use nothing from them.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#define __REG_DMA_H__
#define __REG_GPIO_H__
#define __REG_SPI_H__
#define __REG_SYSC_AWO_H__
#define __REG_SYSC_CMP_H__
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#include "hal/hal_clock.h"
#include "hal/hal_dma.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
}

namespace esphome::ln882x_spi_led_strip {

ESPHOME_LOG_TAG(TAG, "ln882x_spi_led_strip");

// light.py derives the SPI encoding from this APB0 clock, which the SDK's SystemInit() sets.
static constexpr uint32_t APB0_HZ = 40000000;
// Each LED data bit is sent as one SPI byte, MSB first: the leading 1-bits are the high time.
static constexpr uint8_t SPI_BITS_PER_LED_BIT = 8;
// SPI0 TX is hard-wired to DMA channel 4 (see hal_dma.h).
static constexpr uint32_t LED_STRIP_DMA_CH = DMA_CH_4;

static uint8_t spi_pattern(uint8_t high_bits) { return uint8_t(0xFF00 >> high_bits); }

static void spi_dma_init(uint8_t pin, uint8_t prescaler) {
  // LibreTiny numbers pins as (port << 4) | index. Every pin the LN882x boards expose
  // (PA0-PA12, PB3-PB9) can be muxed to SPI0 MOSI.
  const uint32_t gpio_base = (pin >> 4) ? GPIOB_BASE : GPIOA_BASE;
  const auto gpio_pin = static_cast<gpio_pin_t>(1U << (pin & 0xF));
  hal_gpio_pin_afio_select(gpio_base, gpio_pin, SPI0_MOSI);
  hal_gpio_pin_afio_en(gpio_base, gpio_pin, HAL_ENABLE);

  // Only MOSI is routed to a pin; NSS is left unrouted with its output disabled.
  spi_init_type_def spi_init{};
  spi_init.spi_direction = SPI_DIRECTION_2LINES_FULLDUPLEX;
  spi_init.spi_mode = SPI_MODE_MASTER;
  spi_init.spi_data_size = SPI_DATASIZE_8B;
  spi_init.spi_cpol = SPI_CPOL_LOW;
  spi_init.spi_cpha = SPI_CPHA_1EDGE;
  spi_init.spi_nss_mode = SPI_NSS_HARD;
  spi_init.spi_baud_rate_prescaler = static_cast<spi_baud_rate_precaler_t>(prescaler);
  spi_init.spi_first_bit = SPI_FIRST_BIT_MSB;
  hal_spi_init(SPI0_BASE, &spi_init);
  hal_spi_en(SPI0_BASE, HAL_ENABLE);
  hal_spi_ssoe_en(SPI0_BASE, HAL_DISABLE);

  dma_init_t_def dma_init{};
  dma_init.dma_mem_to_mem_en = DMA_MEM_TO_MEM_DIS;
  dma_init.dma_pri_lev = DMA_PRI_LEV_HIGH;
  dma_init.dma_mem_size = DMA_MEM_SIZE_8_BIT;
  dma_init.dma_p_size = DMA_P_SIZE_8_BIT;
  dma_init.dma_mem_inc_en = DMA_MEM_INC_EN;
  dma_init.dma_p_inc_en = DMA_P_INC_DIS;
  dma_init.dma_circ_mode_en = DMA_CIRC_MODE_DIS;
  dma_init.dma_dir = DMA_READ_FORM_MEM;
  dma_init.dma_p_addr = SPI0_DATA_REG;
  hal_dma_init(LED_STRIP_DMA_CH, &dma_init);
  hal_dma_en(LED_STRIP_DMA_CH, HAL_DISABLE);
}

static void spi_dma_transmit(const uint8_t *data, uint16_t len) {
  hal_dma_en(LED_STRIP_DMA_CH, HAL_DISABLE);
  hal_spi_dma_en(SPI0_BASE, SPI_DMA_TX_EN, HAL_DISABLE);
  hal_dma_set_mem_addr(LED_STRIP_DMA_CH, reinterpret_cast<uint32_t>(data));
  hal_dma_set_data_num(LED_STRIP_DMA_CH, len);
  hal_spi_dma_en(SPI0_BASE, SPI_DMA_TX_EN, HAL_ENABLE);
  hal_dma_en(LED_STRIP_DMA_CH, HAL_ENABLE);
}

// The count reaches zero once the last byte is in the SPI data register. The tail of every
// transfer is the reset gap, so starting the next one while that byte shifts out is harmless.
static bool spi_dma_busy() { return hal_dma_get_data_num(LED_STRIP_DMA_CH) != 0; }

LN882xSPILEDStripLightOutput::LN882xSPILEDStripLightOutput(uint8_t pin, uint16_t num_leds,
                                                           light::ChannelColors channel_colors, uint8_t prescaler,
                                                           uint8_t bit0_high_bits, uint8_t bit1_high_bits,
                                                           uint16_t reset_bytes)
    : color_data_bytes_(num_leds * channel_colors.bytes_per_led()),
      // The tail of the DMA buffer stays zero: it is the reset gap.
      dma_data_bytes_(color_data_bytes_ * SPI_BITS_PER_LED_BIT + reset_bytes),
      channel_colors_(channel_colors),
      num_leds_(num_leds),
      pin_(pin),
      prescaler_(prescaler),
      bit0_high_bits_(bit0_high_bits),
      bit1_high_bits_(bit1_high_bits) {}

void LN882xSPILEDStripLightOutput::setup() {
  const uint32_t apb0_hz = hal_clock_get_apb0_clk();
  if (apb0_hz != APB0_HZ) {
    ESP_LOGE(TAG, "APB0 clock is %" PRIu32 " Hz", apb0_hz);
    this->mark_failed(LOG_STR("APB0 clock is not 40 MHz"));
    return;
  }

  // One block for the color, effect and DMA data.
  RAMAllocator<uint8_t> allocator;
  uint8_t *buffer = allocator.allocate(this->color_data_bytes_ + this->num_leds_ + this->dma_data_bytes_);
  if (buffer == nullptr) {
    this->mark_failed(LOG_STR("Allocation error"));
    return;
  }
  memset(buffer, 0, this->color_data_bytes_ + this->num_leds_ + this->dma_data_bytes_);
  this->color_data_ = buffer;
  this->effect_data_ = buffer + this->color_data_bytes_;
  this->dma_data_ = this->effect_data_ + this->num_leds_;

  spi_dma_init(this->pin_, this->prescaler_);

  this->accessor_ = new light::CorrectedBufferColorAccessor(this->color_data_, this->effect_data_,
                                                            this->channel_colors_, &this->correction_);
}

void LN882xSPILEDStripLightOutput::dump_config() {
  const uint32_t spi_bit_ns = (1000000000 / APB0_HZ) << this->prescaler_;
  char channel_colors[5];
  ESP_LOGCONFIG(TAG,
                "LN882x SPI LED Strip:\n"
                "  Pin: %u\n"
                "  Channel colors: %s\n"
                "  Number of LEDs: %" PRIu16 "\n"
                "  Max refresh rate: %" PRIu32 " us\n"
                "  SPI frequency: %" PRIu32 " Hz\n"
                "  Bit 0 / bit 1 high: %" PRIu32 " / %" PRIu32 " ns of %" PRIu32 " ns",
                this->pin_, this->channel_colors_.to_string(channel_colors), this->num_leds_, this->max_refresh_rate_,
                APB0_HZ >> this->prescaler_, this->bit0_high_bits_ * spi_bit_ns, this->bit1_high_bits_ * spi_bit_ns,
                SPI_BITS_PER_LED_BIT * spi_bit_ns);
}

float LN882xSPILEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

void LN882xSPILEDStripLightOutput::write_state(light::LightState *state) {
  if (this->is_failed())
    return;

  const uint32_t now = micros();
  if (this->throttle_show_(now, this->last_refresh_, this->max_refresh_rate_))
    return;

  if (spi_dma_busy()) {
    // Busy sending the last frame, try again later.
    this->schedule_show();
    return;
  }

  this->last_refresh_ = now;
  this->mark_shown_();

  const uint8_t bit0 = spi_pattern(this->bit0_high_bits_);
  const uint8_t bit1 = spi_pattern(this->bit1_high_bits_);
  const uint8_t *color_data = this->color_data_;
  uint8_t *dma_data = this->dma_data_;
  for (size_t i = 0; i < this->color_data_bytes_; i++) {
    const uint8_t color_byte = *(color_data++);
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
      *(dma_data++) = (color_byte & mask) ? bit1 : bit0;
    }
  }

  spi_dma_transmit(this->dma_data_, this->dma_data_bytes_);
}

}  // namespace esphome::ln882x_spi_led_strip

#endif  // USE_LN882X
