// Copyright (c) 2019-2022 Valentin Roland (github.com/vroland)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Original source: https://github.com/vroland/epdiy
// Modified by Xinyuan-LilyGO; combined work licensed under GPL-3.0-or-later: https://github.com/Xinyuan-LilyGO/LilyGo-EPD47
// Modified for ESPHome: removed zlib font decompression (unused)

/******************************************************************************/
/***        include files                                                   ***/
/******************************************************************************/

#include "epd_driver.h"

#include <esp_assert.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/******************************************************************************/
/***        macro definitions                                               ***/
/******************************************************************************/

/******************************************************************************/
/***        type definitions                                                ***/
/******************************************************************************/

typedef struct {
  uint8_t mask;        /* char data will be bitwise AND with this */
  uint8_t lead;        /* start bytes of current char in utf-8 encoded character */
  uint32_t beg;        /* beginning of codepoint range */
  uint32_t end;        /* end of codepoint range */
  int32_t bits_stored; /* the number of bits from the codepoint that fits in char */
} utf_t;

/******************************************************************************/
/***        local function prototypes                                       ***/
/******************************************************************************/

static inline int32_t min(int32_t x, int32_t y) { return x < y ? x : y; }

static inline int32_t max(int32_t x, int32_t y) { return x > y ? x : y; }

static int32_t utf8_len(const uint8_t ch);

static uint32_t next_cp(uint8_t **string);

static FontProperties font_properties_default();

static void IRAM_ATTR draw_char(const GFXfont *font, uint8_t *buffer, int32_t *cursor_x, int32_t cursor_y,
                                uint16_t buf_width, uint16_t buf_height, uint32_t cp, const FontProperties *props);

/**
 * @brief Calculate the bounds of a character when drawn at (x, y), move the
 *        cursor (*x) forward, adjust the given bounds.
 */
static void get_char_bounds(const GFXfont *font, uint32_t cp, int32_t *x, int32_t *y, int32_t *minx, int32_t *miny,
                            int32_t *maxx, int32_t *maxy, const FontProperties *props);

/******************************************************************************/
/***        exported variables                                              ***/
/******************************************************************************/

/******************************************************************************/
/***        local variables                                                 ***/
/******************************************************************************/

/**
 * @brief UTF-8 decode inspired from rosetta code
 *
 * https://rosettacode.org/wiki/UTF-8_encode_and_decode#C
 */
static utf_t *utf[] = {
    /*             mask        lead        beg      end       bits */
    [0] = &(utf_t){0b00111111, 0b10000000, 0, 0, 6},
    [1] = &(utf_t){0b01111111, 0b00000000, 0000, 0177, 7},
    [2] = &(utf_t){0b00011111, 0b11000000, 0200, 03777, 5},
    [3] = &(utf_t){0b00001111, 0b11100000, 04000, 0177777, 4},
    [4] = &(utf_t){0b00000111, 0b11110000, 0200000, 04177777, 3},
    &(utf_t){0},
};

/******************************************************************************/
/***        exported functions                                              ***/
/******************************************************************************/

void get_glyph(const GFXfont *font, uint32_t code_point, GFXglyph **glyph) {
  UnicodeInterval *intervals = font->intervals;
  *glyph = NULL;
  for (int32_t i = 0; i < font->interval_count; i++) {
    UnicodeInterval *interval = &intervals[i];
    if (code_point >= interval->first && code_point <= interval->last) {
      *glyph = &font->glyph[interval->offset + (code_point - interval->first)];
      return;
    }
    if (code_point < interval->first) {
      return;
    }
  }
  return;
}

void get_text_bounds(const GFXfont *font, const char *string, int32_t *x, int32_t *y, int32_t *x1, int32_t *y1,
                     int32_t *w, int32_t *h, const FontProperties *properties) {
  FontProperties props;
  if (properties == NULL) {
    props = font_properties_default();
  } else {
    props = *properties;
  }

  if (*string == '\0') {
    *w = 0;
    *h = 0;
    *y1 = *y;
    *x1 = *x;
    return;
  }
  int32_t minx = 100000, miny = 100000, maxx = -1, maxy = -1;
  int32_t original_x = *x;
  uint32_t c;
  while ((c = next_cp((uint8_t **) &string))) {
    get_char_bounds(font, c, x, y, &minx, &miny, &maxx, &maxy, &props);
  }
  *x1 = min(original_x, minx);
  *w = maxx - *x1;
  *y1 = miny;
  *h = maxy - miny;
}

