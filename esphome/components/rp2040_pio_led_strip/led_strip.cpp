#include "led_strip.h"

#ifdef USE_RP2040

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/stdlib.h>
#include <pico/sem.h>

namespace esphome {
namespace rp2040_pio_led_strip {

static const char *TAG = "rp2040_pio_led_strip";

static uint8_t num_instance_[2] = {0, 0};
static std::map<Chipset, uint> chipset_offsets_ = {
    {CHIPSET_WS2812, 0}, {CHIPSET_WS2812B, 0}, {CHIPSET_SK6812, 0}, {CHIPSET_SM16703, 0}, {CHIPSET_CUSTOM, 0},
};
static std::map<Chipset, bool> conf_count_ = {
    {CHIPSET_WS2812, false},  {CHIPSET_WS2812B, false}, {CHIPSET_SK6812, false},
    {CHIPSET_SM16703, false}, {CHIPSET_CUSTOM, false},
};
static bool dma_chan_active_[12];
static struct semaphore dma_write_complete_sem_[12];

// DMA interrupt service routine
void RP2040PIOLEDStripLightOutput::dma_write_complete_handler_() {
  uint32_t channel = dma_hw->ints0;
  for (uint dma_chan = 0; dma_chan < 12; ++dma_chan) {
    if (RP2040PIOLEDStripLightOutput::dma_chan_active_[dma_chan] && (channel & (1u << dma_chan))) {
      dma_hw->ints0 = (1u << dma_chan);                                               // Clear the interrupt
      sem_release(&RP2040PIOLEDStripLightOutput::dma_write_complete_sem_[dma_chan]);  // Handle the interrupt
    }
  }
}

void RP2040PIOLEDStripLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RP2040 LED Strip...");

  size_t buffer_size = this->get_buffer_size_();

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buf_ = allocator.allocate(buffer_size);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate buffer of size %u", buffer_size);
    this->mark_failed();
    return;
  }

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate effect data of size %u", this->num_leds_);
    this->mark_failed();
    return;
  }

  // Initialize the PIO program

  // Select PIO instance to use (0 or 1)
  if (this->pio_ == nullptr) {
    ESP_LOGE(TAG, "Failed to claim PIO instance");
    this->mark_failed();
    return;
  }

  // if there are multiple strips, we can reuse the same PIO program and save space
  // but there are only 4 state machines on each PIO so we can only have 4 strips per PIO
  uint offset = 0;

  if (RP2040PIOLEDStripLightOutput::num_instance_[this->pio_ == pio0 ? 0 : 1] > 4) {
    ESP_LOGE(TAG, "Too many instances of PIO program");
    this->mark_failed();
    return;
  }
  // keep track of how many instances of the PIO program are running on each PIO
  RP2040PIOLEDStripLightOutput::num_instance_[this->pio_ == pio0 ? 0 : 1]++;

  // if there are multiple strips of the same chipset, we can reuse the same PIO program and save space
  if (this->conf_count_[this->chipset_]) {
    offset = RP2040PIOLEDStripLightOutput::chipset_offsets_[this->chipset_];
  } else {
    // Load the assembled program into the PIO and get its location in the PIO's instruction memory and save it
    offset = pio_add_program(this->pio_, this->program_);
    RP2040PIOLEDStripLightOutput::chipset_offsets_[this->chipset_] = offset;
    RP2040PIOLEDStripLightOutput::conf_count_[this->chipset_] = true;
  }

  // Configure the state machine's PIO, and start it
  this->sm_ = pio_claim_unused_sm(this->pio_, true);
  if (this->sm_ < 0) {
    // in theory this code should never be reached
    ESP_LOGE(TAG, "Failed to claim PIO state machine");
    this->mark_failed();
    return;
  }

  // Initalize the DMA channel (Note: There are 12 DMA channels and 8 state machines so we won't run out)

  this->dma_chan_ = dma_claim_unused_channel(true);
  if (this->dma_chan_ < 0) {
    ESP_LOGE(TAG, "Failed to claim DMA channel");
    this->mark_failed();
    return;
  }

  // Mark the DMA channel as active
  RP2040PIOLEDStripLightOutput::dma_chan_active_[this->dma_chan_] = true;

  this->dma_config_ = dma_channel_get_default_config(this->dma_chan_);
  channel_config_set_transfer_data_size(
      &this->dma_config_,
      DMA_SIZE_8);  // 8 bit transfers (could be 32 but the pio program would need to be changed to handle junk data)
  channel_config_set_read_increment(&this->dma_config_, true);    // increment the read address
  channel_config_set_write_increment(&this->dma_config_, false);  // don't increment the write address
  channel_config_set_dreq(&this->dma_config_,
                          pio_get_dreq(this->pio_, this->sm_, true));  // set the DREQ to the state machine's TX FIFO

  dma_channel_configure(this->dma_chan_, &this->dma_config_,
                        &this->pio_->txf[this->sm_],                     // write to the state machine's TX FIFO
                        this->buf_,                                      // read from memory
                        this->is_rgbw_ ? num_leds_ * 4 : num_leds_ * 3,  // number of bytes to transfer
                        false                                            // don't start yet
  );

  // Initialize the semaphore for this DMA channel
  sem_init(&RP2040PIOLEDStripLightOutput::dma_write_complete_sem_[this->dma_chan_], 1, 1);

  irq_set_exclusive_handler(DMA_IRQ_0, dma_write_complete_handler_);  // after DMA all data, raise an interrupt
  dma_channel_set_irq0_enabled(this->dma_chan_, true);                // map DMA channel to interrupt
  irq_set_enabled(DMA_IRQ_0, true);                                   // enable interrupt

  this->init_(this->pio_, this->sm_, offset, this->pin_, this->max_refresh_rate_);
}

