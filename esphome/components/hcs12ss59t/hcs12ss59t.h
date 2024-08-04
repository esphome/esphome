#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

static const char HCS12SS59T_UNKNOWN_CHAR = '?';
static const size_t HCS12SS59T_NUMDIGITS = 12;
static const uint8_t HCS12SS59T_LIGHT_NORMAL = 0x00;
static const uint8_t HCS12SS59T_LIGHT_OFF = 0x01;
static const uint8_t HCS12SS59T_LIGHT_ON = 0x02;

namespace esphome {
namespace hcs12ss59t {

class HCS12SS59TComponent;

using hcs12ss59t_writer_t = std::function<void(HCS12SS59TComponent &)>;

class HCS12SS59TComponent : public display::DisplayBuffer,
                            public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                  spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_2MHZ> {
 public:
  void set_writer(hcs12ss59t_writer_t &&writer);

  void setup() override;

  void update() override{};

  void dump_config() override;

  float get_setup_priority() const override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

  void display();

  void set_intensity(uint8_t intensity, uint8_t light = HCS12SS59T_LIGHT_NORMAL);

  void printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
  void print(const char *str);
  void strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));
  char get_code(char c);

  void scroll_left() { scroll(1); };
  void scroll_right() { scroll(-1); };
  void scroll(uint16_t steps);

  void set_scroll(bool enabled);
  void set_scroll_speed(uint32_t ms);

  void set_enable_pin(GPIOPin *pin);
  void set_enabled(bool enabled);

 protected:
  int get_width_internal() override { return HCS12SS59T_NUMDIGITS; };
  int get_height_internal() override { return 1; };
  void draw_absolute_pixel_internal(int x, int y, Color color) override{};

  void send_command_(uint8_t a_register, uint8_t data);

  uint8_t intensity_{15};  // Intensity of the display from 0 to 15 (most)
  uint16_t scroll_{0};
  uint32_t scroll_speed_{300};
  std::string buffer_{""};
  optional<GPIOPin *> enable_pin_{};
  bool enabled_{true};
  bool initialised_{false};
  optional<hcs12ss59t_writer_t> writer_{};
};

}  // namespace hcs12ss59t
}  // namespace esphome
