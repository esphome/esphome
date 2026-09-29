#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "image_format.h"

namespace esphome::runtime_image {

struct MimeLookup {
  const char *mime_type;
  ImageFormat format;
};

struct FormatLookup {
  ImageFormat format;
  const char *name;
};

// The first entry per format is its canonical MIME type; the rest are aliases
// seen from real servers (older IIS, CDNs, S3)
static constexpr MimeLookup MIME_LOOKUP_TABLE[] = {
#ifdef USE_RUNTIME_IMAGE_BMP
    {"image/bmp", ImageFormat::BMP},   {"image/x-ms-bmp", ImageFormat::BMP}, {"image/x-bmp", ImageFormat::BMP},
#endif
#ifdef USE_RUNTIME_IMAGE_JPEG
    {"image/jpeg", ImageFormat::JPEG}, {"image/jpg", ImageFormat::JPEG},
#endif
#ifdef USE_RUNTIME_IMAGE_PNG
    {"image/png", ImageFormat::PNG},   {"image/x-png", ImageFormat::PNG},
#endif
#ifdef USE_RUNTIME_IMAGE_QOI
    {"image/qoi", ImageFormat::QOI},   {"image/x-qoi", ImageFormat::QOI},
#endif
};

static constexpr FormatLookup FORMAT_LOOKUP_TABLE[] = {
    {ImageFormat::AUTO, "AUTO"},
#ifdef USE_RUNTIME_IMAGE_BMP
    {ImageFormat::BMP, "BMP"},
#endif
#ifdef USE_RUNTIME_IMAGE_JPEG
    {ImageFormat::JPEG, "JPEG"},
#endif
#ifdef USE_RUNTIME_IMAGE_PNG
    {ImageFormat::PNG, "PNG"},
#endif
#ifdef USE_RUNTIME_IMAGE_QOI
    {ImageFormat::QOI, "QOI"},
#endif
};

const char *get_mime_type_for_format(ImageFormat format) {
  for (const auto &entry : MIME_LOOKUP_TABLE) {
    if (entry.format == format) {
      return entry.mime_type;
    }
  }
  return "image/*";  // AUTO or compiled-out format
}

std::optional<ImageFormat> get_format_for_mime_type(const char *mime_type) {
  for (const auto &entry : MIME_LOOKUP_TABLE) {
    if (str_contains_ignore_case(mime_type, entry.mime_type)) {
      return entry.format;
    }
  }
  return std::nullopt;
}

const char *get_format_name(ImageFormat format) {
  for (const auto &entry : FORMAT_LOOKUP_TABLE) {
    if (entry.format == format) {
      return entry.name;
    }
  }
  return "unknown";
}

}  // namespace esphome::runtime_image
