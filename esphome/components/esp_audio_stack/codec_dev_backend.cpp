#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_ESP_AUDIO_STACK_HARDWARE_CODEC)

#include "codec_dev_backend.h"

#include <audio_codec_ctrl_if.h>
#include <audio_codec_data_if.h>
#include <audio_codec_if.h>
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES7210
#include <es7210_adc.h>
#endif
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8311
#include <es8311_codec.h>
#endif
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8374
#include <es8374_codec.h>
#endif
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8388
#include <es8388_codec.h>
#endif
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8389
#include <es8389_codec.h>
#endif
#include <esp_codec_dev_types.h>
#include <freertos/FreeRTOS.h>

#ifdef USE_I2C
#include "esphome/components/i2c/i2c_bus.h"
#endif
#include "esphome/core/log.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace esphome::esp_audio_stack {

static const char *const TAG = "i2s_codec_dev";

namespace {

#ifdef USE_I2C
struct EsphomeI2cCtrl {
  audio_codec_ctrl_if_t base;
  i2c::I2CBus *bus;
  uint8_t address;
  bool open;
};

static int ctrl_open(const audio_codec_ctrl_if_t *ctrl, void *cfg, int cfg_size) {
  (void) cfg;
  (void) cfg_size;
  if (ctrl == nullptr) {
    return ESP_CODEC_DEV_INVALID_ARG;
  }
  auto *self = reinterpret_cast<EsphomeI2cCtrl *>(const_cast<audio_codec_ctrl_if_t *>(ctrl));
  self->open = self->bus != nullptr;
  return self->open ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_INVALID_ARG;
}

static bool ctrl_is_open(const audio_codec_ctrl_if_t *ctrl) {
  if (ctrl == nullptr) {
    return false;
  }
  auto *self = reinterpret_cast<EsphomeI2cCtrl *>(const_cast<audio_codec_ctrl_if_t *>(ctrl));
  return self->open;
}

static bool encode_reg(int reg, int reg_len, uint8_t *out) {
  if (out == nullptr || reg_len < 1 || reg_len > 2) {
    return false;
  }
  if (reg_len == 2) {
    out[0] = static_cast<uint8_t>((reg >> 8) & 0xFF);
    out[1] = static_cast<uint8_t>(reg & 0xFF);
  } else {
    out[0] = static_cast<uint8_t>(reg & 0xFF);
  }
  return true;
}

static int ctrl_read_reg(const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int data_len) {
  if (ctrl == nullptr || data == nullptr || data_len <= 0) {
    return ESP_CODEC_DEV_INVALID_ARG;
  }
  auto *self = reinterpret_cast<EsphomeI2cCtrl *>(const_cast<audio_codec_ctrl_if_t *>(ctrl));
  if (!self->open || self->bus == nullptr) {
    return ESP_CODEC_DEV_WRONG_STATE;
  }
  uint8_t reg_buf[2]{};
  if (!encode_reg(reg, reg_len, reg_buf)) {
    return ESP_CODEC_DEV_INVALID_ARG;
  }
  auto result = self->bus->write_readv(self->address, reg_buf, reg_len, static_cast<uint8_t *>(data), data_len);
  return result == i2c::NO_ERROR ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_READ_FAIL;
}

static int ctrl_write_reg(const audio_codec_ctrl_if_t *ctrl, int reg, int reg_len, void *data, int data_len) {
  if (ctrl == nullptr || data == nullptr || data_len < 0) {
    return ESP_CODEC_DEV_INVALID_ARG;
  }
  auto *self = reinterpret_cast<EsphomeI2cCtrl *>(const_cast<audio_codec_ctrl_if_t *>(ctrl));
  if (!self->open || self->bus == nullptr) {
    return ESP_CODEC_DEV_WRONG_STATE;
  }
  if (reg_len < 1 || reg_len > 2 || data_len > 8) {
    return ESP_CODEC_DEV_NOT_SUPPORT;
  }
  uint8_t write_buf[10]{};
  if (!encode_reg(reg, reg_len, write_buf)) {
    return ESP_CODEC_DEV_INVALID_ARG;
  }
  memcpy(write_buf + reg_len, data, data_len);
  auto result = self->bus->write_readv(self->address, write_buf, reg_len + data_len, nullptr, 0);
  return result == i2c::NO_ERROR ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static int ctrl_close(const audio_codec_ctrl_if_t *ctrl) {
  if (ctrl == nullptr) {
    return ESP_CODEC_DEV_INVALID_ARG;
  }
  auto *self = reinterpret_cast<EsphomeI2cCtrl *>(const_cast<audio_codec_ctrl_if_t *>(ctrl));
  self->open = false;
  return ESP_CODEC_DEV_OK;
}
#endif  // USE_I2C

}  // namespace

CodecDevBackend::~CodecDevBackend() { this->teardown(); }

const audio_codec_ctrl_if_t *CodecDevBackend::new_i2c_ctrl_(uint8_t address) {
#ifdef USE_I2C
  if (this->i2c_bus_ == nullptr) {
    ESP_LOGE(TAG, "Codec I2C bus is not configured");
    return nullptr;
  }
  auto *ctrl = static_cast<EsphomeI2cCtrl *>(calloc(1, sizeof(EsphomeI2cCtrl)));
  if (ctrl == nullptr) {
    return nullptr;
  }
  ctrl->base.open = ctrl_open;
  ctrl->base.is_open = ctrl_is_open;
  ctrl->base.read_reg = ctrl_read_reg;
  ctrl->base.write_reg = ctrl_write_reg;
  ctrl->base.close = ctrl_close;
  ctrl->bus = this->i2c_bus_;
  ctrl->address = address;
  // Espressif's audio_codec_new_i2c_ctrl() returns an already-open control
  // interface; ES7210/ES8311 constructors check that before creating codecs.
  ctrl->open = true;
  return &ctrl->base;
#else
  (void) address;
  ESP_LOGE(TAG, "Codec I2C support is not compiled in");
  return nullptr;
#endif
}

esp_codec_dev_sample_info_t CodecDevBackend::make_sample_info_(const SampleConfig &config) {
  return {
      .bits_per_sample = config.bits_per_sample,
      .channel = config.channels,
      .channel_mask = config.channel_mask,
      .sample_rate = config.sample_rate,
      .mclk_multiple = static_cast<int>(config.mclk_multiple),
  };
}

const char *CodecDevBackend::codec_kind_name_(CodecKind kind) {
  switch (kind) {
    case CodecKind::ES8311:
      return "ES8311";
    case CodecKind::ES8388:
      return "ES8388";
    case CodecKind::ES8374:
      return "ES8374";
    case CodecKind::ES8389:
      return "ES8389";
    default:
      return "none";
  }
}

const char *CodecDevBackend::input_codec_name() const {
  if (this->es7210_.enabled) {
    return "ES7210";
  }
  if (this->input_codec_.enabled) {
    return codec_kind_name_(this->input_codec_.kind);
  }
  return "none";
}

const char *CodecDevBackend::output_codec_name() const {
  return this->output_codec_.enabled ? codec_kind_name_(this->output_codec_.kind) : "none";
}

const audio_codec_if_t *CodecDevBackend::new_generic_codec_(const GenericCodecConfig &config, bool input,
                                                            uint16_t mclk_div, const audio_codec_ctrl_if_t **ctrl) {
  if (!config.enabled || ctrl == nullptr) {
    return nullptr;
  }
  *ctrl = this->new_i2c_ctrl_(config.address);
  if (*ctrl == nullptr) {
    return nullptr;
  }
  const auto mode = input ? ESP_CODEC_DEV_WORK_MODE_ADC : ESP_CODEC_DEV_WORK_MODE_DAC;
  switch (config.kind) {
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8311
    case CodecKind::ES8311: {
      es8311_codec_cfg_t cfg = {};
      cfg.ctrl_if = *ctrl;
      cfg.gpio_if = nullptr;
      cfg.codec_mode = mode;
      cfg.pa_pin = -1;
      cfg.master_mode = false;
      cfg.use_mclk = config.use_mclk;
      cfg.no_dac_ref = config.no_dac_ref;
      cfg.mclk_div = mclk_div;
      return es8311_codec_new(&cfg);
    }
#endif
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8388
    case CodecKind::ES8388: {
      es8388_codec_cfg_t cfg = {};
      cfg.ctrl_if = *ctrl;
      cfg.gpio_if = nullptr;
      cfg.codec_mode = mode;
      cfg.pa_pin = -1;
      cfg.master_mode = false;
      return es8388_codec_new(&cfg);
    }
#endif
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8374
    case CodecKind::ES8374: {
      es8374_codec_cfg_t cfg = {};
      cfg.ctrl_if = *ctrl;
      cfg.gpio_if = nullptr;
      cfg.codec_mode = mode;
      cfg.pa_pin = -1;
      cfg.master_mode = false;
      return es8374_codec_new(&cfg);
    }
#endif
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES8389
    case CodecKind::ES8389: {
      es8389_codec_cfg_t cfg = {};
      cfg.ctrl_if = *ctrl;
      cfg.gpio_if = nullptr;
      cfg.codec_mode = mode;
      cfg.pa_pin = -1;
      cfg.master_mode = false;
      cfg.use_mclk = config.use_mclk;
      cfg.digital_mic = false;
      cfg.invert_mclk = false;
      cfg.invert_sclk = false;
      cfg.no_dac_ref = config.no_dac_ref;
      cfg.mclk_div = mclk_div;
      return es8389_codec_new(&cfg);
    }
#endif
    default:
      ESP_LOGE(TAG, "Unsupported codec kind: %u", static_cast<unsigned>(config.kind));
      return nullptr;
  }
}

bool CodecDevBackend::setup(uint8_t tx_i2s_port, uint8_t rx_i2s_port, i2s_chan_handle_t tx_handle,
                            i2s_chan_handle_t rx_handle, i2s_clock_src_t clk_src, uint32_t mclk_multiple) {
  this->teardown();
  const uint16_t codec_mclk_div = mclk_multiple == 0 ? 256 : static_cast<uint16_t>(mclk_multiple);

#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  const bool shared_i2s_data = tx_i2s_port == rx_i2s_port;
  if (shared_i2s_data) {
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = tx_i2s_port,
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
        .clk_src = static_cast<int>(clk_src),
    };
    this->tx_data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    if (this->tx_data_if_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create shared esp_codec_dev I2S data interface");
      return false;
    }
    this->rx_data_if_ = this->tx_data_if_;
  } else {
    if (rx_handle != nullptr) {
      audio_codec_i2s_cfg_t rx_i2s_cfg = {
          .port = rx_i2s_port,
          .rx_handle = rx_handle,
          .tx_handle = nullptr,
          .clk_src = static_cast<int>(clk_src),
      };
      this->rx_data_if_ = audio_codec_new_i2s_data(&rx_i2s_cfg);
      if (this->rx_data_if_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create RX esp_codec_dev I2S data interface");
        return false;
      }
    }
    if (tx_handle != nullptr) {
      audio_codec_i2s_cfg_t tx_i2s_cfg = {
          .port = tx_i2s_port,
          .rx_handle = nullptr,
          .tx_handle = tx_handle,
          .clk_src = static_cast<int>(clk_src),
      };
      this->tx_data_if_ = audio_codec_new_i2s_data(&tx_i2s_cfg);
      if (this->tx_data_if_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create TX esp_codec_dev I2S data interface");
        return false;
      }
    }
  }
#else
  audio_codec_i2s_cfg_t i2s_cfg = {
      .port = tx_i2s_port,
      .rx_handle = rx_handle,
      .tx_handle = tx_handle,
      .clk_src = static_cast<int>(clk_src),
  };
  this->data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
  if (this->data_if_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create esp_codec_dev I2S data interface");
    return false;
  }
#endif

