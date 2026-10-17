#include "one_wire_device.h"
#ifdef USE_ESP32
#include <cmath>
#include <driver/gpio.h>
#include <esp_ipc.h>
#include <esp_mac.h>
#include <esp_memory_utils.h>
#include <esp_rom_sys.h>
#include <hal/gpio_ll.h>
#include <soc/gpio_struct.h>
#include <soc/interrupts.h>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::one_wire_device {
ESPHOME_LOG_TAG(TAG, "one_wire_device");
static_assert(std::atomic<int32_t>::is_always_lock_free);
static_assert(std::atomic<uint32_t>::is_always_lock_free);

void OneWireDeviceComponent::raise_value_warning_(const LogString *message) {
  // Do not claim a warning flag which was already raised by another condition.
  if (!this->status_has_warning())
    this->value_warning_ = true;
  this->status_set_warning(message);
}
bool OneWireDeviceComponent::set_value(uint8_t channel, float value) {
  if (!this->value_channel_supported_(channel)) {
    this->configuration_warning_ = true;
    ESP_LOGW(TAG, "Value channel %u is not supported by family 0x%02X", channel, this->family_);
    this->status_set_warning(LOG_STR("Invalid value channel configuration"));
    return false;
  }
  if (this->family_ == FAMILY_DS2413) {
    if (!std::isfinite(value) || (value != 0 && value != 1)) {
      this->pio_.store(this->pio_.load() & ~(1U << (channel + 7)), std::memory_order_relaxed);
      this->raise_value_warning_(LOG_STR("Digital value must be 0 or 1"));
      return false;
    }
    const uint8_t bit = channel - 1;
    uint32_t state = this->pio_.load(std::memory_order_relaxed);
    state = (state & ~(1U << bit)) | (static_cast<uint32_t>(value == 1) << bit) | (1U << (bit + 8));
    this->pio_.store(state, std::memory_order_relaxed);
  } else {
    if (!std::isfinite(value) || value < -55 || value > 125) {
      this->temperature_.store(INT32_MIN, std::memory_order_relaxed);
      this->raise_value_warning_(LOG_STR("Value unavailable or outside -55..125 C"));
      return false;
    }
    this->temperature_.store(static_cast<int32_t>(std::lroundf(value * 16)), std::memory_order_relaxed);
  }
  if (this->value_warning_ && this->available_()) {
    this->value_warning_ = false;
    if (!this->configuration_warning_)
      this->status_clear_warning();
  }
  return true;
}
bool OneWireDeviceComponent::value_channel_supported_(uint8_t channel) const {
  if (this->family_ == FAMILY_DS2413)
    return channel == 1 || channel == 2;
  return channel == 0 &&
         (this->family_ == FAMILY_DS18B20 || this->family_ == FAMILY_DS1822 || this->family_ == FAMILY_DS18S20);
}
void OneWireDeviceComponent::set_value_function(uint8_t channel, std::function<float()> function) {
  if (!this->value_channel_supported_(channel)) {
    ESP_LOGW(TAG, "Unsupported value binding channel %u for family 0x%02X", channel, this->family_);
    this->configuration_warning_ = true;
    this->status_set_warning(LOG_STR("Invalid value binding channel"));
    return;
  }
  this->value_functions_[channel] = std::move(function);
  this->set_value(channel, NAN);
}
void OneWireDeviceComponent::update() {
  for (uint8_t channel = 0; channel < 3; channel++) {
    if (this->value_functions_[channel])
      this->set_value(channel, this->value_functions_[channel]());
  }
}
bool IRAM_ATTR OneWireDeviceComponent::available_() const {
  return SourceSnapshot::capture(this->temperature_, this->pio_).available(this->family_);
}
void OneWireDeviceComponent::setup() {
#if CONFIG_FREERTOS_UNICORE
  ESP_LOGE(TAG, "A dual-core ESP32 is required");
  this->mark_failed();
  return;
#endif
  if (this->pin_ == nullptr) {
    this->mark_failed(LOG_STR("Missing 1-Wire DQ pin"));
    return;
  }
  if (this->pin_->is_inverted()) {
    this->mark_failed(LOG_STR("1-Wire DQ must not be inverted"));
    return;
  }
  const bool output_pin = GPIO_IS_VALID_OUTPUT_GPIO(this->pin_->get_pin());
  if (!output_pin) {
    this->mark_failed(LOG_STR("Invalid 1-Wire output GPIO"));
    return;
  }
  if (!esp_ptr_internal(this)) {
    this->mark_failed(LOG_STR("1-Wire component must be in internal RAM"));
    return;
  }
  this->pin_number_ = this->pin_->get_pin();
  if (this->address_ == 0) {
    uint8_t rom[8]{this->family_};
    const esp_err_t mac_error = esp_read_mac(&rom[1], ESP_MAC_WIFI_STA);
    if (mac_error != ESP_OK) {
      ESP_LOGE(TAG, "Unable to derive ROM: %s; configure an explicit address", esp_err_to_name(mac_error));
      this->mark_failed(LOG_STR("Unable to derive ROM; set an explicit address"));
      return;
    }
    rom[7] = crc8(rom, 7);
    for (uint8_t i = 0; i < 8; i++)
      this->address_ |= static_cast<uint64_t>(rom[i]) << (8 * i);
  }
  this->model_.configure(this->family_);
  this->model_.set_rom(this->address_);
  gpio_set_level(static_cast<gpio_num_t>(this->pin_number_), 1);
  this->pin_->setup();
  this->update();  // Evaluate bindings in foreground, never from the wire interrupt.
  // Keep ESPHome's shared GPIO service untouched. The opposite core owns this interrupt.
  this->interrupt_core_ = 1 - xPortGetCoreID();
  esp_err_t error = esp_ipc_call_blocking(this->interrupt_core_, &OneWireDeviceComponent::install_interrupt, this);
  if (error != ESP_OK || this->interrupt_error_ != ESP_OK) {
    ESP_LOGE(TAG, "Unable to allocate dedicated GPIO ISR");
    this->mark_failed();
    return;
  }
}
void OneWireDeviceComponent::install_interrupt(void *arg) {
  auto *self = static_cast<OneWireDeviceComponent *>(arg);
  self->interrupt_error_ = esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
                                          &OneWireDeviceComponent::gpio_isr, self, &self->interrupt_handle_);
  if (self->interrupt_error_ == ESP_OK) {
    gpio_ll_set_intr_type(&GPIO, self->pin_number_, GPIO_INTR_NEGEDGE);
    self->clear_interrupt_();
    self->enable_interrupt_();
  }
}
void OneWireDeviceComponent::free_interrupt(void *arg) {
  auto *self = static_cast<OneWireDeviceComponent *>(arg);
  self->interrupt_error_ = esp_intr_free(self->interrupt_handle_);
  if (self->interrupt_error_ == ESP_OK)
    self->interrupt_handle_ = nullptr;
}
void OneWireDeviceComponent::on_shutdown() {
  this->stopping_.store(true, std::memory_order_release);
  if (this->interrupt_handle_ != nullptr) {
    gpio_ll_intr_disable(&GPIO, this->pin_number_);
    const esp_err_t disable_error = esp_intr_disable(this->interrupt_handle_);
    if (disable_error != ESP_OK) {
      ESP_LOGW(TAG, "Interrupt disable failed: %s", esp_err_to_name(disable_error));
    }
    const esp_err_t call_error =
        esp_ipc_call_blocking(this->interrupt_core_, &OneWireDeviceComponent::free_interrupt, this);
    if (call_error != ESP_OK) {
      ESP_LOGW(TAG, "Interrupt teardown IPC failed: %s", esp_err_to_name(call_error));
    } else if (this->interrupt_error_ != ESP_OK) {
      ESP_LOGW(TAG, "Interrupt free failed: %s", esp_err_to_name(this->interrupt_error_));
    }
    this->release_();
    gpio_set_direction(static_cast<gpio_num_t>(this->pin_number_), GPIO_MODE_INPUT);
  }
}
void OneWireDeviceComponent::dump_config() {
  LOG_PIN("1-Wire emulation pin: ", this->pin_);
  char rom[17];
  ESP_LOGCONFIG(TAG, "  ROM: 0x%s, family: 0x%02X", format_hex_to(rom, this->address_), this->family_);
  ESP_LOGCONFIG(TAG, "  One device, standard speed; Copy/Recall RAM-only; immediate conversion");
}
bool IRAM_ATTR OneWireDeviceComponent::read_() const { return gpio_ll_get_level(&GPIO, this->pin_number_) != 0; }
void IRAM_ATTR OneWireDeviceComponent::release_() { gpio_ll_set_level(&GPIO, this->pin_number_, 1); }
void IRAM_ATTR OneWireDeviceComponent::pull_low_() { gpio_ll_set_level(&GPIO, this->pin_number_, 0); }
void IRAM_ATTR OneWireDeviceComponent::clear_interrupt_() {
  if (this->pin_number_ < 32) {
    gpio_ll_clear_intr_status(&GPIO, 1UL << this->pin_number_);
  } else {
    gpio_ll_clear_intr_status_high(&GPIO, 1UL << (this->pin_number_ - 32));
  }
}
void IRAM_ATTR OneWireDeviceComponent::enable_interrupt_() {
  gpio_ll_intr_enable_on_core(&GPIO, this->interrupt_core_, this->pin_number_);
}
void IRAM_ATTR OneWireDeviceComponent::gpio_isr(void *arg) {
  auto *self = static_cast<OneWireDeviceComponent *>(arg);
  uint32_t pending;
  if (self->pin_number_ < 32) {
    gpio_ll_get_intr_status(&GPIO, self->interrupt_core_, &pending);
  } else {
    gpio_ll_get_intr_status_high(&GPIO, self->interrupt_core_, &pending);
  }
  if ((pending & (1UL << (self->pin_number_ % 32))) == 0)
    return;
  self->clear_interrupt_();
  if (!self->stopping_.load(std::memory_order_acquire))
    self->handle_edge_();
}
void IRAM_ATTR OneWireDeviceComponent::finish_reset_() {
  this->awaiting_release_ = false;
  this->active_session_ = false;
  gpio_ll_intr_disable(&GPIO, this->pin_number_);
  gpio_ll_set_intr_type(&GPIO, this->pin_number_, GPIO_INTR_NEGEDGE);
  this->model_.reset();
  if (micros() - this->falling_us_ >= 400 && this->available_()) {
    esp_rom_delay_us(30);
    this->pull_low_();
    esp_rom_delay_us(120);
    this->release_();
    this->active_session_ = true;
  }
  this->clear_interrupt_();
  this->enable_interrupt_();
}
void IRAM_ATTR OneWireDeviceComponent::handle_edge_() {
  if (this->awaiting_release_) {
    if (this->read_())
      this->finish_reset_();
    return;
  }
  this->falling_us_ = micros();
  bool transmitting = this->active_session_ && this->model_.is_transmitting();
  bool sampled = true;
  if (transmitting) {
    const SourceSnapshot source{INT32_MIN, this->pio_.load(std::memory_order_relaxed)};
    if (this->model_.needs_pio_sample() && !source.available(this->family_)) {
      this->active_session_ = false;
      transmitting = false;
    } else if (!this->model_.transmit_bit(source.pio)) {
      gpio_ll_intr_disable(&GPIO, this->pin_number_);
      this->pull_low_();
      while (micros() - this->falling_us_ < 40) {
      }
      this->release_();
      this->clear_interrupt_();
      this->enable_interrupt_();
    }
  }
  if (!transmitting) {
    while (micros() - this->falling_us_ < 23) {
    }
    sampled = this->read_();
  }
  while (!this->read_() && micros() - this->falling_us_ < 150) {
  }
  if (!this->read_()) {
    // Bound held-low handling; only the later rising edge emits presence.
    this->awaiting_release_ = true;
    gpio_ll_set_intr_type(&GPIO, this->pin_number_, GPIO_INTR_POSEDGE);
    if (this->read_())
      this->finish_reset_();
    return;
  }
  if (!transmitting && this->active_session_) {
    const auto source = SourceSnapshot::capture(this->temperature_, this->pio_);
    if (!source.available(this->family_)) {
      this->active_session_ = false;
      return;
    }
    this->model_.receive_bit(sampled, static_cast<int16_t>(source.temperature), source.pio);
  }
}
}  // namespace esphome::one_wire_device
#endif
