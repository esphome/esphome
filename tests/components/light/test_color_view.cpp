#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/esp_color_view.h"
#include "esphome/components/light/esp_range_view.h"

namespace esphome::light::testing {
namespace {
constexpr ChannelColors WRGB{1, 2, 3, 0};
}  // namespace

TEST(ColorAccessors, NullColorAccessor) {
  auto &accessor = NullColorAccessor::instance;

  accessor.set(1, Color{10, 20, 30, 40});
  EXPECT_EQ((Color{}), accessor.get(1));
  EXPECT_EQ((Color{}), accessor.get_raw(1));

  accessor.set_raw(1, Color{15, 25, 35, 45});
  EXPECT_EQ((Color{}), accessor.get(1));
  EXPECT_EQ((Color{}), accessor.get_raw(1));

  accessor.set_red(1, 10);
  EXPECT_EQ(0, accessor.get_red(1));

  accessor.set_green(1, 20);
  EXPECT_EQ(0, accessor.get_green(1));

  accessor.set_blue(1, 30);
  EXPECT_EQ(0, accessor.get_blue(1));

  accessor.set_white(1, 40);
  EXPECT_EQ(0, accessor.get_white(1));

  accessor.set_effect_data(1, 50);
  EXPECT_EQ(0, accessor.get_effect_data(1));
}

TEST(ColorAccessors, RawBufferColorAccessor) {
  std::array<uint8_t, 12> color_data{};
  std::array<uint8_t, 3> effect_data{};
  RawBufferColorAccessor accessor(color_data.data(), effect_data.data(), WRGB);

  accessor.set(1, Color{10, 20, 30, 40});
  EXPECT_EQ((Color{10, 20, 30, 40}), accessor.get(1));
  EXPECT_EQ((Color{10, 20, 30, 40}), accessor.get_raw(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 40, 10, 20, 30, 0, 0, 0, 0}));

  accessor.set_raw(1, Color{15, 25, 35, 45});
  EXPECT_EQ((Color{15, 25, 35, 45}), accessor.get(1));
  EXPECT_EQ((Color{15, 25, 35, 45}), accessor.get_raw(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 15, 25, 35, 0, 0, 0, 0}));

  accessor.set_red(1, 10);
  EXPECT_EQ(10, accessor.get_red(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 10, 25, 35, 0, 0, 0, 0}));

  accessor.set_green(1, 20);
  EXPECT_EQ(20, accessor.get_green(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 10, 20, 35, 0, 0, 0, 0}));

  accessor.set_blue(1, 30);
  EXPECT_EQ(30, accessor.get_blue(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 10, 20, 30, 0, 0, 0, 0}));

  accessor.set_white(1, 40);
  EXPECT_EQ(40, accessor.get_white(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 40, 10, 20, 30, 0, 0, 0, 0}));

  accessor.set_effect_data(1, 50);
  EXPECT_EQ(50, accessor.get_effect_data(1));
  EXPECT_EQ(effect_data, (std::array<uint8_t, 3>{0, 50, 0}));
}

TEST(ColorAccessors, CorrectedBufferColorAccessor) {
  std::array<uint8_t, 12> color_data{};
  std::array<uint8_t, 3> effect_data{};
  ESPColorCorrection correction{};
  correction.set_local_brightness(0x7f);  // scale by 50%
  CorrectedBufferColorAccessor accessor(color_data.data(), effect_data.data(), WRGB, &correction);

  accessor.set(1, Color{10, 20, 30, 40});
  EXPECT_EQ((Color{10, 20, 30, 40}), accessor.get(1));
  EXPECT_EQ((Color{5, 10, 15, 20}), accessor.get_raw(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 20, 5, 10, 15, 0, 0, 0, 0}));

  accessor.set_raw(1, Color{15, 25, 35, 45});
  EXPECT_EQ((Color{30, 50, 70, 90}), accessor.get(1));
  EXPECT_EQ((Color{15, 25, 35, 45}), accessor.get_raw(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 15, 25, 35, 0, 0, 0, 0}));

  accessor.set_red(1, 10);
  EXPECT_EQ(10, accessor.get_red(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 5, 25, 35, 0, 0, 0, 0}));

  accessor.set_green(1, 20);
  EXPECT_EQ(20, accessor.get_green(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 5, 10, 35, 0, 0, 0, 0}));

  accessor.set_blue(1, 30);
  EXPECT_EQ(30, accessor.get_blue(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 5, 10, 15, 0, 0, 0, 0}));

  accessor.set_white(1, 40);
  EXPECT_EQ(40, accessor.get_white(1));
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 20, 5, 10, 15, 0, 0, 0, 0}));

  accessor.set_effect_data(1, 50);
  EXPECT_EQ(50, accessor.get_effect_data(1));
  EXPECT_EQ(effect_data, (std::array<uint8_t, 3>{0, 50, 0}));
}

