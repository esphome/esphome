#include "led_strip.h"

#ifdef USE_RP2

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/sem.h>
#include <pico/stdlib.h>

namespace esphome::rp2040_pio_led_strip {

ESPHOME_LOG_TAG(TAG, "rp2040_pio_led_strip");

// DMA interrupt service routine
void RP2040PIOLEDStripLightOutput::dma_write_complete_handler() {
  uint32_t channel = dma_hw->ints0;
  for (uint dma_chan = 0; dma_chan < NUM_DMA_CHANNELS; ++dma_chan) {
    if (RP2040PIOLEDStripLightOutput::dma_chan_active[dma_chan] && (channel & (1u << dma_chan))) {
      dma_hw->ints0 = (1u << dma_chan);                                              // Clear the interrupt
      sem_release(&RP2040PIOLEDStripLightOutput::dma_write_complete_sem[dma_chan]);  // Handle the interrupt
    }
  }
}

void RP2040PIOLEDStripLightOutput::setup() {
  RAMAllocator<uint8_t> allocator;
  if (!this->setup_check(this->internal_accessor_.allocate(allocator, this->num_leds_),
                         LOG_STR(ESP_LOG_MSG_ALLOC_FAIL)))
    return;

  // Initialize the PIO program

  // Select PIO instance to use (0 or 1)
  if (!this->setup_check(this->pio_ != nullptr, LOG_STR("Failed to claim PIO instance")))
    return;

  // There are only 4 state machines on each PIO, so at most 4 strips per PIO
  auto &loaded = RP2040PIOLEDStripLightOutput::loaded_programs[pio_get_index(this->pio_)];
  if (!this->setup_check(loaded.strips < NUM_PIO_STATE_MACHINES, LOG_STR("Too many instances of PIO program")))
    return;

  // Strips with the same program on the same PIO share one copy of it in instruction memory
  uint offset = 0;
  for (uint8_t i = 0; i < NUM_PIO_STATE_MACHINES; i++) {
    if (loaded.program[i] == nullptr) {
      // The SDK panics when instruction memory is full
      if (!this->setup_check(pio_can_add_program(this->pio_, this->program_),
                             LOG_STR("No PIO instruction memory left")))
        return;
      offset = pio_add_program(this->pio_, this->program_);
      loaded.program[i] = this->program_;
      loaded.offset[i] = offset;
      break;
    }
    if (loaded.program[i] == this->program_) {
      offset = loaded.offset[i];
      break;
    }
  }
  loaded.strips++;

  // Configure the state machine's PIO, and start it
  int sm = pio_claim_unused_sm(this->pio_, false);
  if (sm < 0) {
    this->mark_failed(LOG_STR("Failed to claim PIO state machine"));
    return;
  }
  this->sm_ = sm;

  // Other code may already hold every DMA channel
  int dma_chan = dma_claim_unused_channel(false);
  if (dma_chan < 0) {
    pio_sm_unclaim(this->pio_, this->sm_);
    this->mark_failed(LOG_STR("Failed to claim DMA channel"));
    return;
  }
  this->dma_chan_ = dma_chan;

  // Mark the DMA channel as active
  RP2040PIOLEDStripLightOutput::dma_chan_active[this->dma_chan_] = true;

  this->dma_config_ = dma_channel_get_default_config(this->dma_chan_);
  channel_config_set_transfer_data_size(
      &this->dma_config_,
      DMA_SIZE_8);  // 8 bit transfers (could be 32 but the pio program would need to be changed to handle junk data)
  channel_config_set_read_increment(&this->dma_config_, true);    // increment the read address
  channel_config_set_write_increment(&this->dma_config_, false);  // don't increment the write address
  channel_config_set_dreq(&this->dma_config_,
                          pio_get_dreq(this->pio_, this->sm_, true));  // set the DREQ to the state machine's TX FIFO

  dma_channel_configure(this->dma_chan_, &this->dma_config_,
                        &this->pio_->txf[this->sm_],          // write to the state machine's TX FIFO
                        this->internal_accessor_.color_data,  // read from memory
                        this->get_buffer_size_(),             // number of bytes to transfer
                        false                                 // don't start yet
  );

  // Initialize the semaphore for this DMA channel
  sem_init(&RP2040PIOLEDStripLightOutput::dma_write_complete_sem[this->dma_chan_], 1, 1);

  irq_set_exclusive_handler(DMA_IRQ_0, dma_write_complete_handler);  // after DMA all data, raise an interrupt
  dma_channel_set_irq0_enabled(this->dma_chan_, true);               // map DMA channel to interrupt
  irq_set_enabled(DMA_IRQ_0, true);                                  // enable interrupt

  this->init_(this->pio_, this->sm_, offset, this->pin_, this->max_refresh_rate_);

  this->accessor_ = &this->internal_accessor_;
}

void RP2040PIOLEDStripLightOutput::write_state(light::LightState *state) {
  ESP_LOGVV(TAG, "Writing state");

  if (this->is_failed()) {
    ESP_LOGW(TAG, "Light is in failed state, not writing state.");
    return;
  }

  if (this->internal_accessor_.color_data == nullptr) {
    ESP_LOGW(TAG, "Buffer is null, not writing state.");
    return;
  }

  // the bits are already in the correct order for the pio program so we can just copy the buffer using DMA
  sem_acquire_blocking(&RP2040PIOLEDStripLightOutput::dma_write_complete_sem[this->dma_chan_]);
  dma_channel_transfer_from_buffer_now(this->dma_chan_, this->internal_accessor_.color_data, this->get_buffer_size_());
}

void RP2040PIOLEDStripLightOutput::dump_config() {
  char channel_colors[5];
  ESP_LOGCONFIG(TAG,
                "RP2040 PIO LED Strip Light Output:\n"
                "  Pin: GPIO%d\n"
                "  Number of LEDs: %d\n"
                "  Channel colors: %s\n"
                "  Max Refresh Rate: %f Hz",
                this->pin_, this->num_leds_, this->internal_accessor_.channel_colors.to_string(channel_colors),
                this->max_refresh_rate_);
}

float RP2040PIOLEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esphome::rp2040_pio_led_strip

#endif
