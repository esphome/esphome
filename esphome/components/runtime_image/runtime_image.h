#pragma once

#include "esphome/components/image/image.h"
#include "esphome/core/helpers.h"

namespace esphome::runtime_image {

// Forward declaration
class ImageDecoder;

/**
 * @brief Image format types that can be decoded dynamically.
 */
enum ImageFormat {
  /** Automatically detect from data. Not implemented yet. */
  AUTO,
  /** JPEG format. */
  JPEG,
  /** PNG format. */
  PNG,
  /** BMP format. */
  BMP,
};

/**
 * @brief A dynamic image that can be loaded and decoded at runtime.
 *
 * This class provides dynamic buffer allocation and management for images
 * that are decoded at runtime, as opposed to static images compiled into
 * the firmware. It serves as a base class for components that need to
 * load images dynamically from various sources.
 */
class RuntimeImage : public image::Image {
 public:
  /**
   * @brief Construct a new RuntimeImage object.
   *
   * @param format The image format to decode.
   * @param type The pixel format for the image.
   * @param transparency The transparency type for the image.
   * @param placeholder Optional placeholder image to show while loading.
   * @param is_big_endian Whether the image is stored in big-endian format.
   * @param fixed_width Fixed width for the image (0 for auto-resize).
   * @param fixed_height Fixed height for the image (0 for auto-resize).
   */
  RuntimeImage(ImageFormat format, image::ImageType type, image::Transparency transparency,
               image::Image *placeholder = nullptr, bool is_big_endian = false, int fixed_width = 0,
               int fixed_height = 0);

  ~RuntimeImage();

  // Decoder interface methods
  /**
   * @brief Resize the image buffer to the requested dimensions.
   *
   * The buffer will be allocated if not existing.
   * If fixed dimensions have been specified in the constructor, the buffer will be created
   * with those dimensions and not resized, even on request.
   * Otherwise, the old buffer will be deallocated and a new buffer with the requested
   * dimensions allocated.
   *
   * @param width Requested width (ignored if fixed_width_ is set)
   * @param height Requested height (ignored if fixed_height_ is set)
   * @return Size of the allocated buffer in bytes, or 0 if allocation failed.
   */
  int resize(int width, int height);
  void draw_pixel(int x, int y, const Color &color);
  void map_chroma_key(Color &color);
  int get_buffer_width() const { return this->buffer_width_; }
  int get_buffer_height() const { return this->buffer_height_; }

  // Image drawing interface
  void draw(int x, int y, display::Display *display, Color color_on, Color color_off) override;

  /**
   * @brief Begin decoding an image.
   *
   * @param expected_size Optional hint about the expected data size.
   * @return true if decoder was successfully initialized.
   */
  bool begin_decode(size_t expected_size = 0);

  /**
   * @brief Feed data to the decoder.
   *
   * @param data Pointer to the data buffer.
   * @param len Length of data to process.
   * @return Number of bytes consumed by the decoder.
   */
  int feed_data(uint8_t *data, size_t len);

  /**
   * @brief Complete the decoding process.
   *
   * @return true if decoding completed successfully.
   */
  bool end_decode();

  /**
   * @brief Check if decoding is currently in progress.
   */
  bool is_decoding() const { return this->decoder_ != nullptr; }

  /**
   * @brief Check if the decoder has finished processing all data.
   *
   * This delegates to the decoder's format-specific completion check,
   * which handles both known-size and chunked transfer cases.
   */
  bool is_decode_finished() const;

  /**
   * @brief Check if an image is currently loaded.
   */
  bool is_loaded() const { return this->buffer_ != nullptr; }

  /**
   * @brief Get the image format.
   */
  ImageFormat get_format() const { return this->format_; }

  /**
   * @brief Release the image buffer and free memory.
   */
  void release();

  /**
   * @brief Set whether to allow progressive display during decode.
   *
   * When enabled, the image can be displayed even while still decoding.
   * When disabled, the image is only displayed after decoding completes.
   */
  void set_progressive_display(bool progressive) { this->progressive_display_ = progressive; }

 protected:
  /**
   * @brief Resize the image buffer to the requested dimensions.
   *
   * @param width New width in pixels.
   * @param height New height in pixels.
   * @return Size of the allocated buffer, or 0 on failure.
   */
  size_t resize_buffer_(int width, int height);

  /**
   * @brief Release only the image buffer without resetting the decoder.
   *
   * This is safe to call from within the decoder (e.g., during resize).
   */
  void release_buffer_();

  /**
   * @brief Get the buffer size in bytes for given dimensions.
   */
  size_t get_buffer_size_(int width, int height) const;

  /**
   * @brief Get the position in the buffer for a pixel.
   */
  int get_position_(int x, int y) const;

  /**
   * @brief Create decoder instance for the image's format.
   */
  std::unique_ptr<ImageDecoder> create_decoder_();

  // Memory management
  uint8_t *buffer_{nullptr};

  // Decoder management
  std::unique_ptr<ImageDecoder> decoder_{nullptr};
  /** The image format this RuntimeImage is configured to decode. */
  const ImageFormat format_;

  /**
   * Actual width of the current image.
   * This needs to be separate from "Image::get_width()" because the latter
   * must return 0 until the image has been decoded (to avoid showing partially
   * decoded images). When progressive_display_ is enabled, Image dimensions
   * are updated during decoding to allow rendering in progress.
   */
  int buffer_width_{0};
  /**
   * Actual height of the current image.
   * This needs to be separate from "Image::get_height()" because the latter
   * must return 0 until the image has been decoded (to avoid showing partially
   * decoded images). When progressive_display_ is enabled, Image dimensions
   * are updated during decoding to allow rendering in progress.
   */
  int buffer_height_{0};

  // Decoding state
  size_t total_size_{0};
  size_t decoded_bytes_{0};

  /** Fixed width requested on configuration, or 0 if not specified. */
  const int fixed_width_{0};
  /** Fixed height requested on configuration, or 0 if not specified. */
  const int fixed_height_{0};

  /** Placeholder image to show when the runtime image is not available. */
  image::Image *placeholder_{nullptr};

  // Configuration
  bool progressive_display_{false};
  /**
   * Whether the image is stored in big-endian format.
   * This is used to determine how to store 16 bit colors in the buffer.
   */
  bool is_big_endian_{false};
};

}  // namespace esphome::runtime_image