  if (rx_handle != nullptr && this->es7210_.enabled) {
#ifdef USE_ESP_AUDIO_STACK_CODEC_ES7210
    this->es7210_ctrl_ = this->new_i2c_ctrl_(this->es7210_.address);
    if (this->es7210_ctrl_ == nullptr) {
      return false;
    }
    es7210_codec_cfg_t cfg = {};
    cfg.ctrl_if = this->es7210_ctrl_;
    cfg.master_mode = false;
    cfg.mic_selected = this->es7210_.mic_selected;
    cfg.mclk_div = codec_mclk_div;
    this->rx_codec_if_ = es7210_codec_new(&cfg);
    if (this->rx_codec_if_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create ES7210 codec interface");
      return false;
    }
#else
    ESP_LOGE(TAG, "ES7210 codec support was not compiled in");
    return false;
#endif
  } else if (rx_handle != nullptr && this->input_codec_.enabled) {
    this->rx_codec_if_ = this->new_generic_codec_(this->input_codec_, true, codec_mclk_div, &this->input_codec_ctrl_);
    if (this->rx_codec_if_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create %s ADC codec interface", this->input_codec_name());
      return false;
    }
  }

  if (tx_handle != nullptr && this->output_codec_.enabled) {
    this->tx_codec_if_ =
        this->new_generic_codec_(this->output_codec_, false, codec_mclk_div, &this->output_codec_ctrl_);
    if (this->tx_codec_if_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create %s DAC codec interface", this->output_codec_name());
      return false;
    }
  }

