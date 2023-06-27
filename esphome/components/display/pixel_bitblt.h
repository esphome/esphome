#pragma once

#include <cstdarg>
#include <vector>

#include "pixel_formats.h"

#include "esphome/core/color.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace display {

template<typename SrcPixelFormat, typename DestPixelFormat, bool Transparency>
void bitblt(DestPixelFormat *dest, int dest_x, const SrcPixelFormat *src, int src_x, int width, DestPixelFormat color_on, DestPixelFormat color_off);

template<typename PixelFormat>
void fill(PixelFormat *dest, int x, int width, const PixelFormat &color);

} // display
} // esphome
