#include "brizlabs_led_string.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include <cstring>
#include <esp_err.h>

namespace esphome::brizlabs_led_string {

ESPHOME_LOG_TAG(TAG, "brizlabs_led_string");

void BrizLabsLEDString::setup() {
  // This takes direct and full control over the SPI bus and uses it as a clocked signal
  // generator on MOSI only. (So it's not actually using it for SPI -- cool hack.)
  // See encoder.h for more details
  spi_bus_config_t bus = {};
  bus.mosi_io_num = this->pin_;
  bus.miso_io_num = bus.sclk_io_num = bus.quadwp_io_num = bus.quadhd_io_num = -1;
  if (!this->setup_check(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED) == ESP_OK,
                         LOG_STR("SPI bus init failed")))
    return;
  spi_device_interface_config_t dev = {};
  dev.clock_speed_hz = 100000;  // 1 timeslot on the wire = 10 us, which is what their LED modules need
  dev.spics_io_num = -1;
  dev.queue_size = NUM_ZONES;  // a whole write is queued at once
  if (!this->setup_check(spi_bus_add_device(SPI2_HOST, &dev, &this->dev_) == ESP_OK, LOG_STR("SPI device init failed")))
    return;
  this->accessor_ = &this->internal_accessor_;
}

void BrizLabsLEDString::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BrizLabs two-wire LED string:\n"
                "  Data (SPI MOSI): GPIO%d\n"
                "  Zones: %d",
                this->pin_, NUM_ZONES);
}

// Queues the frame and returns; the SPI driver sends the queued frames in order from its interrupt.
bool BrizLabsLEDString::queue_frame_(uint8_t zone, uint8_t r, uint8_t g, uint8_t b) {
  // write_state() only queues once every earlier frame is collected, so in_flight_ is the next free slot
  uint8_t *frame = this->frames_[this->in_flight_];
  spi_transaction_t &t = this->trans_[this->in_flight_];
  t = {};
  // Must be whole bytes. The zero pad bits only extend the idle-low after the frame's trailer.
  t.length = (encode(frame, 0, zone, 0xFF, r, g, b) + 7) & ~7u;
  t.tx_buffer = frame;
  esp_err_t err = spi_device_queue_trans(this->dev_, &t, 0);
  if (err != ESP_OK) {
    // Logged once per run of failures. write_state() retries on every loop until one succeeds.
    if (!this->status_has_warning()) {
      ESP_LOGW(TAG, "queue frame: %s", esp_err_to_name(err));
    }
    return false;
  }
  this->in_flight_++;
  return true;
}

// One broadcast frame if all zones match, else one frame per zone.
bool BrizLabsLEDString::transmit_all_() {
  bool uniform = true;
  for (int z = 0; z < NUM_ZONES; z++) {
    const uint8_t *p = this->buf_ + z * 3;
    uniform &= memcmp(p, this->buf_, 3) == 0;
  }
  if (uniform)
    return this->queue_frame_(0, this->buf_[0], this->buf_[1], this->buf_[2]);
  for (int z = 0; z < NUM_ZONES; z++) {
    const uint8_t *p = this->buf_ + z * 3;
    // Stop at the first failure; write_state() queues every zone again once the queued frames have gone out
    if (!this->queue_frame_(z + 1, p[0], p[1], p[2]))
      return false;
  }
  return true;
}

void BrizLabsLEDString::write_state(light::LightState *state) {
  if (this->is_failed())
    return;

  // Collect the frames that have finished
  spi_transaction_t *done;
  while (this->in_flight_ > 0 && spi_device_get_trans_result(this->dev_, &done, 0) == ESP_OK)
    this->in_flight_--;
  // The previous write is still on the wire and owns trans_ and frames_: try again on the next loop
  if (this->in_flight_ > 0) {
    this->schedule_show();
    return;
  }

  this->mark_shown_();  // power supply on/off by pixel state

  // On failure, try again on a later loop so the frame is not lost
  if (!this->status_comm_result(this->transmit_all_()))
    this->schedule_show();
}

}  // namespace esphome::brizlabs_led_string

#endif  // USE_ESP32
