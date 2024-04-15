#include "i80_component.h"
#ifdef USE_I80
#ifdef USE_ESP_IDF
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include <vector>
#include <cstring>

namespace esphome {
namespace i80 {

static const size_t MAX_TRANSFER = 4092;

static volatile bool write_complete = true;  // NOLINT
static bool trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
  write_complete = true;
  return true;
}

class I80DelegateIdf : public I80Delegate {
 public:
  I80DelegateIdf(esp_lcd_panel_io_t *handle, GPIOPin *cs_pin) : handle_(handle) {
    cs_pin->setup();
    cs_pin->digital_write(true);
    this->cs_pin_ = cs_pin;
  }
  void begin_transaction() override { this->cs_pin_->digital_write(false); }

  void end_transaction() override { this->cs_pin_->digital_write(true); }

  void write_array(const uint8_t *data, size_t length) override {
    while (length != 0) {
      write_complete = false;
      auto chunk = length;
      if (chunk > MAX_TRANSFER)
        chunk = MAX_TRANSFER;
      auto err = esp_lcd_panel_io_tx_color(this->handle_, -1, data, chunk);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Data write failed- err %X", err);
        return;
      }
      length -= chunk;
      data += chunk;
      while (!write_complete)
        continue;
    }
  }

  void write_cmd_data(int cmd, const uint8_t *data, size_t length) override {
    this->begin_transaction();
    if (length <= CONFIG_LCD_PANEL_IO_FORMAT_BUF_SIZE) {
      ESP_LOGV(TAG, "Send command %X, len %u", cmd, length);
      auto err = esp_lcd_panel_io_tx_param(this->handle_, cmd, data, length);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Data write failed- err %X", err);
      }
    } else {
      auto err = esp_lcd_panel_io_tx_param(this->handle_, cmd, nullptr, 0);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Command write failed- err %X", err);
        return;
      }
      this->write_array(data, length);
    }
    this->end_transaction();
  }

 protected:
  esp_lcd_panel_io_handle_t handle_;
  GPIOPin *cs_pin_;
};

class I80BusIdf : public I80Bus {
 public:
  I80BusIdf(InternalGPIOPin *wr_pin, InternalGPIOPin *dc_pin, std::vector<uint8_t> data_pins) {
    esp_lcd_i80_bus_config_t config = {};
    config.bus_width = 8;
    config.dc_gpio_num = dc_pin->get_pin();
    config.wr_gpio_num = wr_pin->get_pin();
    config.clk_src = LCD_CLK_SRC_PLL160M;
    config.max_transfer_bytes = MAX_TRANSFER;
    config.psram_trans_align = 16;
    config.sram_trans_align = 16;
    for (size_t i = 0; i != data_pins.size(); i++) {
      config.data_gpio_nums[i] = data_pins[i];
    }
    auto err = esp_lcd_new_i80_bus(&config, &this->handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Bus init failed - err %X", err);
      this->handle_ = nullptr;
    }
  }

  I80Delegate *get_delegate(GPIOPin *cs_pin, uint32_t data_rate) override {
    esp_lcd_panel_io_i80_config_t config = {};
    config.on_color_trans_done = trans_done;
    config.cs_gpio_num = -1;
    config.pclk_hz = data_rate;
    config.trans_queue_depth = 1;
    config.lcd_cmd_bits = 8;
    config.lcd_param_bits = 8;
    config.dc_levels.dc_cmd_level = 0;
    config.dc_levels.dc_data_level = 1;
    config.flags.pclk_active_neg = false;
    config.flags.pclk_idle_low = false;

    esp_lcd_panel_io_handle_t d_handle;
    auto err = esp_lcd_new_panel_io_i80(this->handle_, &config, &d_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Panel init failed - err %X", err);
      return NULL_DELEGATE;
    }
    return new I80DelegateIdf(d_handle, cs_pin);  // NOLINT
  }

  bool is_failed() { return this->handle_ == nullptr; }

 protected:
  esp_lcd_i80_bus_handle_t handle_{};
};

void I80Component::setup() {
  auto *bus = new I80BusIdf(this->wr_pin_, this->dc_pin_, this->data_pins_);  // NOLINT
  // write pin is unused, but pulled high (inactive) if present
  if (this->rd_pin_ != nullptr) {
    this->rd_pin_->setup();
    this->rd_pin_->digital_write(true);
  }
  if (bus->is_failed())
    this->mark_failed();
  this->bus_ = bus;
}
I80Delegate *I80Component::register_device(I80Client *device, GPIOPin *cs_pin, unsigned int data_rate) {
  if (this->devices_.count(device) != 0) {
    ESP_LOGE(TAG, "i80 device already registered");
    return this->devices_[device];
  }
  auto *delegate = this->bus_->get_delegate(cs_pin, data_rate);  // NOLINT
  this->devices_[device] = delegate;
  return delegate;
}
void I80Component::unregister_device(I80Client *device) {
  if (this->devices_.count(device) == 0) {
    esph_log_e(TAG, "i80 device not registered");
    return;
  }
  delete this->devices_[device];  // NOLINT
  this->devices_.erase(device);
}

void I80Client::dump_config() {
  ESP_LOGCONFIG(TAG, "  Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
  LOG_PIN("  CS Pin: ", this->cs_);
}
}  // namespace i80
}  // namespace esphome
#endif  // USE_ESP_IDF
#endif  // USE_I80