Color round_trip(const ESPColorCorrection &correction, Color color) {
  return correction.color_uncorrect(correction.color_correct(color));
}

TEST(ColorAccessors, ESPColorView) {
  std::array<uint8_t, 12> color_data{};
  std::array<uint8_t, 3> effect_data{};
  ESPColorCorrection correction{};
  correction.set_local_brightness(0x7f);  // scale by 50%
  CorrectedBufferColorAccessor accessor(color_data.data(), effect_data.data(), WRGB, &correction);
  ESPColorView view(&accessor, 1);

  view.set(Color{10, 20, 30, 40});
  EXPECT_EQ((Color{10, 20, 30, 40}), view.get());
  EXPECT_EQ((Color{5, 10, 15, 20}), view.get_raw());
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 20, 5, 10, 15, 0, 0, 0, 0}));

  view.set(ESPHSVColor(100, 150, 200));
  EXPECT_EQ((round_trip(correction, ESPHSVColor(100, 150, 200).to_rgb())), view.get());

  view.set_hsv(ESPHSVColor(120, 170, 220));
  EXPECT_EQ((round_trip(correction, ESPHSVColor(120, 170, 220).to_rgb())), view.get());

  view.set_rgbw(10, 20, 30, 40);
  EXPECT_EQ((Color{10, 20, 30, 40}), view.get());

  view.set_rgb(12, 24, 36);  // W should be unchanged
  EXPECT_EQ((Color{12, 24, 36, 40}), view.get());

  view = Color{10, 20, 30, 40};
  EXPECT_EQ((Color{10, 20, 30, 40}), view.get());

  view = ESPHSVColor(100, 150, 200);
  EXPECT_EQ((round_trip(correction, ESPHSVColor(100, 150, 200).to_rgb())), view.get());

  view.set_raw(Color{15, 25, 35, 45});
  EXPECT_EQ((Color{30, 50, 70, 90}), view.get());
  EXPECT_EQ((Color{15, 25, 35, 45}), view.get_raw());
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 15, 25, 35, 0, 0, 0, 0}));

  view.set_red(10);
  EXPECT_EQ(10, view.get_red());
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 5, 25, 35, 0, 0, 0, 0}));

  view.set_green(20);
  EXPECT_EQ(20, view.get_green());
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 5, 10, 35, 0, 0, 0, 0}));

  view.set_blue(30);
  EXPECT_EQ(30, view.get_blue());
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 45, 5, 10, 15, 0, 0, 0, 0}));

  view.set_white(40);
  EXPECT_EQ(40, view.get_white());
  EXPECT_EQ(color_data, (std::array<uint8_t, 12>{0, 0, 0, 0, 20, 5, 10, 15, 0, 0, 0, 0}));

  view.set_effect_data(50);
  EXPECT_EQ(50, view.get_effect_data());
  EXPECT_EQ(effect_data, (std::array<uint8_t, 3>{0, 50, 0}));

  view.set(Color{10, 20, 30, 40});
  view.fade_to_white(31);
  EXPECT_EQ((round_trip(correction, Color{10, 20, 30, 40}.fade_to_white(31))), view.get());

  view.set(Color{10, 20, 30, 40});
  view.fade_to_black(31);
  EXPECT_EQ((round_trip(correction, Color{10, 20, 30, 40}.fade_to_black(31))), view.get());

  view.set(Color{10, 20, 30, 40});
  view.lighten(5);
  EXPECT_EQ((round_trip(correction, Color{10, 20, 30, 40}.lighten(5))), view.get());

  view.set(Color{10, 20, 30, 40});
  view.darken(5);
  EXPECT_EQ((round_trip(correction, Color{10, 20, 30, 40}.darken(5))), view.get());

  ESPColorView view2(&accessor, 2);
  view2.set(Color{12, 25, 36, 48});
  view = view2;
  EXPECT_EQ((Color{12, 24, 36, 48}), view.get());
  view = ESPColorView(view);
  EXPECT_EQ((Color{12, 24, 36, 48}), view.get());
}

