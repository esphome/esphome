// Copyright (c) 2019-2022 Valentin Roland (github.com/vroland)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Original source: https://github.com/vroland/epdiy
// Modified by Xinyuan-LilyGO; combined work licensed under GPL-3.0-or-later:
// https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 Modified for ESPHome: utilities.h renamed to board_pins.h

/**
 * A high-level library for drawing to an EPD.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "board_pins.h"

#ifdef ESP_PLATFORM
#include <esp_attr.h>
#else
#ifndef IRAM_ATTR
#define IRAM_ATTR  // NOLINT
#endif
#endif

#ifdef __cplusplus
namespace esphome {
namespace lilygo_t5_47_plus {
extern "C" {
#endif

/**
 * @brief Width of the display area in pixels.
 */
#define EPD_WIDTH 960  // NOLINT

/**
 * @brief Height of the display area in pixels.
 */
#define EPD_HEIGHT 540  // NOLINT

/**
 * @brief An area on the display.
 */
typedef struct {  // NOLINT(modernize-use-using)
  int32_t x;      /** Horizontal position. */
  int32_t y;      /** Vertical position. */
  int32_t width;  /** Area / image width, must be positive. */
  int32_t height; /** Area / image height, must be positive. */
} Rect_t;

/**
 * @brief The image drawing mode.
 */
typedef enum {  // NOLINT(modernize-use-using)
  BLACK_ON_WHITE = 1 << 0, /** Draw black / grayscale image on a white display. */
  WHITE_ON_WHITE = 1 << 1, /** "Draw with white ink" on a white display. */
  WHITE_ON_BLACK = 1 << 2, /** Draw with white ink on a black display. */
} DrawMode_t;

/**
 * @brief Font drawing flags.
 */
enum DrawFlags {
  DRAW_BACKGROUND = 1 << 0, /** Draw a background. Take the background into account when calculating the size. */
};

/**
 * @brief Font properties.
 */
typedef struct {  // NOLINT(modernize-use-using)
  uint8_t fg_color : 4;    /** Foreground color */
  uint8_t bg_color : 4;    /** Background color */
  uint32_t fallback_glyph; /** Use the glyph for this codepoint for missing glyphs. */
  uint32_t flags;          /** Additional flags, reserved for future use */
} FontProperties;

/**
 * @brief Initialize the ePaper display
 */
void epd_init();

/**
 * @brief Enable display power supply.
 */
void epd_poweron();

/**
 * @brief Disable display power supply.
 */
void epd_poweroff();

/**
 * @brief Clear the whole screen by flashing it.
 */
void epd_clear();

void epd_poweroff_all();

/**
 * @brief Clear an area by flashing it.
 *
 * @param area The area to clear.
 */
void epd_clear_area(Rect_t area);

/**
 * @brief Clear an area by flashing it.
 *
 * @param area       The area to clear.
 * @param cycles     The number of black-to-white clear cycles.
 * @param cycle_time Length of a cycle. Default: 50 (us).
 */
void epd_clear_area_cycles(Rect_t area, int32_t cycles, int32_t cycle_time);

/**
 * @brief Darken / lighten an area for a given time.
 *
 * @param area  The area to darken / lighten.
 * @param time  The time in us to apply voltage to each pixel.
 * @param color 1: lighten, 0: darken.
 */
void epd_push_pixels(Rect_t area, int16_t time, int32_t color);

/**
 * @brief Draw a picture to a given area. The image area is not cleared and
 *        assumed to be white before drawing.
 *
 * @param area The display area to draw to. `width` and `height` of the area
 *             must correspond to the image dimensions in pixels.
 * @param data The image data, as a buffer of 4 bit wide brightness values.
 *             Pixel data is packed (two pixels per byte). A byte cannot wrap
 *             over multiple rows, images of uneven width must add a padding
 *             nibble per line.
 */
void IRAM_ATTR epd_draw_grayscale_image(Rect_t area, uint8_t *data);