  if (rx_handle != nullptr) {
    esp_codec_dev_cfg_t rx_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = this->rx_codec_if_,
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
        .data_if = this->rx_data_if_,
#else
        .data_if = this->data_if_,
#endif
    };
    this->rx_dev_ = esp_codec_dev_new(&rx_cfg);
    if (this->rx_dev_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create esp_codec_dev RX device");
      return false;
    }
  }

  if (tx_handle != nullptr) {
    esp_codec_dev_cfg_t tx_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = this->tx_codec_if_,
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
        .data_if = this->tx_data_if_,
#else
        .data_if = this->data_if_,
#endif
    };
    this->tx_dev_ = esp_codec_dev_new(&tx_cfg);
    if (this->tx_dev_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create esp_codec_dev TX device");
      return false;
    }
  }

  this->prepared_ = true;
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  ESP_LOGI(TAG, "esp_codec_dev backend ready (rx_codec=%s, tx_codec=%s, data_if=%s)", this->input_codec_name(),
           this->output_codec_name(), shared_i2s_data ? "shared" : "split");
#else
  ESP_LOGI(TAG, "esp_codec_dev backend ready (rx_codec=%s, tx_codec=%s)", this->input_codec_name(),
           this->output_codec_name());
#endif
  return true;
}

bool CodecDevBackend::open(const SampleConfig *tx_config, const SampleConfig *rx_config) {
  if (!this->prepared_) {
    return false;
  }
  if (this->open_) {
    return true;
  }

  // Match Espressif's TDM codec-dev tests: bring up playback first, then record,
  // so the TX side has configured the shared clock before RX starts consuming it.
  if (this->tx_dev_ != nullptr && tx_config != nullptr) {
    auto fs = make_sample_info_(*tx_config);
    int ret = esp_codec_dev_open(this->tx_dev_, &fs);
    if (ret != ESP_CODEC_DEV_OK) {
      ESP_LOGE(TAG, "Failed to open TX codec device: %d", ret);
      return false;
    }
    this->apply_output_volume_curve_();
  }
  if (this->rx_dev_ != nullptr && rx_config != nullptr) {
    auto fs = make_sample_info_(*rx_config);
    int ret = esp_codec_dev_open(this->rx_dev_, &fs);
    if (ret != ESP_CODEC_DEV_OK) {
      ESP_LOGE(TAG, "Failed to open RX codec device: %d", ret);
      if (this->tx_dev_ != nullptr) {
        esp_codec_dev_close(this->tx_dev_);
      }
      return false;
    }
    if (this->es7210_.enabled) {
      this->set_input_gain(this->es7210_.input_gain_db);
      if (this->es7210_.has_ref_channel_gain) {
        this->set_input_channel_gain(this->es7210_.ref_channel, this->es7210_.ref_channel_gain_db);
      }
    } else if (this->input_codec_.enabled) {
      this->set_input_gain(this->input_codec_.input_gain_db);
    }
  }
  this->open_ = true;
  return true;
}

