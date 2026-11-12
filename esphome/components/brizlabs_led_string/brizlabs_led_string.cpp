#include "brizlabs_led_string.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include <cstring>

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
  dev.queue_size = 1;
  if (!this->setup_check(spi_bus_add_device(SPI2_HOST, &dev, &this->dev_) == ESP_OK, LOG_STR("SPI device init failed")))
    return;
  this->accessor_ = &this->internal_accessor_;
}

void BrizLabsLEDString::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BrizLabs two-wire LED string:\n"
                "  Data (SPI MOSI): GPIO%d\n"
                "  Zones: %d",
                this->pin_, this->num_zones_);
}

bool BrizLabsLEDString::send_frame_(uint8_t zone, uint8_t r, uint8_t g, uint8_t b) {
  spi_transaction_t t = {};
  // Must be whole bytes. The zero pad bits only extend the idle-low after the frame's trailer.
  t.length = (encode(this->frame_, 0, zone, 0xFF, r, g, b) + 7) & ~7u;
  t.tx_buffer = this->frame_;
  esp_err_t err = spi_device_transmit(this->dev_, &t);
  // Logged once per run of failures. write_state() retries on every loop until one succeeds.
  if (err != ESP_OK && !this->status_has_warning()) {
    ESP_LOGW(TAG, "transmit: %s", esp_err_to_name(err));
  }
  return err == ESP_OK;
}

// One broadcast frame if all zones match, else one frame per zone.
bool BrizLabsLEDString::transmit_all_() {
  bool uniform = true;
  for (int z = 0; z < this->num_zones_; z++) {
    const uint8_t *p = this->buf_ + z * 3;
    uniform &= memcmp(p, this->buf_, 3) == 0;
  }
  if (uniform)
    return this->send_frame_(0, this->buf_[0], this->buf_[1], this->buf_[2]);
  for (int z = 0; z < this->num_zones_; z++) {
    const uint8_t *p = this->buf_ + z * 3;
    // Stop at the first failure; write_state() sends every zone again on the next loop
    if (!this->send_frame_(z + 1, p[0], p[1], p[2]))
      return false;
  }
  return true;
}

void BrizLabsLEDString::write_state(light::LightState *state) {
  if (this->is_failed())
    return;

  this->mark_shown_();  // power supply on/off by pixel state

  // On failure, send again on the next loop so the frame is not lost
  if (!this->status_comm_result(this->transmit_all_()))
    this->schedule_show();
}

}  // namespace esphome::brizlabs_led_string

#endif  // USE_ESP32