/**
 * @brief Draw a picture to a given area, with some draw mode.
 *
 * @note The image area is not cleared before drawing. For example, this can be
 *       used for pixel-aligned clearing.
 *
 * @param area The display area to draw to. `width` and `height` of the area
 *             must correspond to the image dimensions in pixels.
 * @param data The image data, as a buffer of 4 bit wide brightness values.
 *             Pixel data is packed (two pixels per byte). A byte cannot wrap
 *             over multiple rows, images of uneven width must add a padding
 *             nibble per line.
 */
void IRAM_ATTR epd_draw_image(Rect_t area, uint8_t *data, DrawMode_t mode);

void IRAM_ATTR epd_draw_frame_1bit(Rect_t area, uint8_t *ptr, DrawMode_t mode, int32_t time);

/**
 * @brief Rectancle representing the whole screen area.
 */
Rect_t epd_full_screen();

/**
 * @brief Draw a picture to a given framebuffer.
 *
 * @param image_area  The area to copy to. `width` and `height` of the area must
 *                    correspond to the image dimensions in pixels.
 * @param image_data  The image data, as a buffer of 4 bit wide brightness values.
 *                    Pixel data is packed (two pixels per byte). A byte cannot
 *                    wrap over multiple rows, images of uneven width must add a
 *                    padding nibble per line.
 * @param framebuffer The framebuffer object, which must
 *                    be `EPD_WIDTH / 2 * EPD_HEIGHT` large.
 */
void epd_copy_to_framebuffer(Rect_t image_area, uint8_t *image_data, uint8_t *framebuffer);

/**
 * @brief Draw a pixel a given framebuffer.
 *
 * @param x           Horizontal position in pixels.
 * @param y           Vertical position in pixels.
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to.
 */