void write_mode(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y, uint8_t *framebuffer,
                DrawMode_t mode, const FontProperties *properties) {
  if (*string == '\0')
    return;

  FontProperties props = (properties == NULL) ? font_properties_default() : *properties;

  int32_t x1 = 0, y1 = 0, w = 0, h = 0;
  int32_t tmp_cur_x = *cursor_x;
  int32_t tmp_cur_y = *cursor_y;
  get_text_bounds(font, string, &tmp_cur_x, &tmp_cur_y, &x1, &y1, &w, &h, &props);

  uint8_t *buffer;
  int32_t buf_width;
  int32_t buf_height;
  int32_t baseline_height = *cursor_y - y1;

  // The local cursor position:
  // 0, if drawing to a local temporary buffer
  // the given cursor position, if drawing to a full frame buffer
  int32_t local_cursor_x = 0;
  int32_t local_cursor_y = 0;

  if (framebuffer == NULL) {
    buf_width = (w / 2 + w % 2);
    buf_height = h;
    buffer = (uint8_t *) malloc(buf_width * buf_height);
    memset(buffer, 255, buf_width * buf_height);
    local_cursor_y = buf_height - baseline_height;
  } else {
    buf_width = EPD_WIDTH / 2;
    buf_height = EPD_HEIGHT;
    buffer = framebuffer;
    local_cursor_x = *cursor_x;
    local_cursor_y = *cursor_y;
  }

  uint32_t c;

  int32_t cursor_x_init = local_cursor_x;
  int32_t cursor_y_init = local_cursor_y;

  uint8_t bg = props.bg_color;
  if (props.flags & DRAW_BACKGROUND) {
    for (int32_t l = 0; l < font->advance_y; l++) {
      epd_draw_hline(local_cursor_x, local_cursor_y - (font->advance_y - baseline_height) + l, w, bg << 4, buffer);
    }
  }
  while ((c = next_cp((uint8_t **) &string))) {
    draw_char(font, buffer, &local_cursor_x, local_cursor_y, buf_width, buf_height, c, &props);
  }

  *cursor_x += local_cursor_x - cursor_x_init;
  *cursor_y += local_cursor_y - cursor_y_init;

  if (framebuffer == NULL) {
    Rect_t area = {.x = x1, .y = *cursor_y - h + baseline_height, .width = w, .height = h};
    epd_draw_image(area, buffer, mode);
    free(buffer);
  }
}

void writeln(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y, uint8_t *framebuffer) {
  return write_mode(font, string, cursor_x, cursor_y, framebuffer, BLACK_ON_WHITE, NULL);
}

void write_string(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y, uint8_t *framebuffer) {
  char *token, *newstring, *tofree;
  if (string == NULL) {
    ESP_LOGE("font.c", "cannot draw a NULL string!");
    return;
  }
  tofree = newstring = strdup(string);
  if (newstring == NULL) {
    ESP_LOGE("font.c", "cannot allocate string copy!");
    return;
  }

  // taken from the strsep manpage
  int32_t line_start = *cursor_x;
  while ((token = strsep(&newstring, "\n")) != NULL) {
    *cursor_x = line_start;
    writeln(font, token, cursor_x, cursor_y, framebuffer);
    *cursor_y += font->advance_y;
  }

  free(tofree);
}

/******************************************************************************/
/***        local functions                                                 ***/
/******************************************************************************/

static int32_t utf8_len(const uint8_t ch) {
  int32_t len = 0;
  for (utf_t **u = utf; *u; ++u) {
    if ((ch & ~(*u)->mask) == (*u)->lead) {
      break;
    }
    ++len;
  }
  if (len > 4) { /* Malformed leading byte */
    assert("invalid unicode.");
  }
  return len;
}