void RP2040PIOLEDStripLightOutput::write_state(light::LightState *state) {
  ESP_LOGVV(TAG, "Writing state...");

  if (this->is_failed()) {
    ESP_LOGW(TAG, "Light is in failed state, not writing state.");
    return;
  }

  if (this->buf_ == nullptr) {
    ESP_LOGW(TAG, "Buffer is null, not writing state.");
    return;
  }

  // the bits are already in the correct order for the pio program so we can just copy the buffer using DMA
  sem_acquire_blocking(&RP2040PIOLEDStripLightOutput::dma_write_complete_sem_[this->dma_chan_]);
  dma_channel_transfer_from_buffer_now(this->dma_chan_, this->buf_, this->get_buffer_size_());
}

light::ESPColorView RP2040PIOLEDStripLightOutput::get_view_internal(int32_t index) const {
  int32_t r = 0, g = 0, b = 0, w = 0;
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
  uint8_t multiplier = this->is_rgbw_ ? 4 : 3;
  return {this->buf_ + (index * multiplier) + r,
          this->buf_ + (index * multiplier) + g,
          this->buf_ + (index * multiplier) + b,
          this->is_rgbw_ ? this->buf_ + (index * multiplier) + 3 : nullptr,
          &this->effect_data_[index],
          &this->correction_};
}

void RP2040PIOLEDStripLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "RP2040 PIO LED Strip Light Output:");
  ESP_LOGCONFIG(TAG, "  Pin: GPIO%d", this->pin_);
  ESP_LOGCONFIG(TAG, "  Number of LEDs: %d", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  RGBW: %s", YESNO(this->is_rgbw_));
  ESP_LOGCONFIG(TAG, "  RGB Order: %s", rgb_order_to_string(this->rgb_order_));
  ESP_LOGCONFIG(TAG, "  Max Refresh Rate: %f Hz", this->max_refresh_rate_);
}

float RP2040PIOLEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace rp2040_pio_led_strip
}  // namespace esphome

#endif
