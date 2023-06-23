#include "image.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_JPEGDEC
#include "JPEGDEC.h"
#endif  // USE_JPEGDEC

namespace esphome {
namespace image {

static const char *TAG = "image";

void Image::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  switch (type_) {
    case IMAGE_TYPE_BINARY: {
      if (display->draw_pixels_at(
        x, y,
        width_, height_,
        data_start_,
        display::PixelW1::bytes_stride(width_),
        this->transparent_ ? display::PixelFormat::A1 : display::PixelFormat::W1,
        color_on, color_off)) {
        return;
      }

      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          if (this->get_binary_pixel_(img_x, img_y)) {
            display->draw_pixel_at(x + img_x, y + img_y, color_on);
          } else if (!this->transparent_) {
            display->draw_pixel_at(x + img_x, y + img_y, color_off);
          }
        }
      }
      break;
    }
    case IMAGE_TYPE_GRAYSCALE:
      if (display->draw_pixels_at(
        x, y,
        width_, height_,
        data_start_,
        display::PixelW8::bytes_stride(width_),
        this->transparent_ ? display::PixelFormat::W8_KEY : display::PixelFormat::W8)) {
        return;
      }
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_grayscale_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGB565:
      if (display->draw_pixels_at(
        x, y,
        width_, height_,
        data_start_,
        display::PixelRGB565_BE::bytes_stride(width_),
        display::PixelFormat::RGB565_BE)) {
        return;
      }
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgb565_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGB24:
      if (display->draw_pixels_at(
        x, y,
        width_, height_,
        data_start_,
        display::PixelRGB888::bytes_stride(width_),
        display::PixelFormat::RGB888)) {
        return;
      }
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgb24_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;
    case IMAGE_TYPE_RGBA:
      if (display->draw_pixels_at(
        x, y,
        width_, height_,
        data_start_,
        display::PixelRGBA8888::bytes_stride(width_),
        display::PixelFormat::RGBA8888)) {
        return;
      }
      for (int img_x = 0; img_x < width_; img_x++) {
        for (int img_y = 0; img_y < height_; img_y++) {
          auto color = this->get_rgba_pixel_(img_x, img_y);
          if (color.w >= 0x80) {
            display->draw_pixel_at(x + img_x, y + img_y, color);
          }
        }
      }
      break;

    case IMAGE_TYPE_JPEG:
      this->draw_jpeg(x, y, display);
      break;
  }
}
Color Image::get_pixel(int x, int y, Color color_on, Color color_off) const {
  if (x < 0 || x >= this->width_ || y < 0 || y >= this->height_)
    return color_off;
  switch (this->type_) {
    case IMAGE_TYPE_BINARY:
      return this->get_binary_pixel_(x, y) ? color_on : color_off;
    case IMAGE_TYPE_GRAYSCALE:
      return this->get_grayscale_pixel_(x, y);
    case IMAGE_TYPE_RGB565:
      return this->get_rgb565_pixel_(x, y);
    case IMAGE_TYPE_RGB24:
      return this->get_rgb24_pixel_(x, y);
    case IMAGE_TYPE_RGBA:
      return this->get_rgba_pixel_(x, y);
    default:
      return color_off;
  }
}
bool Image::get_binary_pixel_(int x, int y) const {
  const uint32_t width_8 = ((this->width_ + 7u) / 8u) * 8u;
  const uint32_t pos = x + y * width_8;
  return progmem_read_byte(this->data_start_ + (pos / 8u)) & (0x80 >> (pos % 8u));
}
Color Image::get_rgba_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_) * 4;
  return Color(progmem_read_byte(this->data_start_ + pos + 0), progmem_read_byte(this->data_start_ + pos + 1),
               progmem_read_byte(this->data_start_ + pos + 2), progmem_read_byte(this->data_start_ + pos + 3));
}
Color Image::get_rgb24_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_) * 3;
  Color color = Color(progmem_read_byte(this->data_start_ + pos + 0), progmem_read_byte(this->data_start_ + pos + 1),
                      progmem_read_byte(this->data_start_ + pos + 2));
  if (color.b == 1 && color.r == 0 && color.g == 0 && transparent_) {
    // (0, 0, 1) has been defined as transparent color for non-alpha images.
    // putting blue == 1 as a first condition for performance reasons (least likely value to short-cut the if)
    color.w = 0;
  } else {
    color.w = 0xFF;
  }
  return color;
}
Color Image::get_rgb565_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_) * 2;
  uint16_t rgb565 =
      progmem_read_byte(this->data_start_ + pos + 0) << 8 | progmem_read_byte(this->data_start_ + pos + 1);
  auto r = (rgb565 & 0xF800) >> 11;
  auto g = (rgb565 & 0x07E0) >> 5;
  auto b = rgb565 & 0x001F;
  Color color = Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
  if (rgb565 == 0x0020 && transparent_) {
    // darkest green has been defined as transparent color for transparent RGB565 images.
    color.w = 0;
  } else {
    color.w = 0xFF;
  }
  return color;
}
Color Image::get_grayscale_pixel_(int x, int y) const {
  const uint32_t pos = (x + y * this->width_);
  const uint8_t gray = progmem_read_byte(this->data_start_ + pos);
  uint8_t alpha = (gray == 1 && transparent_) ? 0 : 0xFF;
  return Color(gray, gray, gray, alpha);
}