static uint32_t next_cp(uint8_t **string) {
  if (**string == 0)
    return 0;

  int32_t bytes = utf8_len(**string);
  uint8_t *chr = *string;
  *string += bytes;
  int32_t shift = utf[0]->bits_stored * (bytes - 1);
  uint32_t codep = (*chr++ & utf[bytes]->mask) << shift;

  for (int32_t i = 1; i < bytes; ++i, ++chr) {
    shift -= utf[0]->bits_stored;
    codep |= ((uint8_t) *chr & utf[0]->mask) << shift;
  }

  return codep;
}

static FontProperties font_properties_default() {
  FontProperties props = {.fg_color = 0, .bg_color = 15, .fallback_glyph = 0, .flags = 0};
  return props;
}

static void IRAM_ATTR draw_char(const GFXfont *font, uint8_t *buffer, int32_t *cursor_x, int32_t cursor_y,
                                uint16_t buf_width, uint16_t buf_height, uint32_t cp, const FontProperties *props) {
  GFXglyph *glyph;
  get_glyph(font, cp, &glyph);

  if (!glyph) {
    get_glyph(font, props->fallback_glyph, &glyph);
  }

  if (!glyph) {
    return;
  }

  uint32_t offset = glyph->data_offset;
  uint8_t width = glyph->width;
  uint8_t height = glyph->height;
  int32_t left = glyph->left;

  int32_t byte_width = (width / 2 + width % 2);
  unsigned long bitmap_size = byte_width * height;
  uint8_t *bitmap = &font->bitmap[offset];

  uint8_t color_lut[16];
  for (int32_t c = 0; c < 16; c++) {
    int32_t color_difference = (int32_t) props->fg_color - (int32_t) props->bg_color;
    color_lut[c] = max(0, min(15, props->bg_color + c * color_difference / 15));
  }

  for (int32_t y = 0; y < height; y++) {
    int32_t yy = cursor_y - glyph->top + y;
    if (yy < 0 || yy >= buf_height) {
      continue;
    }
    int32_t start_pos = *cursor_x + left;
    bool byte_complete = start_pos % 2;
    int32_t x = max(0, -start_pos);
    int32_t max_x = min(start_pos + width, buf_width * 2);
    for (int32_t xx = start_pos; xx < max_x; xx++) {
      uint32_t buf_pos = yy * buf_width + xx / 2;
      uint8_t old = buffer[buf_pos];
      uint8_t bm = bitmap[y * byte_width + x / 2];
      if ((x & 1) == 0) {
        bm = bm & 0xF;
      } else {
        bm = bm >> 4;
      }

      if ((xx & 1) == 0) {
        buffer[buf_pos] = (old & 0xF0) | color_lut[bm];
      } else {
        buffer[buf_pos] = (old & 0x0F) | (color_lut[bm] << 4);
      }
      byte_complete = !byte_complete;
      x++;
    }
  }

  *cursor_x += glyph->advance_x;
}

static void get_char_bounds(const GFXfont *font, uint32_t cp, int32_t *x, int32_t *y, int32_t *minx, int32_t *miny,
                            int32_t *maxx, int32_t *maxy, const FontProperties *props) {
  GFXglyph *glyph;
  get_glyph(font, cp, &glyph);

  if (!glyph) {
    get_glyph(font, props->fallback_glyph, &glyph);
  }

  if (!glyph)
    return;

  int32_t x1 = *x + glyph->left;
  int32_t y1 = *y + (glyph->top - glyph->height);
  int32_t x2 = x1 + glyph->width;
  int32_t y2 = y1 + glyph->height;

  // background needs to be taken into account
  if (props->flags & DRAW_BACKGROUND) {
    *minx = min(*x, min(*minx, x1));
    *maxx = max(max(*x + glyph->advance_x, x2), *maxx);
    *miny = min(*y + font->descender, min(*miny, y1));
    *maxy = max(font->descender + font->advance_y, max(*maxy, y2));
  } else {
    if (x1 < *minx)
      *minx = x1;
    if (y1 < *miny)
      *miny = y1;
    if (x2 > *maxx)
      *maxx = x2;
    if (y2 > *maxy)
      *maxy = y2;
  }
  *x += glyph->advance_x;
}

/******************************************************************************/
/***        END OF FILE                                                     ***/
/******************************************************************************/
