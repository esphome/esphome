#include "hcs12ss59t.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace hcs12ss59t {

static const char *const TAG = "hcs12ss59t";

static const uint8_t HCS12SS59T_REGISTER_SEEK = 0x10;
static const uint8_t HCS12SS59T_REGISTER_INTENSITY = 0x50;
static const uint8_t HCS12SS59T_REGISTER_DIGIT_COUNT = 0x60;
static const uint8_t HCS12SS59T_REGISTER_LIGHTS = 0x70;

float HCS12SS59TComponent::get_setup_priority() const { return setup_priority::PROCESSOR; }
void HCS12SS59TComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HCS-12SS59T...");

  this->dump_config();

  this->reset_pin_->setup();
  this->enable_pin_->setup();

  this->reset_pin_->digital_write(true);
  this->enable_pin_->digital_write(true);

  this->spi_setup();
  // this->initialised_ = true;

  delayMicroseconds(1);
  this->reset_pin_->digital_write(false);
  delayMicroseconds(1);
  this->reset_pin_->digital_write(true);

  // this->set_intensity(this->intensity_);

  // this->display();

  ESP_LOGCONFIG(TAG, "Finished HCS-12SS59T setup");
}
void HCS12SS59TComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HCS12SS59T:");
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Intensity: %u", this->intensity_);
  ESP_LOGCONFIG(TAG, "  Scroll Position: %u", this->scroll_);
  // ESP_LOGCONFIG(TAG, "  Scroll Speed: %lu", this->scroll_speed_);
  ESP_LOGCONFIG(TAG, "  Buffer Size: %u", this->buffer_size_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

void HCS12SS59TComponent::display() {
  if (!this->initialised_) {
    return;
  }

  ESP_LOGCONFIG(TAG, "Display");

  const uint8_t size = buffer_size_;

  this->enable();

  for (uint8_t offset = 0; offset < HCS12SS59T_NUMDIGITS; offset++) {
    char c = offset < size ? buffer_[(scroll_) % size] : ' ';

    this->write_byte(this->get_code(c));
  }

  this->disable();
}
void HCS12SS59TComponent::send_command_(uint8_t a_register, uint8_t data) {
  this->transfer_byte(a_register | data);
  delayMicroseconds(8);
}
void HCS12SS59TComponent::print(const char *str) {
  int index = 0;

  while (str[index] && index < HCS12SS59T_BUFFER_SIZE) {
    this->buffer_[index] = str[index];
    index++;
  }

  this->buffer_size_ = index;

  this->display();
}
void HCS12SS59TComponent::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[HCS12SS59T_BUFFER_SIZE];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->print(buffer);
}
void HCS12SS59TComponent::set_writer(hcs12ss59t_writer_t &&writer) { this->writer_ = writer; }
void HCS12SS59TComponent::set_intensity(uint8_t intensity, uint8_t light) {
  if (!this->initialised_) {
    return;
  }

  this->enable();

  this->send_command_(HCS12SS59T_REGISTER_INTENSITY, clamp(intensity, (uint8_t) 0, (uint8_t) 15));
  this->send_command_(HCS12SS59T_REGISTER_LIGHTS, clamp(light, (uint8_t) 0, (uint8_t) 2));

  this->disable();

  this->intensity_ = intensity;
}
void HCS12SS59TComponent::strftime(const char *format, ESPTime time) {
  char buffer[HCS12SS59T_BUFFER_SIZE];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    this->print(buffer);
}

char HCS12SS59TComponent::get_code(char c) {
  if (c >= '@' && c <= '_') {
    c -= 48;
  } else if (c >= ' ' && c <= '?') {
    c += 16;
  } else if (c >= 'a' && c <= 'z') {
    c -= 80;
  } else {  // Invalid character
    c = this->get_code('?');
  }

  return c;
}

void HCS12SS59TComponent::scroll(uint16_t steps) {
  this->scroll_ = (this->buffer_size_ + steps + this->scroll_) % this->buffer_size_;
}

void HCS12SS59TComponent::set_scroll(bool enabled) {
  // this->cancel_interval("scroll");

  // if (enabled) {
  //   this->set_interval("scroll", this->scroll_speed_, [this]() {
  //     this->scroll(this->scroll_speed_ > 0 ? 1 : -1);
  //     this->display();
  //   });
  // }
}
void HCS12SS59TComponent::set_scroll_speed(uint32_t ms) {
  this->scroll_speed_ = ms;

  this->set_scroll(ms > 0);
}

void HCS12SS59TComponent::set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }

void HCS12SS59TComponent::set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

}  // namespace hcs12ss59t
}  // namespace esphome
