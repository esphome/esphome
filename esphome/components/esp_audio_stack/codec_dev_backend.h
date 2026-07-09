#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_ESP_AUDIO_STACK_HARDWARE_CODEC)

#include <driver/i2s_types.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <hal/i2s_types.h>

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace i2c {
class I2CBus;
}  // namespace i2c
}  // namespace esphome

namespace esphome::esp_audio_stack {

class CodecDevBackend {
 public:
  enum class CodecKind : uint8_t {
    NONE = 0,
    ES8311,
    ES8388,
    ES8374,
    ES8389,
  };

  struct Es7210Config {
    bool enabled{false};
    uint8_t address{0x40};
    uint8_t mic_selected{0x0F};
    float input_gain_db{30.0f};
    bool has_ref_channel_gain{false};
    uint8_t ref_channel{1};
    float ref_channel_gain_db{0.0f};
  };

  struct GenericCodecConfig {
    bool enabled{false};
    CodecKind kind{CodecKind::NONE};
    uint8_t address{0x18};
    bool use_mclk{true};
    bool no_dac_ref{true};
    float input_gain_db{24.0f};
  };

  struct SampleConfig {
    uint32_t sample_rate{16000};
    uint8_t bits_per_sample{16};
    uint8_t channels{2};
    uint16_t channel_mask{0x0003};
    uint32_t mclk_multiple{256};
  };

  CodecDevBackend() = default;
  ~CodecDevBackend();

  CodecDevBackend(const CodecDevBackend &) = delete;
  CodecDevBackend &operator=(const CodecDevBackend &) = delete;

  void set_i2c_bus(i2c::I2CBus *bus) { this->i2c_bus_ = bus; }
  void set_es7210_config(const Es7210Config &config) { this->es7210_ = config; }
  void set_input_codec_config(const GenericCodecConfig &config) { this->input_codec_ = config; }
  void set_output_codec_config(const GenericCodecConfig &config) { this->output_codec_ = config; }

  bool setup(uint8_t tx_i2s_port, uint8_t rx_i2s_port, i2s_chan_handle_t tx_handle, i2s_chan_handle_t rx_handle,
             i2s_clock_src_t clk_src, uint32_t mclk_multiple);
  bool open(const SampleConfig *tx_config, const SampleConfig *rx_config);
  void close();
  void teardown();

  bool read(void *data, size_t len);
  bool write(void *data, size_t len);

  void set_output_volume(float volume);
  void set_output_volume_curve(float min_db);
  void set_output_mute(bool mute);
  void set_input_gain(float gain_db);
  void set_input_channel_gain(uint8_t channel, float gain_db);

  bool has_tx() const { return this->tx_dev_ != nullptr; }
  bool has_rx() const { return this->rx_dev_ != nullptr; }
  bool is_open() const { return this->open_; }
  bool has_output_codec() const { return this->output_codec_.enabled; }
  bool has_input_codec() const { return this->es7210_.enabled || this->input_codec_.enabled; }
  const char *input_codec_name() const;
  const char *output_codec_name() const;

 private:
  static const char *codec_kind_name_(CodecKind kind);
  static esp_codec_dev_sample_info_t make_sample_info_(const SampleConfig &config);

  const audio_codec_ctrl_if_t *new_i2c_ctrl_(uint8_t address);
  void apply_output_volume_curve_();
  const audio_codec_if_t *new_generic_codec_(const GenericCodecConfig &config, bool input, uint16_t mclk_div,
                                             const audio_codec_ctrl_if_t **ctrl);
  void destroy_codecs_();

  i2c::I2CBus *i2c_bus_{nullptr};
  Es7210Config es7210_{};
  GenericCodecConfig input_codec_{};
  GenericCodecConfig output_codec_{};

#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  const audio_codec_data_if_t *rx_data_if_{nullptr};
  const audio_codec_data_if_t *tx_data_if_{nullptr};
#else
  const audio_codec_data_if_t *data_if_{nullptr};
#endif
  const audio_codec_ctrl_if_t *es7210_ctrl_{nullptr};
  const audio_codec_ctrl_if_t *input_codec_ctrl_{nullptr};
  const audio_codec_ctrl_if_t *output_codec_ctrl_{nullptr};
  const audio_codec_if_t *rx_codec_if_{nullptr};
  const audio_codec_if_t *tx_codec_if_{nullptr};
  esp_codec_dev_handle_t rx_dev_{nullptr};
  esp_codec_dev_handle_t tx_dev_{nullptr};
  bool output_volume_curve_configured_{false};
  float output_volume_min_db_{-49.0f};

  bool prepared_{false};
  bool open_{false};
};

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32 && USE_ESP_AUDIO_STACK_HARDWARE_CODEC