#ifdef USE_JPEGDEC
struct JpegData {
  display::Display *display{nullptr};
  int jpeg_format{-1};
  display::PixelFormat pixel_format{display::PixelFormat::Unknown};
  bool transparent{false};
};

static int jpeg_draw(JPEGDRAW *pDraw) {
  JpegData *data = (JpegData*)pDraw->pUser;
  display::Display *display = data->display;

  if (display->draw_pixels_at(
    pDraw->x, pDraw->y,
    pDraw->iWidth, pDraw->iHeight,
    (const uint8_t*)pDraw->pPixels,
    pDraw->iBpp * pDraw->iWidth / 8,
    data->pixel_format
  )) {
    return 1;
  }

  if (data->pixel_format != display::PixelFormat::RGB565_BE) {
    return 0;
  }

  const uint16_t *pixelData = pDraw->pPixels;
  for (int y = 0; y < pDraw->iHeight; y++) {
    for (int x = 0; x < pDraw->iWidth; x++, pixelData++) {
      // TODO: this is not very fast.
      auto r = (*pixelData & 0xF800) >> 11;
      auto g = (*pixelData & 0x07E0) >> 5;
      auto b = *pixelData & 0x001F;
      Color color = Color((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
      if (*pixelData == 0x0020 && data->transparent) {
        // darkest green has been defined as transparent color for transparent RGB565 images.
        color.w = 0;
      } else {
        color.w = 0xFF;
      }
      display->draw_pixel_at(pDraw->x + x, pDraw->y + y, color);
    }
  }
  return 1;
}
#endif  // USE_JPEGDEC

void Image::draw_jpeg(int x, int y, display::Display *display) {
#ifdef USE_JPEGDEC
  JPEGDEC* jpeg = new JPEGDEC();
  JpegData data;
  int nativeFormat = -1;

  data.display = display;
  data.pixel_format = display->get_native_pixel_format();
  data.transparent = this->transparent_;

  switch (data.pixel_format) {
    case display::PixelFormat::RGB565_LE:
      data.jpeg_format = RGB565_LITTLE_ENDIAN;
      break;

    case display::PixelFormat::RGB565_BE:
      data.jpeg_format = RGB565_BIG_ENDIAN;
      break;

    case display::PixelFormat::W8:
      data.jpeg_format = EIGHT_BIT_GRAYSCALE;
      break;

      // Pixel Format is not supported, fallback to RGB565_BE
    default:
      data.jpeg_format = RGB565_LITTLE_ENDIAN;
      data.pixel_format = display::PixelFormat::RGB565_LE;
      break;
  }

  if (jpeg->openFLASH((uint8_t*) this->data_start_, this->data_size_, jpeg_draw)) {
    jpeg->setUserPointer(&data);
    jpeg->setPixelType(data.jpeg_format);
    if (jpeg->decode(x, y, 0)) {
      ESP_LOGV("jpeg", "Decode succeeded");
    }
    jpeg->close();
  }
  delete jpeg;
#else   // USE_JPEGDEC
  ESP_LOGE(TAG, "JPEGDEC is not compiled in.");
#endif  // USE_JPEGDEC
}
int Image::get_width() const { return this->width_; }
int Image::get_height() const { return this->height_; }
ImageType Image::get_type() const { return this->type_; }
Image::Image(const uint8_t *data_start, int data_size, int width, int height, ImageType type)
    : data_size_(data_size), width_(width), height_(height), type_(type), data_start_(data_start) {}

}  // namespace image
}  // namespace esphome