void CodecDevBackend::close() {
  if (!this->open_) {
    return;
  }
  if (this->rx_dev_ != nullptr) {
    esp_codec_dev_close(this->rx_dev_);
  }
  if (this->tx_dev_ != nullptr) {
    esp_codec_dev_close(this->tx_dev_);
  }
  this->open_ = false;
}

bool CodecDevBackend::read(void *data, size_t len) {
  if (this->rx_dev_ == nullptr || data == nullptr || len == 0) {
    return false;
  }
  const int ret = esp_codec_dev_read(this->rx_dev_, data, static_cast<int>(len));
  if (ret != ESP_CODEC_DEV_OK) {
    return false;
  }
  return true;
}

bool CodecDevBackend::write(void *data, size_t len) {
  if (this->tx_dev_ == nullptr || data == nullptr || len == 0) {
    return false;
  }
  const int ret = esp_codec_dev_write(this->tx_dev_, data, static_cast<int>(len));
  if (ret != ESP_CODEC_DEV_OK) {
    return false;
  }
  return true;
}

void CodecDevBackend::set_output_volume(float volume) {
  if (this->tx_dev_ == nullptr) {
    return;
  }
  if (!(volume > 0.0f)) {
    volume = 0.0f;
  } else if (volume > 1.0f) {
    volume = 1.0f;
  }
  esp_codec_dev_set_out_vol(this->tx_dev_, static_cast<int>(volume * 100.0f + 0.5f));
}

