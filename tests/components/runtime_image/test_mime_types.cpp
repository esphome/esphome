#include <gtest/gtest.h>

#include <optional>

#include "esphome/components/runtime_image/runtime_image.h"

namespace esphome::runtime_image::testing {

TEST(RuntimeImageMime, FormatForKnownMimeTypes) {
  EXPECT_EQ(get_format_for_mime_type("image/bmp"), BMP);
  EXPECT_EQ(get_format_for_mime_type("image/x-ms-bmp"), BMP);
  EXPECT_EQ(get_format_for_mime_type("image/x-bmp"), BMP);
#ifdef USE_RUNTIME_IMAGE_JPEG
  EXPECT_EQ(get_format_for_mime_type("image/jpeg"), JPEG);
  EXPECT_EQ(get_format_for_mime_type("image/jpg"), JPEG);
#endif  // USE_RUNTIME_IMAGE_JPEG
  EXPECT_EQ(get_format_for_mime_type("image/png"), PNG);
  EXPECT_EQ(get_format_for_mime_type("image/x-png"), PNG);
  EXPECT_EQ(get_format_for_mime_type("image/qoi"), QOI);
  EXPECT_EQ(get_format_for_mime_type("image/x-qoi"), QOI);
}

TEST(RuntimeImageMime, FormatMatchingIsCaseInsensitive) {
  EXPECT_EQ(get_format_for_mime_type("Image/PNG"), PNG);
  EXPECT_EQ(get_format_for_mime_type("IMAGE/BMP"), BMP);
}

TEST(RuntimeImageMime, FormatMatchesContentTypeWithParameters) {
  // Content-Type headers may carry parameters after the media type
  EXPECT_EQ(get_format_for_mime_type("image/png; charset=binary"), PNG);
  EXPECT_EQ(get_format_for_mime_type("image/bmp;name=\"a.bmp\""), BMP);
}

TEST(RuntimeImageMime, UnknownMimeTypeHasNoFormat) {
  EXPECT_EQ(get_format_for_mime_type("text/html"), std::nullopt);
  EXPECT_EQ(get_format_for_mime_type("application/octet-stream"), std::nullopt);
  EXPECT_EQ(get_format_for_mime_type("image/*"), std::nullopt);
  EXPECT_EQ(get_format_for_mime_type(""), std::nullopt);
  EXPECT_EQ(get_format_for_mime_type(nullptr), std::nullopt);
}

TEST(RuntimeImageMime, MimeTypeForFormatRoundTrip) {
  EXPECT_STREQ(get_mime_type_for_format(BMP), "image/bmp");
#ifdef USE_RUNTIME_IMAGE_JPEG
  EXPECT_STREQ(get_mime_type_for_format(JPEG), "image/jpeg");
#endif  // USE_RUNTIME_IMAGE_JPEG
  EXPECT_STREQ(get_mime_type_for_format(PNG), "image/png");
  EXPECT_STREQ(get_mime_type_for_format(QOI), "image/qoi");
  // AUTO has no single MIME type and falls back to the wildcard
  EXPECT_STREQ(get_mime_type_for_format(AUTO), "image/*");

  // Every decodable format must resolve back to itself through its MIME type
  for (ImageFormat format : {
           BMP,
#ifdef USE_RUNTIME_IMAGE_JPEG
           JPEG,
#endif  // USE_RUNTIME_IMAGE_JPEG
           PNG,
           QOI,
       }) {
    EXPECT_EQ(get_format_for_mime_type(get_mime_type_for_format(format)), format) << format;
  }
}

}  // namespace esphome::runtime_image::testing
