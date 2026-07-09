#include "esp_audio_stack.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cstring>

#if __has_include(<esp_ae_bit_cvt.h>) && __has_include(<esp_ae_data_weaver.h>) && __has_include(<esp_ae_rate_cvt.h>)
#include <esp_ae_bit_cvt.h>
#include <esp_ae_data_weaver.h>
#include <esp_ae_rate_cvt.h>
#endif
#include <esp_heap_caps.h>
#include <esp_log.h>

namespace esphome::esp_audio_stack {

#if __has_include(<esp_ae_bit_cvt.h>) && __has_include(<esp_ae_data_weaver.h>) && __has_include(<esp_ae_rate_cvt.h>)

static const char *const TAG = "i2s_ae_fx";

namespace {
constexpr uint8_t MAX_DEINTLV_CH = 8;

int16_t *alloc_internal(size_t count, const char *who) {
  auto *p = static_cast<int16_t *>(heap_caps_malloc(count * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (p == nullptr) {
    ESP_LOGE(who, "audio effects buffer alloc failed: %u bytes, %u internal free, %u largest",
             static_cast<unsigned>(count * sizeof(int16_t)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  }
  return p;
}

bool ensure_buffer(int16_t *&ptr, size_t &cap, size_t needed, const char *who) {
  if (cap >= needed)
    return true;
  if (ptr != nullptr)
    heap_caps_free(ptr);
  ptr = alloc_internal(needed, who);
  cap = ptr != nullptr ? needed : 0;
  return ptr != nullptr;
}

class BitCvtHandle {
 public:
  ~BitCvtHandle() { this->close_handle_(); }

  void init(uint32_t sample_rate, uint8_t channel, uint8_t src_bits, uint8_t dest_bits) {
    if (this->sample_rate_ == sample_rate && this->channel_ == channel && this->src_bits_ == src_bits &&
        this->dest_bits_ == dest_bits) {
      return;
    }
    this->sample_rate_ = sample_rate;
    this->channel_ = channel;
    this->src_bits_ = src_bits;
    this->dest_bits_ = dest_bits;
    this->close_handle_();
  }

  bool ready() {
    if (this->src_bits_ == this->dest_bits_)
      return true;
    if (this->handle_ != nullptr)
      return true;
    if (this->sample_rate_ == 0 || this->channel_ == 0 || this->src_bits_ == 0 || this->dest_bits_ == 0) {
      ESP_LOGE(TAG, "invalid esp_ae_bit_cvt config: rate=%u ch=%u src=%u dest=%u",
               static_cast<unsigned>(this->sample_rate_), static_cast<unsigned>(this->channel_),
               static_cast<unsigned>(this->src_bits_), static_cast<unsigned>(this->dest_bits_));
      return false;
    }

    esp_ae_bit_cvt_cfg_t cfg{};
    cfg.sample_rate = this->sample_rate_;
    cfg.channel = this->channel_;
    cfg.src_bits = this->src_bits_;
    cfg.dest_bits = this->dest_bits_;
    const esp_ae_err_t err = esp_ae_bit_cvt_open(&cfg, &this->handle_);
    if (err == ESP_AE_ERR_OK && this->handle_ != nullptr)
      return true;

    ESP_LOGE(TAG, "esp_ae_bit_cvt_open failed: err=%d rate=%u ch=%u src=%u dest=%u", static_cast<int>(err),
             static_cast<unsigned>(this->sample_rate_), static_cast<unsigned>(this->channel_),
             static_cast<unsigned>(this->src_bits_), static_cast<unsigned>(this->dest_bits_));
    return false;
  }

  bool process(const void *in, uint32_t sample_num, void *out, const char *scope) {
    if (this->src_bits_ == this->dest_bits_) {
      const size_t bytes = static_cast<size_t>(sample_num) * this->channel_ * (this->src_bits_ >> 3);
      if (in != out)
        memcpy(out, in, bytes);
      return true;
    }
    if (!this->ready())
      return false;
    const esp_ae_err_t err = esp_ae_bit_cvt_process(this->handle_, sample_num, const_cast<void *>(in), out);
    if (err == ESP_AE_ERR_OK)
      return true;
    ESP_LOGE(TAG, "esp_ae_bit_cvt %s failed: err=%d samples=%u ch=%u src=%u dest=%u", scope, static_cast<int>(err),
             static_cast<unsigned>(sample_num), static_cast<unsigned>(this->channel_),
             static_cast<unsigned>(this->src_bits_), static_cast<unsigned>(this->dest_bits_));
    return false;
  }

 private:
  void close_handle_() {
    if (this->handle_ != nullptr) {
      esp_ae_bit_cvt_close(this->handle_);
      this->handle_ = nullptr;
    }
  }

  uint32_t sample_rate_{0};
  uint8_t channel_{0};
  uint8_t src_bits_{0};
  uint8_t dest_bits_{0};
  esp_ae_bit_cvt_handle_t handle_{nullptr};
};

[[maybe_unused]] bool distribute_channels(int16_t *const *ch, uint8_t nch, size_t count, int16_t *mic_interleaved,
                                          int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch) {
  if (nch == 0 || ch[0] == nullptr)
    return false;
  if (mic_mono != nullptr)
    memcpy(mic_mono, ch[0], count * sizeof(int16_t));

  if (num_mic_ch >= 2 && nch >= 2) {
    if (mic_interleaved != nullptr) {
      esp_ae_sample_t mic_in[2] = {ch[0], ch[1]};
      const esp_ae_err_t err = esp_ae_intlv_process(2, 16, static_cast<uint32_t>(count), mic_in, mic_interleaved);
      if (err != ESP_AE_ERR_OK) {
        ESP_LOGE(TAG, "esp_ae_intlv_process mic failed: err=%d samples=%u", static_cast<int>(err),
                 static_cast<unsigned>(count));
        return false;
      }
    }
    if (ref_out != nullptr && nch >= 3)
      memcpy(ref_out, ch[2], count * sizeof(int16_t));
  } else if (ref_out != nullptr && nch >= 2) {
    memcpy(ref_out, ch[1], count * sizeof(int16_t));
  }
  return true;
}

class RateCvtHandle {
 public:
  ~RateCvtHandle() { this->close_handle_(); }

  void init(uint32_t ratio, uint32_t src_rate, uint32_t dest_rate, uint8_t channels, uint8_t complexity,
            uint8_t perf_type) {
    this->ratio_ = ratio;
    this->src_rate_ = src_rate;
    this->dest_rate_ = dest_rate;
    this->channels_ = channels;
    this->complexity_ = std::min<uint8_t>(std::max<uint8_t>(complexity, 1), 3);
    this->perf_type_ = perf_type == 0 ? ESP_AE_RATE_CVT_PERF_TYPE_MEMORY : ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
    this->close_handle_();
  }

  void reset() { this->close_handle_(); }
  bool passthrough() const { return this->ratio_ <= 1; }

  bool ready() {
    if (this->passthrough())
      return true;
    if (this->handle_ != nullptr)
      return true;
    if (this->src_rate_ == 0 || this->dest_rate_ == 0 || this->src_rate_ == this->dest_rate_ || this->channels_ == 0) {
      ESP_LOGE(TAG, "invalid esp_ae_rate_cvt config: src=%u dest=%u ch=%u", static_cast<unsigned>(this->src_rate_),
               static_cast<unsigned>(this->dest_rate_), static_cast<unsigned>(this->channels_));
      return false;
    }

    esp_ae_rate_cvt_cfg_t cfg{};
    cfg.src_rate = this->src_rate_;
    cfg.dest_rate = this->dest_rate_;
    cfg.channel = this->channels_;
    cfg.bits_per_sample = 16;
    cfg.complexity = this->complexity_;
    cfg.perf_type = this->perf_type_;
    const esp_ae_err_t err = esp_ae_rate_cvt_open(&cfg, &this->handle_);
    if (err == ESP_AE_ERR_OK && this->handle_ != nullptr)
      return true;

    ESP_LOGE(TAG, "esp_ae_rate_cvt_open failed: err=%d src=%u dest=%u ch=%u", static_cast<int>(err),
             static_cast<unsigned>(this->src_rate_), static_cast<unsigned>(this->dest_rate_),
             static_cast<unsigned>(this->channels_));
    return false;
  }

  bool process(int16_t *in, size_t in_count, int16_t *out, size_t expected_out, const char *scope) {
    if (!this->ready())
      return false;
    uint32_t out_samples = static_cast<uint32_t>(expected_out);
    const esp_ae_err_t err =
        esp_ae_rate_cvt_process(this->handle_, in, static_cast<uint32_t>(in_count), out, &out_samples);
    return this->check_(scope, err, out_samples, expected_out);
  }

  bool process_deintlv(int16_t **in, size_t in_count, int16_t **out, size_t expected_out, const char *scope) {
    if (!this->ready())
      return false;
    esp_ae_sample_t in_args[MAX_RATE_CVT_CHANNELS]{};
    esp_ae_sample_t out_args[MAX_RATE_CVT_CHANNELS]{};
    for (uint8_t i = 0; i < this->channels_; i++) {
      in_args[i] = in[i];
      out_args[i] = out[i];
    }
    uint32_t out_samples = static_cast<uint32_t>(expected_out);
    const esp_ae_err_t err = esp_ae_rate_cvt_deintlv_process(this->handle_, in_args, static_cast<uint32_t>(in_count),
                                                             out_args, &out_samples);
    return this->check_(scope, err, out_samples, expected_out);
  }

 private:
  bool check_(const char *scope, esp_ae_err_t err, uint32_t actual, size_t expected) {
    // This wrapper intentionally supports exact integer decimation only.
    // CONFIG_SCHEMA rejects non-integer sample_rate/output_sample_rate pairs.
    // If non-integer SRC is ever allowed, this check must be redesigned to
    // propagate variable out_samples through every downstream consumer.
    if (err == ESP_AE_ERR_OK && actual == expected)
      return true;
    ESP_LOGE(TAG, "esp_ae_rate_cvt %s failed/misaligned: err=%d out=%u expected=%u ch=%u", scope, static_cast<int>(err),
             static_cast<unsigned>(actual), static_cast<unsigned>(expected), static_cast<unsigned>(this->channels_));
    return false;
  }

  void close_handle_() {
    if (this->handle_ != nullptr) {
      esp_ae_rate_cvt_close(this->handle_);
      this->handle_ = nullptr;
    }
  }

  uint32_t ratio_{1};
  uint32_t src_rate_{0};
  uint32_t dest_rate_{0};
  uint8_t channels_{0};
  uint8_t complexity_{3};
  esp_ae_rate_cvt_perf_type_t perf_type_{ESP_AE_RATE_CVT_PERF_TYPE_SPEED};
  esp_ae_rate_cvt_handle_t handle_{nullptr};
};
}  // namespace

#if defined(USE_ESP_AUDIO_STACK_MONO_RX) || defined(USE_ESP_AUDIO_STACK_MONO_REF)
class AudioEffectsRateConverterImpl {
 public:
  ~AudioEffectsRateConverterImpl() {
    if (this->scratch_ != nullptr)
      heap_caps_free(this->scratch_);
  }

  void init(uint32_t ratio, uint32_t src_rate, uint32_t dest_rate, uint8_t complexity, uint8_t perf_type) {
    this->ratio_ = ratio;
    this->src_rate_ = src_rate;
    this->dest_rate_ = dest_rate;
    this->rate_cvt_.init(ratio, src_rate, dest_rate, 1, complexity, perf_type);
    this->bit_cvt_.init(src_rate, 1, 32, 16);
  }

  void reset() { this->rate_cvt_.reset(); }

  bool prepare(size_t in_count, bool source_32bit) {
    if (source_32bit) {
      this->bit_cvt_.init(this->src_rate_, 1, 32, 16);
      if (!this->bit_cvt_.ready())
        return false;
    }
    if (this->ratio_ <= 1)
      return true;
    return ensure_buffer(this->scratch_, this->scratch_cap_, in_count, "RateCvt") && this->rate_cvt_.ready();
  }

  bool process(const int16_t *in, int16_t *out, size_t in_count) {
    if (this->ratio_ <= 1) {
      memcpy(out, in, in_count * sizeof(int16_t));
      return true;
    }
    const size_t out_count = in_count / this->ratio_;
    return this->rate_cvt_.process(const_cast<int16_t *>(in), in_count, out, out_count, "mono");
  }

  bool process_strided(const int16_t *in, int16_t *out, size_t out_count, size_t stride, size_t offset) {
    if (stride != 1 || offset != 0) {
      ESP_LOGE(TAG, "mono strided 16-bit conversion now requires Espressif data-weaver path; got stride=%u offset=%u",
               static_cast<unsigned>(stride), static_cast<unsigned>(offset));
      return false;
    }
    return this->process(in, out, out_count * this->ratio_);
  }

  bool process_strided_32(const int32_t *in, int16_t *out, size_t out_count, size_t stride, size_t offset) {
    if (stride != 1 || offset != 0) {
      ESP_LOGE(TAG, "mono strided 32-bit conversion now requires Espressif data-weaver path; got stride=%u offset=%u",
               static_cast<unsigned>(stride), static_cast<unsigned>(offset));
      return false;
    }
    const size_t in_count = out_count * this->ratio_;
    this->bit_cvt_.init(this->src_rate_, 1, 32, 16);
    if (this->ratio_ <= 1) {
      return this->bit_cvt_.process(in, static_cast<uint32_t>(in_count), out, "mono32");
    }
    if (!ensure_buffer(this->scratch_, this->scratch_cap_, in_count, "RateCvt"))
      return false;
    if (!this->bit_cvt_.process(in, static_cast<uint32_t>(in_count), this->scratch_, "mono32"))
      return false;
    return this->process(this->scratch_, out, in_count);
  }

 private:
  uint32_t ratio_{1};
  uint32_t src_rate_{0};
  uint32_t dest_rate_{0};
  RateCvtHandle rate_cvt_;
  BitCvtHandle bit_cvt_;
  int16_t *scratch_{nullptr};
  size_t scratch_cap_{0};
};
#endif

#ifdef USE_ESP_AUDIO_STACK_MULTI_RX
class MultiChannelAudioEffectsRateConverterImpl {
 public:
  ~MultiChannelAudioEffectsRateConverterImpl() {
    for (uint8_t i = 0; i < MAX_RATE_CVT_CHANNELS; i++) {
      if (this->out_ch_[i] != nullptr)
        heap_caps_free(this->out_ch_[i]);
    }
    for (uint8_t i = 0; i < MAX_DEINTLV_CH; i++) {
      if (this->deintlv_ch_[i] != nullptr)
        heap_caps_free(this->deintlv_ch_[i]);
    }
    if (this->bit_scratch_ != nullptr)
      heap_caps_free(this->bit_scratch_);
  }

  void init(uint32_t ratio, uint8_t num_channels, uint32_t src_rate, uint32_t dest_rate, uint8_t complexity,
            uint8_t perf_type) {
    this->ratio_ = ratio;
    this->src_rate_ = src_rate;
    this->dest_rate_ = dest_rate;
    this->channels_ = std::min<uint8_t>(num_channels, MAX_RATE_CVT_CHANNELS);
    this->rate_cvt_.init(ratio, src_rate, dest_rate, this->channels_, complexity, perf_type);
  }

  void reset() { this->rate_cvt_.reset(); }

  bool prepare(size_t in_count, size_t out_count, uint8_t num_channels, uint8_t source_channels, bool source_32bit) {
    const uint8_t nch = std::min<uint8_t>(num_channels, MAX_RATE_CVT_CHANNELS);
    const uint8_t deintlv_channels = source_32bit ? source_channels : nch;
    if (!this->ensure_deintlv_buffers_(deintlv_channels, in_count))
      return false;
    if (source_32bit && !this->ensure_bit_conversion_(source_channels, in_count))
      return false;
    if (this->ratio_ <= 1)
      return true;
    return this->ensure_output_buffers_(nch, out_count) && this->rate_cvt_.ready();
  }

  bool process_multi(const int16_t *in, size_t out_count, size_t stride, const uint8_t *offsets,
                     int16_t *mic_interleaved, int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch) {
    return this->process_multi_t_(in, out_count, stride, offsets, mic_interleaved, mic_mono, ref_out, num_mic_ch,
                                  false);
  }

  bool process_multi_32(const int32_t *in, size_t out_count, size_t stride, const uint8_t *offsets,
                        int16_t *mic_interleaved, int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch) {
    return this->process_multi_t_(in, out_count, stride, offsets, mic_interleaved, mic_mono, ref_out, num_mic_ch, true);
  }

 private:
  template<typename T>
  bool process_multi_t_(const T *in, size_t out_count, size_t stride, const uint8_t *offsets, int16_t *mic_interleaved,
                        int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch, bool source_32bit) {
    const size_t in_count = out_count * this->ratio_;
    if (!this->deinterleave_selected_(in, in_count, stride, offsets, source_32bit))
      return false;

    int16_t *selected[MAX_RATE_CVT_CHANNELS]{};
    for (uint8_t c = 0; c < this->channels_; c++) {
      selected[c] = source_32bit ? this->deintlv_ch_[offsets[c]] : this->deintlv_ch_[c];
    }

    if (this->ratio_ <= 1) {
      return distribute_channels(selected, this->channels_, out_count, mic_interleaved, mic_mono, ref_out, num_mic_ch);
    }

    if (!this->ensure_output_buffers_(this->channels_, out_count))
      return false;
    if (!this->rate_cvt_.process_deintlv(selected, in_count, this->out_ch_, out_count, "multi"))
      return false;
    return distribute_channels(this->out_ch_, this->channels_, out_count, mic_interleaved, mic_mono, ref_out,
                               num_mic_ch);
  }

  template<typename T>
  bool deinterleave_selected_(const T *in, size_t in_count, size_t stride, const uint8_t *offsets, bool source_32bit) {
    if (stride == 0 || stride > MAX_DEINTLV_CH) {
      ESP_LOGE(TAG, "unsupported source channel count for esp_ae_deintlv: %u", static_cast<unsigned>(stride));
      return false;
    }
    for (uint8_t c = 0; c < this->channels_; c++) {
      if (offsets[c] >= stride) {
        ESP_LOGE(TAG, "channel offset %u outside source channel count %u", static_cast<unsigned>(offsets[c]),
                 static_cast<unsigned>(stride));
        return false;
      }
    }
    const void *deintlv_input = in;
    if (source_32bit) {
      if (!this->ensure_deintlv_buffers_(static_cast<uint8_t>(stride), in_count))
        return false;
      if (!this->ensure_bit_conversion_(static_cast<uint8_t>(stride), in_count))
        return false;
      if (!this->bit_cvt_.process(in, static_cast<uint32_t>(in_count), this->bit_scratch_, "multi32"))
        return false;
      deintlv_input = this->bit_scratch_;
    } else {
      return this->extract_selected_channels_(static_cast<const int16_t *>(deintlv_input), in_count, stride, offsets);
    }

    esp_ae_sample_t out_args[MAX_DEINTLV_CH]{};
    for (uint8_t c = 0; c < stride; c++) {
      out_args[c] = this->deintlv_ch_[c];
    }
    const esp_ae_err_t err = esp_ae_deintlv_process(static_cast<uint8_t>(stride), 16, static_cast<uint32_t>(in_count),
                                                    const_cast<void *>(deintlv_input), out_args);
    if (err == ESP_AE_ERR_OK)
      return true;
    ESP_LOGE(TAG, "esp_ae_deintlv_process failed: err=%d samples=%u ch=%u", static_cast<int>(err),
             static_cast<unsigned>(in_count), static_cast<unsigned>(stride));
    return false;
  }

  bool extract_selected_channels_(const int16_t *in, size_t in_count, size_t stride, const uint8_t *offsets) {
    if (!this->ensure_deintlv_buffers_(this->channels_, in_count))
      return false;
    for (uint8_t c = 0; c < this->channels_; c++) {
      int16_t *out = this->deintlv_ch_[c];
      const size_t offset = offsets[c];
      for (size_t i = 0; i < in_count; i++) {
        out[i] = in[i * stride + offset];
      }
    }
    return true;
  }

  bool ensure_bit_conversion_(uint8_t source_channels, size_t in_count) {
    const size_t sample_slots = in_count * source_channels;
    if (!ensure_buffer(this->bit_scratch_, this->bit_scratch_cap_, sample_slots, "AEBitCvt"))
      return false;
    this->bit_cvt_.init(this->src_rate_, source_channels, 32, 16);
    return this->bit_cvt_.ready();
  }

  bool ensure_deintlv_buffers_(uint8_t source_channels, size_t in_count) {
    if (source_channels == 0 || source_channels > MAX_DEINTLV_CH)
      return false;
    for (uint8_t c = 0; c < source_channels; c++) {
      if (!ensure_buffer(this->deintlv_ch_[c], this->deintlv_cap_[c], in_count, "AEDeintlv"))
        return false;
    }
    return true;
  }

  bool ensure_output_buffers_(uint8_t channels, size_t out_count) {
    for (uint8_t c = 0; c < channels; c++) {
      if (!ensure_buffer(this->out_ch_[c], this->out_cap_[c], out_count, "MCRateCvt"))
        return false;
    }
    return true;
  }

  uint32_t ratio_{1};
  uint32_t src_rate_{0};
  uint32_t dest_rate_{0};
  uint8_t channels_{0};
  RateCvtHandle rate_cvt_;
  BitCvtHandle bit_cvt_;
  int16_t *deintlv_ch_[MAX_DEINTLV_CH]{};
  size_t deintlv_cap_[MAX_DEINTLV_CH]{};
  int16_t *out_ch_[MAX_RATE_CVT_CHANNELS]{};
  size_t out_cap_[MAX_RATE_CVT_CHANNELS]{};
  int16_t *bit_scratch_{nullptr};
  size_t bit_scratch_cap_{0};
};
#endif

#if defined(USE_ESP_AUDIO_STACK_MONO_RX) || defined(USE_ESP_AUDIO_STACK_MONO_REF)
AudioEffectsRateConverter::AudioEffectsRateConverter() : impl_(std::make_unique<AudioEffectsRateConverterImpl>()) {}
AudioEffectsRateConverter::~AudioEffectsRateConverter() = default;
void AudioEffectsRateConverter::init(uint32_t ratio, uint32_t src_rate, uint32_t dest_rate, uint8_t complexity,
                                     uint8_t perf_type) {
  this->impl_->init(ratio, src_rate, dest_rate, complexity, perf_type);
}
void AudioEffectsRateConverter::reset() { this->impl_->reset(); }
bool AudioEffectsRateConverter::prepare(size_t in_count, bool source_32bit) {
  return this->impl_->prepare(in_count, source_32bit);
}
bool AudioEffectsRateConverter::process(const int16_t *in, int16_t *out, size_t in_count) {
  return this->impl_->process(in, out, in_count);
}
bool AudioEffectsRateConverter::process_strided(const int16_t *in, int16_t *out, size_t out_count, size_t stride,
                                                size_t offset) {
  return this->impl_->process_strided(in, out, out_count, stride, offset);
}
bool AudioEffectsRateConverter::process_strided_32(const int32_t *in, int16_t *out, size_t out_count, size_t stride,
                                                   size_t offset) {
  return this->impl_->process_strided_32(in, out, out_count, stride, offset);
}
#endif

#ifdef USE_ESP_AUDIO_STACK_MULTI_RX
MultiChannelAudioEffectsRateConverter::MultiChannelAudioEffectsRateConverter()
    : impl_(std::make_unique<MultiChannelAudioEffectsRateConverterImpl>()) {}
MultiChannelAudioEffectsRateConverter::~MultiChannelAudioEffectsRateConverter() = default;
void MultiChannelAudioEffectsRateConverter::init(uint32_t ratio, uint8_t num_channels, uint32_t src_rate,
                                                 uint32_t dest_rate, uint8_t complexity, uint8_t perf_type) {
  this->impl_->init(ratio, num_channels, src_rate, dest_rate, complexity, perf_type);
}
void MultiChannelAudioEffectsRateConverter::reset() { this->impl_->reset(); }
bool MultiChannelAudioEffectsRateConverter::prepare(size_t in_count, size_t out_count, uint8_t num_channels,
                                                    uint8_t source_channels, bool source_32bit) {
  return this->impl_->prepare(in_count, out_count, num_channels, source_channels, source_32bit);
}
bool MultiChannelAudioEffectsRateConverter::process_multi(const int16_t *in, size_t out_count, size_t in_stride,
                                                          const uint8_t *channel_offsets, int16_t *mic_interleaved,
                                                          int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch) {
  return this->impl_->process_multi(in, out_count, in_stride, channel_offsets, mic_interleaved, mic_mono, ref_out,
                                    num_mic_ch);
}
bool MultiChannelAudioEffectsRateConverter::process_multi_32(const int32_t *in, size_t out_count, size_t in_stride,
                                                             const uint8_t *channel_offsets, int16_t *mic_interleaved,
                                                             int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch) {
  return this->impl_->process_multi_32(in, out_count, in_stride, channel_offsets, mic_interleaved, mic_mono, ref_out,
                                       num_mic_ch);
}
#endif

#endif  // esp-audio-libs headers available

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