void CodecDevBackend::set_output_volume_curve(float min_db) {
  if (!std::isfinite(min_db)) {
    return;
  }
  if (min_db < -96.0f)
    min_db = -96.0f;
  if (min_db > 0.0f)
    min_db = 0.0f;
  this->output_volume_min_db_ = min_db;
  this->output_volume_curve_configured_ = true;
  this->apply_output_volume_curve_();
}

void CodecDevBackend::apply_output_volume_curve_() {
  if (!this->output_volume_curve_configured_ || this->tx_dev_ == nullptr || !this->output_codec_.enabled) {
    return;
  }
  esp_codec_dev_vol_map_t vol_map[2] = {
      {.vol = 1, .db_value = this->output_volume_min_db_},
      {.vol = 100, .db_value = 0.0f},
  };
  esp_codec_dev_vol_curve_t curve = {
      .vol_map = vol_map,
      .count = 2,
  };
  const int ret = esp_codec_dev_set_vol_curve(this->tx_dev_, &curve);
  if (ret != ESP_CODEC_DEV_OK) {
    ESP_LOGW(TAG, "Failed to set codec output volume curve: %d", ret);
  }
}

void CodecDevBackend::set_output_mute(bool mute) {
  if (this->tx_dev_ != nullptr) {
    esp_codec_dev_set_out_mute(this->tx_dev_, mute);
  }
}

void CodecDevBackend::set_input_gain(float gain_db) {
  if (this->rx_dev_ != nullptr) {
    esp_codec_dev_set_in_gain(this->rx_dev_, gain_db);
  }
}

void CodecDevBackend::set_input_channel_gain(uint8_t channel, float gain_db) {
  if (this->rx_dev_ != nullptr && channel < 16) {
    esp_codec_dev_set_in_channel_gain(this->rx_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(channel), gain_db);
  }
}

void CodecDevBackend::destroy_codecs_() {
  if (this->rx_codec_if_ != nullptr) {
    audio_codec_delete_codec_if(this->rx_codec_if_);
    this->rx_codec_if_ = nullptr;
  }
  if (this->tx_codec_if_ != nullptr) {
    audio_codec_delete_codec_if(this->tx_codec_if_);
    this->tx_codec_if_ = nullptr;
  }
  if (this->es7210_ctrl_ != nullptr) {
    audio_codec_delete_ctrl_if(this->es7210_ctrl_);
    this->es7210_ctrl_ = nullptr;
  }
  if (this->input_codec_ctrl_ != nullptr) {
    audio_codec_delete_ctrl_if(this->input_codec_ctrl_);
    this->input_codec_ctrl_ = nullptr;
  }
  if (this->output_codec_ctrl_ != nullptr) {
    audio_codec_delete_ctrl_if(this->output_codec_ctrl_);
    this->output_codec_ctrl_ = nullptr;
  }
}

void CodecDevBackend::teardown() {
  this->close();
  if (this->rx_dev_ != nullptr) {
    esp_codec_dev_delete(this->rx_dev_);
    this->rx_dev_ = nullptr;
  }
  if (this->tx_dev_ != nullptr) {
    esp_codec_dev_delete(this->tx_dev_);
    this->tx_dev_ = nullptr;
  }
  this->destroy_codecs_();
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  const audio_codec_data_if_t *rx_data_if = this->rx_data_if_;
  const audio_codec_data_if_t *tx_data_if = this->tx_data_if_;
  if (rx_data_if != nullptr) {
    audio_codec_delete_data_if(this->rx_data_if_);
    this->rx_data_if_ = nullptr;
  }
  if (tx_data_if != nullptr && tx_data_if != rx_data_if) {
    audio_codec_delete_data_if(this->tx_data_if_);
  }
  this->tx_data_if_ = nullptr;
#else
  if (this->data_if_ != nullptr) {
    audio_codec_delete_data_if(this->data_if_);
    this->data_if_ = nullptr;
  }
#endif
  this->prepared_ = false;
  this->open_ = false;
}

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32 && USE_ESP_AUDIO_STACK_HARDWARE_CODEC
