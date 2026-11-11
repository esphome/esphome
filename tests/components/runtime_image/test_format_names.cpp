#include <gtest/gtest.h>

#include "esphome/components/runtime_image/image_format.h"

namespace esphome::runtime_image::testing {

TEST(RuntimeImageFormatNames, FormatNames) {
  EXPECT_STREQ(LOG_STR_ARG(get_format_name(AUTO)), "AUTO");
  EXPECT_STREQ(LOG_STR_ARG(get_format_name(BMP)), "BMP");
  EXPECT_STREQ(LOG_STR_ARG(get_format_name(JPEG)), "JPEG");
  EXPECT_STREQ(LOG_STR_ARG(get_format_name(PNG)), "PNG");
  EXPECT_STREQ(LOG_STR_ARG(get_format_name(QOI)), "QOI");
}

TEST(RuntimeImageFormatNames, OutOfRangeName) {
  EXPECT_STREQ(LOG_STR_ARG(get_format_name(static_cast<ImageFormat>(254))), "UNKNOWN");
}

}  // namespace esphome::runtime_image::testing
