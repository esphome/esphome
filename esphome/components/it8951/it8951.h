#pragma once

#if defined(USE_ESP32) && defined(USE_ARDUINO)

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "SPI.h"
#include "driver/gpio.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace it8951 {

enum IT8951Model {
  IT8951_MODEL_SEEED_RETERMINAL_E1003 = 0,
  IT8951_MODEL_SEEED_EE03 = 1,
};

struct IT8951Pins {
  gpio_num_t sck;
  gpio_num_t miso;
  gpio_num_t mosi;
  gpio_num_t cs;
  gpio_num_t busy;
  gpio_num_t enable;
  gpio_num_t reset;
  gpio_num_t ite_enable;
  bool has_ite_enable;
  const char *name;
};

#define IT8951_TCON_SYS_RUN 0x0001
#define IT8951_TCON_STANDBY 0x0002
#define IT8951_TCON_SLEEP 0x0003
#define IT8951_TCON_REG_RD 0x0010
#define IT8951_TCON_REG_WR 0x0011
#define IT8951_TCON_LD_IMG 0x0020
#define IT8951_TCON_LD_IMG_AREA 0x0021
#define IT8951_TCON_LD_IMG_END 0x0022

#define USDEF_I80_CMD_DPY_AREA 0x0034
#define USDEF_I80_CMD_GET_DEV_INFO 0x0302
#define USDEF_I80_CMD_VCOM 0x0039
#define USDEF_I80_CMD_TEMP 0x0040

#define IT8951_4BPP 2
#define IT8951_8BPP 3

#define IT8951_LDIMG_L_ENDIAN 0

#define I80CPCR 0x0004
#define LISAR 0x0208
#define LUTAFSR 0x1224
#define UP1SR 0x1138
#define BGVR 0x1250

struct IT8951DevInfo {
  uint16_t panel_width;
  uint16_t panel_height;
  uint16_t img_buf_addr_l;
  uint16_t img_buf_addr_h;
  uint16_t fw_version[8];
  uint16_t lut_version[8];
};

class IT8951Display : public display::DisplayBuffer {
 public:
  void set_model(IT8951Model model) { this->model_ = model; }
  void set_vcom(uint16_t vcom) { this->vcom_ = vcom; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_GRAYSCALE; }

 protected:
  int get_width_internal() override { return 1872; }
  int get_height_internal() override { return 1404; }
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  void configure_model_();
  void spi_send_word_(uint16_t word);
  uint16_t spi_recv_word_();
  void set_pin_mode_(gpio_num_t pin, gpio_mode_t mode);
  void write_pin_(gpio_num_t pin, bool value);
  bool read_pin_(gpio_num_t pin) const;
  void sleep_ms_(uint32_t ms);
  void lcd_write_cmd_code_(uint16_t cmd);
  void lcd_write_data_(uint16_t data);
  void lcd_write_framebuffer_4bpp_(const uint16_t *buf, uint16_t width_in_words, uint16_t height);
  void lcd_write_framebuffer_1bpp_(uint16_t width, uint16_t height);
  uint16_t lcd_read_data_();
  void lcd_read_n_data_(uint16_t *buf, uint32_t word_count);
  void lcd_wait_for_ready_();
  void lcd_sys_run_();
  void hardware_reset_();
  void power_cycle_();
  void wait_for_display_ready_();
  bool framebuffer_is_binary_();
  uint8_t get_pixel_nibble_(uint16_t x, uint16_t y);

  void lcd_send_cmd_arg_(uint16_t cmd, uint16_t *args, uint16_t num_args);
  uint16_t it8951_read_reg_(uint16_t addr);
  void it8951_write_reg_(uint16_t addr, uint16_t val);
  void get_it8951_system_info_();
  void set_img_buf_base_addr_(uint32_t addr);
  void it8951_load_img_area_start_(uint16_t endian, uint16_t pix_fmt, uint16_t rotate, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h);
  void it8951_display_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t mode);
  void it8951_display_area_1bpp_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t mode, uint8_t bg_gray,
                                 uint8_t fg_gray);
  void log_dev_info_words_(const char *label) const;
  bool has_valid_dev_info_() const;
  void write_vcom_(uint16_t selector, uint16_t value);
  bool probe_controller_(const char *label, bool send_sys_run, int vcom_selector);

  IT8951Model model_{IT8951_MODEL_SEEED_RETERMINAL_E1003};
  IT8951Pins pins_{};
  IT8951DevInfo dev_info_{};
  uint32_t img_buf_addr_{0};
  uint16_t vcom_{1400};
  uint32_t spi_frequency_{1000000};
  uint16_t vcom_write_selector_{0};
  const char *probe_path_{nullptr};
  uint8_t *framebuffer_{nullptr};
  const char *fail_reason_{nullptr};
  uint16_t probe_vcom_{0};
};

}  // namespace it8951
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_ARDUINO)
