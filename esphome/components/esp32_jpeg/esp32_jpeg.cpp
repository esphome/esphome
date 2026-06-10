#include "esp32_jpeg.h"

#ifdef USE_ESP32_JPEG

#include <algorithm>
#include <cstring>
#include <utility>

#include "esp_heap_caps.h"
#include "esp_idf_version.h"

#if defined(SOC_JPEG_CODEC_SUPPORTED) && SOC_JPEG_CODEC_SUPPORTED
#include "driver/jpeg_decode.h"
#include "driver/jpeg_encode.h"
#endif

namespace esphome::esp32_jpeg {
namespace {

uint32_t align_up(uint32_t value, uint32_t alignment) { return (value + alignment - 1) / alignment * alignment; }

#if defined(SOC_JPEG_CODEC_SUPPORTED) && SOC_JPEG_CODEC_SUPPORTED
jpeg_enc_input_format_t to_encode_format(PixelFormat format) {
  switch (format) {
    case PixelFormat::RGB565:
      return JPEG_ENCODE_IN_FORMAT_RGB565;
    case PixelFormat::RGB888:
      return JPEG_ENCODE_IN_FORMAT_RGB888;
    case PixelFormat::GRAY:
      return JPEG_ENCODE_IN_FORMAT_GRAY;
  }
  return JPEG_ENCODE_IN_FORMAT_RGB888;
}

jpeg_dec_output_format_t to_decode_format(PixelFormat format) {
  switch (format) {
    case PixelFormat::RGB565:
      return JPEG_DECODE_OUT_FORMAT_RGB565;
    case PixelFormat::RGB888:
      return JPEG_DECODE_OUT_FORMAT_RGB888;
    case PixelFormat::GRAY:
      return JPEG_DECODE_OUT_FORMAT_GRAY;
  }
  return JPEG_DECODE_OUT_FORMAT_RGB888;
}

jpeg_down_sampling_type_t to_down_sampling(DownSampling down_sampling) {
  switch (down_sampling) {
    case DownSampling::YUV444:
      return JPEG_DOWN_SAMPLING_YUV444;
    case DownSampling::YUV422:
      return JPEG_DOWN_SAMPLING_YUV422;
    case DownSampling::YUV420:
      return JPEG_DOWN_SAMPLING_YUV420;
    case DownSampling::GRAY:
      return JPEG_DOWN_SAMPLING_GRAY;
  }
  return JPEG_DOWN_SAMPLING_YUV420;
}

DownSampling from_down_sampling(jpeg_down_sampling_type_t down_sampling) {
  switch (down_sampling) {
    case JPEG_DOWN_SAMPLING_YUV444:
      return DownSampling::YUV444;
    case JPEG_DOWN_SAMPLING_YUV422:
      return DownSampling::YUV422;
    case JPEG_DOWN_SAMPLING_YUV420:
      return DownSampling::YUV420;
    case JPEG_DOWN_SAMPLING_GRAY:
      return DownSampling::GRAY;
  }
  return DownSampling::YUV420;
}

jpeg_dec_rgb_element_order_t to_rgb_order(RgbElementOrder order) {
  return order == RgbElementOrder::RGB ? JPEG_DEC_RGB_ELEMENT_ORDER_RGB : JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
}

jpeg_yuv_rgb_conv_std_t to_color_standard(ColorConversionStandard standard) {
  return standard == ColorConversionStandard::BT709 ? JPEG_YUV_RGB_CONV_STD_BT709 : JPEG_YUV_RGB_CONV_STD_BT601;
}
#endif

}  // namespace

JpegBuffer::~JpegBuffer() { this->release(); }

JpegBuffer::JpegBuffer(JpegBuffer &&other) noexcept {
  this->data_ = std::exchange(other.data_, nullptr);
  this->size_ = std::exchange(other.size_, 0);
  this->capacity_ = std::exchange(other.capacity_, 0);
}

JpegBuffer &JpegBuffer::operator=(JpegBuffer &&other) noexcept {
  if (this != &other) {
    this->release();
    this->data_ = std::exchange(other.data_, nullptr);
    this->size_ = std::exchange(other.size_, 0);
    this->capacity_ = std::exchange(other.capacity_, 0);
  }
  return *this;
}

void JpegBuffer::reset(uint8_t *data, size_t size, size_t capacity) {
  this->release();
  this->data_ = data;
  this->size_ = size;
  this->capacity_ = capacity;
}

void JpegBuffer::release() {
  if (this->data_ != nullptr) {
    heap_caps_free(this->data_);
    this->data_ = nullptr;
  }
  this->size_ = 0;
  this->capacity_ = 0;
}

size_t bytes_per_pixel(PixelFormat format) {
  switch (format) {
    case PixelFormat::RGB565:
      return 2;
    case PixelFormat::RGB888:
      return 3;
    case PixelFormat::GRAY:
      return 1;
  }
  return 0;
}

size_t raw_image_size(uint32_t width, uint32_t height, PixelFormat format) {
  return static_cast<size_t>(width) * height * bytes_per_pixel(format);
}

size_t decoded_output_size(const PictureInfo &info, PixelFormat format) {
  return raw_image_size(align_up(info.width, 16), align_up(info.height, 16), format);
}

esp_err_t get_info(const uint8_t *jpeg, size_t jpeg_size, PictureInfo *info) {
#if defined(SOC_JPEG_CODEC_SUPPORTED) && SOC_JPEG_CODEC_SUPPORTED
  if (jpeg == nullptr || jpeg_size == 0 || info == nullptr)
    return ESP_ERR_INVALID_ARG;

  jpeg_decode_picture_info_t picture_info = {};
  esp_err_t err = jpeg_decoder_get_info(jpeg, jpeg_size, &picture_info);
  if (err != ESP_OK)
    return err;

  info->width = picture_info.width;
  info->height = picture_info.height;
  info->down_sampling = from_down_sampling(picture_info.sample_method);
  return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t encode(const EncodeConfig &config, const uint8_t *input, size_t input_size, JpegBuffer *output) {
#if defined(SOC_JPEG_CODEC_SUPPORTED) && SOC_JPEG_CODEC_SUPPORTED
  if (input == nullptr || output == nullptr || config.width == 0 || config.height == 0)
    return ESP_ERR_INVALID_ARG;

  const size_t expected_input_size = raw_image_size(config.width, config.height, config.input_format);
  if (input_size < expected_input_size)
    return ESP_ERR_INVALID_SIZE;

  output->release();

  jpeg_encoder_handle_t encoder = nullptr;
  jpeg_encode_engine_cfg_t engine_cfg = {
      .intr_priority = 0,
      .timeout_ms = config.timeout_ms,
  };
  esp_err_t err = jpeg_new_encoder_engine(&engine_cfg, &encoder);
  if (err != ESP_OK)
    return err;

  size_t output_capacity = 0;
  jpeg_encode_memory_alloc_cfg_t mem_cfg = {
      .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER,
  };
  uint8_t *output_data =
      static_cast<uint8_t *>(jpeg_alloc_encoder_mem(expected_input_size, &mem_cfg, &output_capacity));
  if (output_data == nullptr) {
    jpeg_del_encoder_engine(encoder);
    return ESP_ERR_NO_MEM;
  }

  uint8_t quality = std::min<uint8_t>(std::max<uint8_t>(config.quality, 1), 100);
  jpeg_encode_cfg_t encode_cfg = {
    .height = config.height,
    .width = config.width,
    .src_type = to_encode_format(config.input_format),
    .sub_sample = to_down_sampling(config.down_sampling),
    .image_quality = quality,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    .pixel_reverse = config.pixel_reverse,
#endif
  };
  uint32_t encoded_size = 0;
  err = jpeg_encoder_process(encoder, &encode_cfg, input, expected_input_size, output_data, output_capacity,
                             &encoded_size);
  jpeg_del_encoder_engine(encoder);
  if (err != ESP_OK || encoded_size == 0) {
    heap_caps_free(output_data);
    return err == ESP_OK ? ESP_FAIL : err;
  }

  uint8_t *stored_data = static_cast<uint8_t *>(heap_caps_malloc(encoded_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (stored_data == nullptr)
    stored_data = static_cast<uint8_t *>(heap_caps_malloc(encoded_size, MALLOC_CAP_8BIT));
  if (stored_data == nullptr) {
    heap_caps_free(output_data);
    return ESP_ERR_NO_MEM;
  }
  std::memcpy(stored_data, output_data, encoded_size);
  heap_caps_free(output_data);
  output->reset(stored_data, encoded_size, encoded_size);
  return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t decode(const DecodeConfig &config, const uint8_t *jpeg, size_t jpeg_size, uint8_t *output, size_t output_size,
                 size_t *written) {
#if defined(SOC_JPEG_CODEC_SUPPORTED) && SOC_JPEG_CODEC_SUPPORTED
  if (jpeg == nullptr || jpeg_size == 0 || output == nullptr || output_size == 0)
    return ESP_ERR_INVALID_ARG;

  if (written != nullptr)
    *written = 0;

  jpeg_decoder_handle_t decoder = nullptr;
  jpeg_decode_engine_cfg_t engine_cfg = {
      .intr_priority = 0,
      .timeout_ms = config.timeout_ms,
  };
  esp_err_t err = jpeg_new_decoder_engine(&engine_cfg, &decoder);
  if (err != ESP_OK)
    return err;

  jpeg_decode_cfg_t decode_cfg = {
      .output_format = to_decode_format(config.output_format),
      .rgb_order = to_rgb_order(config.rgb_order),
      .conv_std = to_color_standard(config.color_conversion),
  };
  uint32_t decoded_size = 0;
  err = jpeg_decoder_process(decoder, &decode_cfg, jpeg, jpeg_size, output, output_size, &decoded_size);
  jpeg_del_decoder_engine(decoder);
  if (err != ESP_OK)
    return err;

  if (written != nullptr)
    *written = decoded_size;
  return ESP_OK;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

}  // namespace esphome::esp32_jpeg

#endif  // USE_ESP32_JPEG
