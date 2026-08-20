#include <gtest/gtest.h>

#include <cstring>
#include <optional>

#include "esphome/components/runtime_image/runtime_image.h"

namespace esphome::runtime_image::testing {

TEST(RuntimeImageMime, FormatForKnownMimeTypes) {
  EXPECT_EQ(get_format_for_mime_type("image/bmp"), BMP);
  EXPECT_EQ(get_format_for_mime_type("image/png"), PNG);
#ifdef USE_RUNTIME_IMAGE_JPEG
  EXPECT_EQ(get_format_for_mime_type("image/jpeg"), JPEG);
  EXPECT_EQ(get_format_for_mime_type("image/jpg"), JPEG);
#endif  // USE_RUNTIME_IMAGE_JPEG
  EXPECT_EQ(get_format_for_mime_type("image/*"), AUTO);
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
  EXPECT_EQ(get_format_for_mime_type(""), std::nullopt);
  EXPECT_EQ(get_format_for_mime_type(nullptr), std::nullopt);
}

TEST(RuntimeImageMime, MimeTypeForFormatRoundTrip) {
  EXPECT_STREQ(get_mime_type_for_format(BMP), "image/bmp");
  EXPECT_STREQ(get_mime_type_for_format(PNG), "image/png");
#ifdef USE_RUNTIME_IMAGE_JPEG
  EXPECT_STREQ(get_mime_type_for_format(JPEG), "image/jpeg");
#endif  // USE_RUNTIME_IMAGE_JPEG
  EXPECT_STREQ(get_mime_type_for_format(AUTO), "image/*");

  // Every table entry must resolve back to its own format.
  for (const auto &entry : MIME_LOOKUP_TABLE) {
    EXPECT_EQ(get_format_for_mime_type(entry.mime_type), entry.format) << entry.mime_type;
  }
}

TEST(RuntimeImageMime, MaxMimeTypeLengthCoversTable) {
  for (const auto &entry : MIME_LOOKUP_TABLE) {
    EXPECT_LE(strlen(entry.mime_type), MAX_MIME_TYPE_LENGTH) << entry.mime_type;
  }
}

}  // namespace esphome::runtime_image::testing
