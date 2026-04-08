#include "it8951.h"

#ifdef USE_ESP32

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace it8951 {

static const char *const TAG = "it8951";
static const uint32_t IT8951_SPI_PROBE_FREQUENCY = 1000000;
static const uint32_t IT8951_SPI_RUN_FREQUENCY = 4000000;
static const gpio_num_t IT8951_UNUSED_PIN = static_cast<gpio_num_t>(-1);
static const gpio_num_t IT8951_GPIO_38 = static_cast<gpio_num_t>(38);
static const gpio_num_t IT8951_GPIO_43 = static_cast<gpio_num_t>(43);
static const gpio_num_t IT8951_GPIO_44 = static_cast<gpio_num_t>(44);

void IT8951Display::configure_model_() {
  switch (this->model_) {
    case IT8951_MODEL_SEEED_EE03:
      this->pins_ = {
          GPIO_NUM_4, IT8951_GPIO_43, IT8951_GPIO_38, IT8951_UNUSED_PIN, false, "Seeed XIAO ePaper Display Board EE03",
      };
      break;
    case IT8951_MODEL_SEEED_RETERMINAL_E1003:
    default:
      this->pins_ = {
          GPIO_NUM_13, GPIO_NUM_11, GPIO_NUM_12, GPIO_NUM_21, true, "Seeed reTerminal E1003",
      };
      break;
  }
}

void IT8951Display::spi_send_word_(uint16_t word) { this->write_byte16(word); }

uint16_t IT8951Display::spi_recv_word_() {
  uint8_t bytes[2];
  this->read_array(bytes, sizeof(bytes));
  return (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
}

void IT8951Display::set_pin_mode_(gpio_num_t pin, gpio_mode_t mode) { gpio_set_direction(pin, mode); }

void IT8951Display::write_pin_(gpio_num_t pin, bool value) { gpio_set_level(pin, value ? 1 : 0); }

bool IT8951Display::read_pin_(gpio_num_t pin) const { return gpio_get_level(pin) != 0; }

void IT8951Display::sleep_ms_(uint32_t ms) {
  while (ms != 0) {
    const uint32_t chunk_ms = std::min<uint32_t>(ms, 50);
    vTaskDelay(pdMS_TO_TICKS(chunk_ms));
    App.feed_wdt();
    ms -= chunk_ms;
  }
}

void IT8951Display::lcd_wait_for_ready_() {
  const uint32_t start = millis();
  while (!this->read_pin_(this->pins_.busy)) {
    if (millis() - start > 3000) {
      ESP_LOGE(TAG, "HRDY timeout");
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void IT8951Display::hardware_reset_() {
  if (this->cs_ != nullptr) {
    this->cs_->digital_write(true);
  }
  this->write_pin_(this->pins_.reset, true);
  this->write_pin_(this->pins_.enable, true);
  if (this->pins_.has_ite_enable) {
    this->write_pin_(this->pins_.ite_enable, true);
  }
  this->sleep_ms_(50);
  this->write_pin_(this->pins_.reset, false);
  this->sleep_ms_(10);
  this->write_pin_(this->pins_.reset, true);
  this->sleep_ms_(10);
}

void IT8951Display::power_cycle_() {
  if (this->cs_ != nullptr) {
    this->cs_->digital_write(true);
  }
  this->write_pin_(this->pins_.reset, true);
  this->write_pin_(this->pins_.enable, false);
  if (this->pins_.has_ite_enable) {
    this->write_pin_(this->pins_.ite_enable, false);
  }
  this->sleep_ms_(100);
  this->write_pin_(this->pins_.enable, true);
  if (this->pins_.has_ite_enable) {
    this->write_pin_(this->pins_.ite_enable, true);
  }
  this->sleep_ms_(500);
  this->hardware_reset_();
  this->sleep_ms_(1500);
}

void IT8951Display::reconfigure_spi_(uint32_t data_rate) {
  this->spi_frequency_ = data_rate;
  this->spi_teardown();
  this->set_data_rate(data_rate);
  this->spi_setup();
}

void IT8951Display::lcd_write_cmd_code_(uint16_t cmd) {
  this->enable();
  this->lcd_wait_for_ready_();
  this->spi_send_word_(0x6000);
  this->lcd_wait_for_ready_();
  this->spi_send_word_(cmd);
  this->disable();
}

void IT8951Display::lcd_write_data_(uint16_t data) {
  this->enable();
  this->lcd_wait_for_ready_();
  this->spi_send_word_(0x0000);
  this->lcd_wait_for_ready_();
  this->spi_send_word_(data);
  this->disable();
}

void IT8951Display::lcd_write_framebuffer_4bpp_(const uint16_t *buf, uint16_t width_in_words, uint16_t height) {
  uint8_t row_buffer[936];
  const uint32_t row_size_bytes = uint32_t(width_in_words) * 2;
  if (row_size_bytes > sizeof(row_buffer)) {
    ESP_LOGE(TAG, "Row buffer too small for %u-byte transfer", static_cast<unsigned>(row_size_bytes));
    return;
  }

  this->enable();
  this->lcd_wait_for_ready_();
  this->spi_send_word_(0x0000);
  this->lcd_wait_for_ready_();

  for (uint16_t y = 0; y < height; y++) {
    const uint32_t row_start = uint32_t(y) * width_in_words;
    for (uint16_t x = 0; x < width_in_words; x++) {
      const uint16_t word = buf[row_start + (width_in_words - 1 - x)];
      const uint32_t byte_index = uint32_t(x) * 2;
      row_buffer[byte_index] = static_cast<uint8_t>(word >> 8);
      row_buffer[byte_index + 1] = static_cast<uint8_t>(word & 0xFF);
    }
    this->write_array(row_buffer, row_size_bytes);
    if ((y & 0x07) == 0) {
      App.feed_wdt();
    }
  }

  this->disable();
}

void IT8951Display::lcd_write_framebuffer_1bpp_(uint16_t width, uint16_t height) {
  uint16_t row_words[117];
  uint8_t row_buffer[234];
  const uint16_t width_in_words = width / 16;
  const uint32_t row_size_bytes = uint32_t(width_in_words) * 2;

  if (width_in_words > (sizeof(row_words) / sizeof(row_words[0])) || row_size_bytes > sizeof(row_buffer)) {
    ESP_LOGE(TAG, "1bpp row buffer too small for %u-byte transfer", static_cast<unsigned>(row_size_bytes));
    return;
  }

  this->enable();
  this->lcd_wait_for_ready_();
  this->spi_send_word_(0x0000);
  this->lcd_wait_for_ready_();

  for (uint16_t y = 0; y < height; y++) {
    memset(row_words, 0x00, row_size_bytes);

    for (uint16_t x = 0; x < width; x++) {
      const uint8_t nibble = this->get_pixel_nibble_(x, y);
      if (nibble <= 0x07) {
        row_words[x / 16] |= uint16_t(0x8000 >> (x & 0x0F));
      }
    }

    for (uint16_t i = 0; i < width_in_words; i++) {
      const uint16_t word = row_words[width_in_words - 1 - i];
      const uint32_t byte_index = uint32_t(i) * 2;
      row_buffer[byte_index] = static_cast<uint8_t>(word >> 8);
      row_buffer[byte_index + 1] = static_cast<uint8_t>(word & 0xFF);
    }

    this->write_array(row_buffer, row_size_bytes);
    if ((y & 0x07) == 0) {
      App.feed_wdt();
    }
  }

  this->disable();
}

uint16_t IT8951Display::lcd_read_data_() {
  this->enable();
  this->lcd_wait_for_ready_();
  this->spi_send_word_(0x1000);
  this->spi_recv_word_();
  this->lcd_wait_for_ready_();
  const uint16_t data = this->spi_recv_word_();
  this->disable();
  return data;
}

void IT8951Display::lcd_read_n_data_(uint16_t *buf, uint32_t word_count) {
  this->enable();
  this->lcd_wait_for_ready_();
  this->spi_send_word_(0x1000);
  this->lcd_wait_for_ready_();
  this->spi_recv_word_();
  this->lcd_wait_for_ready_();
  for (uint32_t i = 0; i < word_count; i++) {
    buf[i] = this->spi_recv_word_();
  }
  this->disable();
}

void IT8951Display::lcd_sys_run_() { this->lcd_write_cmd_code_(IT8951_TCON_SYS_RUN); }

void IT8951Display::wait_for_display_ready_() {
  const uint32_t start = millis();
  while (this->it8951_read_reg_(LUTAFSR) != 0) {
    if (millis() - start > 30000) {
      ESP_LOGW(TAG, "Display-ready timeout while waiting for LUTAFSR to clear");
      break;
    }
    App.feed_wdt();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

bool IT8951Display::framebuffer_is_binary_() {
  if (this->framebuffer_ == nullptr) {
    return false;
  }

  const uint32_t buffer_size = (uint32_t(this->get_width_internal()) * this->get_height_internal()) / 2;
  for (uint32_t i = 0; i < buffer_size; i++) {
    const uint8_t hi = this->framebuffer_[i] >> 4;
    const uint8_t lo = this->framebuffer_[i] & 0x0F;
    if ((hi != 0x00 && hi != 0x0F) || (lo != 0x00 && lo != 0x0F)) {
      return false;
    }
  }
  return true;
}

uint8_t IT8951Display::get_pixel_nibble_(uint16_t x, uint16_t y) {
  const uint32_t pos = (x + y * this->get_width_internal()) / 2;
  if ((x & 1U) == 0) {
    return this->framebuffer_[pos] >> 4;
  }
  return this->framebuffer_[pos] & 0x0F;
}

void IT8951Display::write_vcom_(uint16_t selector, uint16_t value) {
  this->lcd_write_cmd_code_(USDEF_I80_CMD_VCOM);
  this->lcd_write_data_(selector);
  this->lcd_write_data_(value);
}

bool IT8951Display::has_valid_dev_info_() const {
  return this->dev_info_.panel_width > 0 && this->dev_info_.panel_width < 10000 && this->dev_info_.panel_height > 0 &&
         this->dev_info_.panel_height < 10000;
}

void IT8951Display::log_dev_info_words_(const char *label) const {
  const uint16_t *raw = reinterpret_cast<const uint16_t *>(&this->dev_info_);
  ESP_LOGD(TAG, "[%s] DevInfo raw [W=%u H=%u BufL=0x%04X BufH=0x%04X]", label, raw[0], raw[1], raw[2], raw[3]);
  ESP_LOGD(TAG, "[%s] DevInfo FW: %02X %02X %02X %02X %02X %02X %02X %02X", label, raw[4], raw[5], raw[6], raw[7],
           raw[8], raw[9], raw[10], raw[11]);
  ESP_LOGD(TAG, "[%s] DevInfo LUT: %02X %02X %02X %02X %02X %02X %02X %02X", label, raw[12], raw[13], raw[14], raw[15],
           raw[16], raw[17], raw[18], raw[19]);
}

bool IT8951Display::probe_controller_(const char *label, bool send_sys_run, int vcom_selector) {
  this->probe_path_ = label;
  this->probe_vcom_ = 0;
  memset(&this->dev_info_, 0, sizeof(this->dev_info_));

  if (send_sys_run) {
    ESP_LOGD(TAG, "[%s] Sending SYS_RUN wake command", label);
    this->lcd_sys_run_();
    this->sleep_ms_(10);
  }

  if (vcom_selector > 0) {
    ESP_LOGD(TAG, "[%s] Writing VCOM=%u with selector 0x%04X", label, this->vcom_, vcom_selector);
    this->write_vcom_(vcom_selector, this->vcom_);
    this->lcd_write_cmd_code_(USDEF_I80_CMD_VCOM);
    this->lcd_write_data_(0x0000);
    this->probe_vcom_ = this->lcd_read_data_();
    if (this->probe_vcom_ == this->vcom_) {
      this->vcom_write_selector_ = vcom_selector;
    }
  }

  this->get_it8951_system_info_();
  this->log_dev_info_words_(label);
  return this->has_valid_dev_info_();
}

void IT8951Display::setup() {
  this->configure_model_();
  ESP_LOGCONFIG(TAG, "Setting up IT8951 model %s...", this->pins_.name);

  this->set_pin_mode_(this->pins_.enable, GPIO_MODE_OUTPUT);
  this->set_pin_mode_(this->pins_.reset, GPIO_MODE_OUTPUT);
  this->set_pin_mode_(this->pins_.busy, GPIO_MODE_INPUT);
  if (this->pins_.has_ite_enable) {
    this->set_pin_mode_(this->pins_.ite_enable, GPIO_MODE_OUTPUT);
  }

  if (this->cs_ != nullptr) {
    this->cs_->setup();
    this->cs_->digital_write(true);
  }
  this->write_pin_(this->pins_.enable, true);
  this->write_pin_(this->pins_.reset, true);
  if (this->pins_.has_ite_enable) {
    this->write_pin_(this->pins_.ite_enable, true);
  }

  this->set_data_rate(IT8951_SPI_PROBE_FREQUENCY);
  this->spi_setup();

  struct ProbeAttempt {
    const char *label;
    bool send_sys_run;
    int vcom_selector;
  };
  static const ProbeAttempt PROBE_ATTEMPTS[] = {
      {"cold read", false, 0},
      {"wake then read", true, 0},
      {"wake + VCOM 0x0001", true, 0x0001},
      {"wake + VCOM 0x0002", true, 0x0002},
  };

  bool found_device = false;
  for (size_t i = 0; i < sizeof(PROBE_ATTEMPTS) / sizeof(PROBE_ATTEMPTS[0]); i++) {
    ESP_LOGD(TAG, "Probe attempt %u: %s", static_cast<unsigned>(i + 1), PROBE_ATTEMPTS[i].label);
    this->power_cycle_();
    this->lcd_wait_for_ready_();
    if (this->probe_controller_(PROBE_ATTEMPTS[i].label, PROBE_ATTEMPTS[i].send_sys_run,
                                PROBE_ATTEMPTS[i].vcom_selector)) {
      found_device = true;
      break;
    }
  }

  if (!found_device) {
    this->fail_reason_ = "IT8951 never returned valid device info";
    this->mark_failed();
    return;
  }

  if (this->vcom_write_selector_ == 0) {
    this->write_vcom_(0x0002, this->vcom_);
    this->lcd_write_cmd_code_(USDEF_I80_CMD_VCOM);
    this->lcd_write_data_(0x0000);
    this->probe_vcom_ = this->lcd_read_data_();
    if (this->probe_vcom_ == this->vcom_) {
      this->vcom_write_selector_ = 0x0002;
    } else {
      this->write_vcom_(0x0001, this->vcom_);
      this->lcd_write_cmd_code_(USDEF_I80_CMD_VCOM);
      this->lcd_write_data_(0x0000);
      this->probe_vcom_ = this->lcd_read_data_();
      if (this->probe_vcom_ == this->vcom_) {
        this->vcom_write_selector_ = 0x0001;
      }
    }
  }

  this->img_buf_addr_ = (uint32_t(this->dev_info_.img_buf_addr_h) << 16) | this->dev_info_.img_buf_addr_l;

  this->it8951_write_reg_(I80CPCR, 0x0001);
  this->lcd_write_cmd_code_(USDEF_I80_CMD_TEMP);
  this->lcd_write_data_(0x0001);
  this->lcd_write_data_(14);

  this->reconfigure_spi_(IT8951_SPI_RUN_FREQUENCY);

  const uint32_t buffer_size = (uint32_t(this->get_width_internal()) * this->get_height_internal()) / 2;
  this->framebuffer_ = static_cast<uint8_t *>(heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM));
  if (this->framebuffer_ == nullptr) {
    this->fail_reason_ = "PSRAM allocation failed";
    this->mark_failed();
    return;
  }
  memset(this->framebuffer_, 0xFF, buffer_size);
}

void IT8951Display::update() {
  if (this->framebuffer_ == nullptr) {
    ESP_LOGW(TAG, "Skipping update because the framebuffer is not available");
    return;
  }

  this->do_update_();

  const uint16_t w = this->get_width_internal();
  const uint16_t h = this->get_height_internal();
  const uint16_t width_in_words = (w + 3) / 4;
  const bool use_1bpp = this->framebuffer_is_binary_();

  this->wait_for_display_ready_();
  this->set_img_buf_base_addr_(this->img_buf_addr_);

  if (use_1bpp) {
    const uint16_t one_bpp_width_bytes = w / 8;
    ESP_LOGD(TAG, "Using sharp 1bpp upload path");
    this->it8951_load_img_area_start_(IT8951_LDIMG_L_ENDIAN, IT8951_8BPP, 0, 0, 0, one_bpp_width_bytes, h);
    this->lcd_write_framebuffer_1bpp_(w, h);
  } else {
    ESP_LOGD(TAG, "Using 4bpp grayscale upload path");
    this->it8951_write_reg_(UP1SR + 2, this->it8951_read_reg_(UP1SR + 2) & ~(1 << 2));
    this->it8951_load_img_area_start_(IT8951_LDIMG_L_ENDIAN, IT8951_4BPP, 0, 0, 0, w, h);
    this->lcd_write_framebuffer_4bpp_(reinterpret_cast<const uint16_t *>(this->framebuffer_), width_in_words, h);
  }

  this->lcd_write_cmd_code_(IT8951_TCON_LD_IMG_END);
  const uint16_t refresh_mode = 2;
  if (use_1bpp) {
    this->it8951_display_area_1bpp_(0, 0, w, h, refresh_mode, 0xFF, 0x00);
  } else {
    this->it8951_display_area_(0, 0, w, h, refresh_mode);
  }
}

void IT8951Display::fill(Color color) {
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }
  if (this->framebuffer_ == nullptr) {
    return;
  }

  const uint8_t packed = color.is_on() ? 0x00 : 0xFF;
  const uint32_t buffer_size = (uint32_t(this->get_width_internal()) * this->get_height_internal()) / 2;
  memset(this->framebuffer_, packed, buffer_size);
}

void IT8951Display::clear() { this->fill(display::COLOR_OFF); }

void IT8951Display::dump_config() {
  LOG_DISPLAY("", "IT8951", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->pins_.name != nullptr ? this->pins_.name : "unknown");
  ESP_LOGCONFIG(TAG, "  SPI Frequency: %u Hz", this->spi_frequency_);
  ESP_LOGCONFIG(TAG, "  VCOM: %u", this->vcom_);
  ESP_LOGCONFIG(TAG, "  VCOM selector: 0x%04X", this->vcom_write_selector_);
  ESP_LOGCONFIG(TAG, "  Probe path: %s", this->probe_path_ != nullptr ? this->probe_path_ : "none");
  ESP_LOGCONFIG(TAG, "  DevInfo Panel: %ux%u", this->dev_info_.panel_width, this->dev_info_.panel_height);
  ESP_LOGCONFIG(TAG, "  DevInfo ImgBuf: 0x%04X%04X", this->dev_info_.img_buf_addr_h, this->dev_info_.img_buf_addr_l);
  ESP_LOGCONFIG(TAG, "  Buffer allocated: %s", this->framebuffer_ != nullptr ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Busy pin: %s", this->read_pin_(this->pins_.busy) ? "HIGH (ready)" : "LOW (busy)");
  ESP_LOGCONFIG(TAG, "  VCOM read-back: %u (0x%04X)", this->probe_vcom_, this->probe_vcom_);
  if (this->fail_reason_ != nullptr) {
    ESP_LOGE(TAG, "  FAILURE REASON: %s", this->fail_reason_);
  }
  LOG_UPDATE_INTERVAL(this);
}

void IT8951Display::draw_absolute_pixel_internal(int x, int y, Color color) {
  const int width = this->get_width_internal();
  const int height = this->get_height_internal();
  if (x < 0 || x >= width || y < 0 || y >= height) {
    return;
  }
  if (this->framebuffer_ == nullptr) {
    return;
  }

  const uint32_t pos = (x + y * width) / 2;
  const uint8_t pixel_val = color.is_on() ? 0x00 : 0x0F;

  if ((x % 2) == 0) {
    this->framebuffer_[pos] = (this->framebuffer_[pos] & 0x0F) | (pixel_val << 4);
  } else {
    this->framebuffer_[pos] = (this->framebuffer_[pos] & 0xF0) | pixel_val;
  }
}

void IT8951Display::lcd_send_cmd_arg_(uint16_t cmd, uint16_t *args, uint16_t num_args) {
  this->lcd_write_cmd_code_(cmd);
  for (uint16_t i = 0; i < num_args; i++) {
    this->lcd_write_data_(args[i]);
  }
}

uint16_t IT8951Display::it8951_read_reg_(uint16_t addr) {
  this->lcd_write_cmd_code_(IT8951_TCON_REG_RD);
  this->lcd_write_data_(addr);
  return this->lcd_read_data_();
}

void IT8951Display::it8951_write_reg_(uint16_t addr, uint16_t val) {
  this->lcd_write_cmd_code_(IT8951_TCON_REG_WR);
  this->lcd_write_data_(addr);
  this->lcd_write_data_(val);
}

void IT8951Display::get_it8951_system_info_() {
  memset(&this->dev_info_, 0, sizeof(this->dev_info_));
  this->lcd_write_cmd_code_(USDEF_I80_CMD_GET_DEV_INFO);
  this->lcd_read_n_data_(reinterpret_cast<uint16_t *>(&this->dev_info_), sizeof(IT8951DevInfo) / 2);
}

void IT8951Display::set_img_buf_base_addr_(uint32_t addr) {
  const uint16_t hi = (addr >> 16) & 0xFFFF;
  const uint16_t lo = addr & 0xFFFF;
  this->it8951_write_reg_(LISAR + 2, hi);
  this->it8951_write_reg_(LISAR, lo);
}

void IT8951Display::it8951_load_img_area_start_(uint16_t endian, uint16_t pix_fmt, uint16_t rotate, uint16_t x,
                                                uint16_t y, uint16_t w, uint16_t h) {
  uint16_t args[5];
  args[0] = (endian << 8) | (pix_fmt << 4) | rotate;
  args[1] = x;
  args[2] = y;
  args[3] = w;
  args[4] = h;
  this->lcd_send_cmd_arg_(IT8951_TCON_LD_IMG_AREA, args, 5);
}

void IT8951Display::it8951_display_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t mode) {
  this->lcd_write_cmd_code_(USDEF_I80_CMD_DPY_AREA);
  this->lcd_write_data_(x);
  this->lcd_write_data_(y);
  this->lcd_write_data_(w);
  this->lcd_write_data_(h);
  this->lcd_write_data_(mode);
}

void IT8951Display::it8951_display_area_1bpp_(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t mode,
                                              uint8_t bg_gray, uint8_t fg_gray) {
  this->it8951_write_reg_(UP1SR + 2, this->it8951_read_reg_(UP1SR + 2) | (1 << 2));
  this->it8951_write_reg_(BGVR, (uint16_t(bg_gray) << 8) | fg_gray);
  this->it8951_display_area_(x, y, w, h, mode);
  this->wait_for_display_ready_();
}

}  // namespace it8951
}  // namespace esphome

#endif  // USE_ESP32