template<size_t size> constexpr std::array<uint8_t, size * 4> make_wrgb(std::array<Color, size> colors) {
  std::array<uint8_t, size * 4> result;
  for (size_t i = 0; i < size; i++) {
    result[i * 4] = colors[i].w;
    result[i * 4 + 1] = colors[i].r;
    result[i * 4 + 2] = colors[i].g;
    result[i * 4 + 3] = colors[i].b;
  }
  return result;
}

TEST(ColorAccessors, ESPRangeView) {
  std::array<uint8_t, 32> color_data{};
  std::array<uint8_t, 8> effect_data{};
  RawBufferColorAccessor accessor(color_data.data(), effect_data.data(), WRGB);

  ESPRangeView range14(&accessor, 1, 4);
  ESPRangeView range36(&accessor, 3, 6);
  ESPRangeView range47(&accessor, 4, 7);
  Color color0{};
  Color color1{10, 20, 30, 40};
  ESPHSVColor hsv_color2{100, 150, 200};
  Color color2 = hsv_color2.to_rgb();
  ESPHSVColor hsv_color3{120, 170, 220};
  Color color3 = hsv_color3.to_rgb();
  Color color4{15, 25, 35, 45};
  Color color;

  range14.set(color1);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color1, color1, color0, color0, color0, color0}));
  EXPECT_EQ(color1, range14[0].get());
  EXPECT_EQ(color1, range14[1].get());
  EXPECT_EQ(color1, range14[2].get());

  range14[0] = color4;
  range14[1] = color0;
  range14[2] = color3;
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color4, color0, color3, color0, color0, color0, color0}));

  range14.set(hsv_color2);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color2, color2, color2, color0, color0, color0, color0}));

  range47.set_hsv(hsv_color3);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color2, color2, color2, color3, color3, color3, color0}));
  EXPECT_EQ(color3, range47[0].get());
  EXPECT_EQ(color3, range47[1].get());
  EXPECT_EQ(color3, range47[2].get());

  range14.set_rgbw(color1.r, color1.g, color1.b, color1.w);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color1, color1, color3, color3, color3, color0}));

  range14.set_rgb(color2.r, color2.g, color2.b);  // W should be unchanged
  color = {color2.r, color2.g, color2.b, color1.w};
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14 = hsv_color2;
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color2, color2, color2, color3, color3, color3, color0}));
  range14 = color1;
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color1, color1, color3, color3, color3, color0}));

  range14.set_red(15);
  color = Color{15, color1.g, color1.b, color1.w};
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14.set_green(25);
  color = Color{15, 25, color1.b, color1.w};
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14.set_blue(35);
  color = Color{15, 25, 35, color1.w};
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14.set_white(45);
  color = Color{15, 25, 35, 45};
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14.set_effect_data(50);
  EXPECT_EQ(effect_data, (std::array<uint8_t, 8>{0, 50, 50, 50, 0, 0, 0, 0}));
  range47.set_effect_data(55);
  EXPECT_EQ(effect_data, (std::array<uint8_t, 8>{0, 50, 50, 50, 55, 55, 55, 0}));

  range14.set(color1);
  range14.fade_to_white(31);
  color = color1.fade_to_white(31);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14.set(color1);
  range14.fade_to_black(31);
  color = color1.fade_to_black(31);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14.set(color1);
  range14.lighten(5);
  color = color1.lighten(5);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14.set(color1);
  range14.darken(5);
  color = color1.darken(5);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color, color, color, color3, color3, color3, color0}));

  range14 = range47;
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color3, color3, color3, color3, color3, color3, color0}));

  range36[0] = color1;
  range36[1] = color2;
  range36[2] = color4;
  range14 = range36;
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color2, color4, color2, color4, color3, color0}));

  range36[0] = color1;
  range36[1] = color2;
  range36[2] = color4;
  range47 = range36;
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color2, color1, color1, color2, color4, color0}));

  range47 = ESPRangeView(range47);
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color2, color1, color1, color2, color4, color0}));

  range47[0] = range47[1];
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color2, color1, color2, color2, color4, color0}));

  std::array<uint8_t, 12> other_color_data{};
  std::array<uint8_t, 3> other_effect_data{};
  RawBufferColorAccessor other_accessor(other_color_data.data(), other_effect_data.data(), WRGB);
  ESPRangeView other_range(&other_accessor, 0, 3);
  other_range[0] = color3;
  other_range[1] = color2;
  other_range[2] = color1;
  range36 = other_range;
  EXPECT_EQ(color_data, make_wrgb<8>({color0, color1, color2, color3, color2, color1, color4, color0}));
}

}  // namespace esphome::light::testing