void epd_draw_pixel(int32_t x, int32_t y, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a horizontal line to a given framebuffer.
 *
 * @param x           Horizontal start position in pixels.
 * @param y           Vertical start position in pixels.
 * @param length      Length of the line in pixels.
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to, which must
 *                    be `EPD_WIDTH / 2 * EPD_HEIGHT` bytes large.
 */
void epd_draw_hline(int32_t x, int32_t y, int32_t length, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a horizontal line to a given framebuffer.
 *
 * @param x           Horizontal start position in pixels.
 * @param y           Vertical start position in pixels.
 * @param length      Length of the line in pixels.
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to, which must
 *                    be `EPD_WIDTH / 2 * EPD_HEIGHT` bytes large.
 */
void epd_draw_vline(int32_t x, int32_t y, int32_t length, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a circle with given center and radius
 *
 * @param x0          Center-point x coordinate
 * @param y0          Center-point y coordinate
 * @param r           Radius of the circle in pixels
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to
 */
void epd_draw_circle(int32_t x, int32_t y, int32_t r, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a circle with fill with given center and radius
 *
 * @param x0          Center-point x coordinate
 * @param y0          Center-point y coordinate
 * @param r           Radius of the circle in pixels
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to,
 */
void epd_fill_circle(int32_t x, int32_t y, int32_t r, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw an oval with given center and radius
 *
 * @param x0          Center-point x coordinate
 * @param y0          Center-point y coordinate
 * @param rx          Radius of the circle in x plane
 * @param ry          Radius of the circle in y plane
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to
 */
void epd_draw_oval(int x0, int y0, int rx, int ry, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a rectanle with no fill color
 *
 * @param x           Top left corner x coordinate
 * @param y           Top left corner y coordinate
 * @param w           Width in pixels
 * @param h           Height in pixels
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to,
 */
void epd_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a rectanle with fill color
 *
 * @param x           Top left corner x coordinate
 * @param y           Top left corner y coordinate
 * @param w           Width in pixels
 * @param h           Height in pixels
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to
 */
void epd_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Write a line.  Bresenham's algorithm - thx wikpedia
 *
 * @param x0          Start point x coordinate
 * @param y0          Start point y coordinate
 * @param x1          End point x coordinate
 * @param y1          End point y coordinate
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to
 */
void epd_write_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a line
 *
 * @param x0          Start point x coordinate
 * @param y0          Start point y coordinate
 * @param x1          End point x coordinate
 * @param y1          End point y coordinate
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to
 */
void epd_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color, uint8_t *framebuffer);

/**
 * @brief Draw a triangle with no fill color
 *
 * @param x0          Vertex #0 x coordinate
 * @param y0          Vertex #0 y coordinate
 * @param x1          Vertex #1 x coordinate
 * @param y1          Vertex #1 y coordinate
 * @param x2          Vertex #2 x coordinate
 * @param y2          Vertex #2 y coordinate
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to
 */
void epd_draw_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color,
                       uint8_t *framebuffer);

/**
 * @brief Draw a triangle with color-fill
 *
 * @param x0          Vertex #0 x coordinate
 * @param y0          Vertex #0 y coordinate
 * @param x1          Vertex #1 x coordinate
 * @param y1          Vertex #1 y coordinate
 * @param x2          Vertex #2 x coordinate
 * @param y2          Vertex #2 y coordinate
 * @param color       The gray value of the line (0-255);
 * @param framebuffer The framebuffer to draw to
 */
void epd_fill_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color,
                       uint8_t *framebuffer);

/**
 * @brief Font data stored PER GLYPH
 */
typedef struct {  // NOLINT(modernize-use-using)
  uint8_t width;            /** Bitmap dimensions in pixels */
  uint8_t height;           /** Bitmap dimensions in pixels */
  uint8_t advance_x;        /** Distance to advance cursor (x axis) */
  int16_t left;             /** X dist from cursor pos to UL corner */
  int16_t top;              /** Y dist from cursor pos to UL corner */
  uint16_t compressed_size; /** Size of the zlib-compressed font data. */
  uint32_t data_offset;     /** Pointer into GFXfont->bitmap */
} GFXglyph;

/**
 * @brief Glyph interval structure
 */
typedef struct {  // NOLINT(modernize-use-using)
  uint32_t first;  /** The first unicode code point of the interval */
  uint32_t last;   /** The last unicode code point of the interval */
  uint32_t offset; /** Index of the first code point into the glyph array */
} UnicodeInterval;

/**
 * @brief Data stored for FONT AS A WHOLE
 */
typedef struct {  // NOLINT(modernize-use-using)
  uint8_t *bitmap;            /** Glyph bitmaps, concatenated */
  GFXglyph *glyph;            /** Glyph array */
  UnicodeInterval *intervals; /** Valid unicode intervals for this font */
  uint32_t interval_count;    /** Number of unicode intervals. */
  bool compressed;            /** Does this font use compressed glyph bitmaps? */
  uint8_t advance_y;          /** Newline distance (y axis) */
  int32_t ascender;           /** Maximal height of a glyph above the base line */
  int32_t descender;          /** Maximal height of a glyph below the base line */
} GFXfont;

/**
 * @brief Get the text bounds for string, when drawn at (x, y).
 *        Set font properties to NULL to use the defaults.
 */
void get_text_bounds(const GFXfont *font, const char *string, int32_t *x, int32_t *y, int32_t *x1, int32_t *y1,
                     int32_t *w, int32_t *h, const FontProperties *props);

/**
 * @brief Write text to the EPD.
 */
void writeln(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y, uint8_t *framebuffer);

/**
 * @brief Write text to the EPD.
 *
 * @note If framebuffer is NULL, draw mode `mode` is used for direct drawing.
 */
void write_mode(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y, uint8_t *framebuffer,
                DrawMode_t mode, const FontProperties *properties);

/**
 * @brief Get the font glyph for a unicode code point.
 */
void get_glyph(const GFXfont *font, uint32_t code_point, GFXglyph **glyph);

/**
 * @brief Write a (multi-line) string to the EPD.
 */
void write_string(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y, uint8_t *framebuffer);

#ifdef __cplusplus
}  // extern "C"
}  // namespace lilygo_t5_47_plus
}  // namespace esphome
#endif
