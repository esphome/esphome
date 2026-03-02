// Copyright (c) 2019-2022 Valentin Roland (github.com/vroland)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Original source: https://github.com/vroland/epdiy
// Modified by Xinyuan-LilyGO; combined work licensed under GPL-3.0-or-later:
// https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 Modified for ESPHome: removed IDF version guards, code formatting

/******************************************************************************/
/***        include files                                                   ***/
/******************************************************************************/

#include "epd_driver.h"
#include "ed047tc1.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_assert.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_types.h>
#include <xtensa/core-macros.h>

#include <string.h>
#include <math.h>

/******************************************************************************/
/***        macro definitions                                               ***/
/******************************************************************************/

/**
 * @brief number of bytes needed for one line of EPD pixel data.
 */
#define EPD_LINE_BYTES EPD_WIDTH / 4

#define CLEAR_BYTE 0B10101010
#define DARK_BYTE 0B01010101

#ifndef _swap_int
#define _swap_int(a, b) \
  { \
    int32_t t = a; \
    a = b; \
    b = t; \
  }
#endif

/******************************************************************************/
/***        type definitions                                                ***/
/******************************************************************************/

typedef struct {
  uint8_t *data_ptr;
  SemaphoreHandle_t done_smphr;
  Rect_t area;
  int32_t frame;
  DrawMode_t mode;
} OutputParams;

/******************************************************************************/
/***        local function prototypes                                       ***/
/******************************************************************************/

/**
 * @brief Reorder the output buffer to account for I2S FIFO order.
 */
static void reorder_line_buffer(uint32_t *line_data);

/**
 * @brief output a row to the display.
 */
static void write_row(uint32_t output_time_dus);

/**
 * @brief skip a display row
 */
static void skip_row(uint8_t pipeline_finish_time);

static void IRAM_ATTR reset_lut(uint8_t *lut_mem, DrawMode_t mode);

static void IRAM_ATTR update_LUT(uint8_t *lut_mem, uint8_t k, DrawMode_t mode);

/**
 * @brief bit-shift a buffer `shift` <= 7 bits to the right.
 */
static void IRAM_ATTR bit_shift_buffer_right(uint8_t *buf, uint32_t len, int32_t shift);

static void IRAM_ATTR nibble_shift_buffer_right(uint8_t *buf, uint32_t len);

static void IRAM_ATTR provide_out(OutputParams *params);

static void IRAM_ATTR feed_display(OutputParams *params);

static void epd_fill_circle_helper(int32_t x0, int32_t y0, int32_t r, int32_t corners, int32_t delta, uint8_t color,
                                   uint8_t *framebuffer);

/******************************************************************************/
/***        exported variables                                              ***/
/******************************************************************************/

/******************************************************************************/
/***        local variables                                                 ***/
/******************************************************************************/

/**
 * @brief status tracker for row skipping
 */
static uint32_t skipping;

/* 4bpp Contrast cycles in order of contrast (Darkest first).  */
static const int32_t contrast_cycles_4[15] = {30, 30, 20, 20, 30, 30, 30, 40, 40, 50, 50, 50, 100, 200, 300};

static const int32_t contrast_cycles_4_white[15] = {10, 10, 8, 8, 8, 8, 8, 10, 10, 15, 15, 20, 20, 100, 300};

// Heap space to use for the EPD output lookup table, which
// is calculated for each cycle.
static uint8_t *conversion_lut;
static QueueHandle_t output_queue;

static const DRAM_ATTR uint32_t lut_1bpp[256] = {
    0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015, 0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051,
    0x0054, 0x0055, 0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115, 0x0140, 0x0141, 0x0144, 0x0145,
    0x0150, 0x0151, 0x0154, 0x0155, 0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415, 0x0440, 0x0441,
    0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455, 0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515,
    0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555, 0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011,
    0x1014, 0x1015, 0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055, 0x1100, 0x1101, 0x1104, 0x1105,
    0x1110, 0x1111, 0x1114, 0x1115, 0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155, 0x1400, 0x1401,
    0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415, 0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
    0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515, 0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551,
    0x1554, 0x1555, 0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015, 0x4040, 0x4041, 0x4044, 0x4045,
    0x4050, 0x4051, 0x4054, 0x4055, 0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115, 0x4140, 0x4141,
    0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155, 0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415,
    0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455, 0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511,
    0x4514, 0x4515, 0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555, 0x5000, 0x5001, 0x5004, 0x5005,
    0x5010, 0x5011, 0x5014, 0x5015, 0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055, 0x5100, 0x5101,
    0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115, 0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
    0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415, 0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451,
    0x5454, 0x5455, 0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515, 0x5540, 0x5541, 0x5544, 0x5545,
    0x5550, 0x5551, 0x5554, 0x5555};

/******************************************************************************/
/***        exported functions                                              ***/
/******************************************************************************/

void epd_init() {
  skipping = 0;
  epd_base_init(EPD_WIDTH);

  conversion_lut = (uint8_t *) heap_caps_malloc(1 << 16, MALLOC_CAP_8BIT);
  assert(conversion_lut != NULL);
  output_queue = xQueueCreate(64, EPD_WIDTH / 2);
  assert(output_queue != NULL);
}

void epd_push_pixels(Rect_t area, int16_t time, int32_t color) {
  uint8_t row[EPD_LINE_BYTES] = {0};

  for (uint32_t i = 0; i < area.width; i++) {
    uint32_t position = i + area.x % 4;
    uint8_t mask = (color ? CLEAR_BYTE : DARK_BYTE) & (0b00000011 << (2 * (position % 4)));
    row[area.x / 4 + position / 4] |= mask;
  }
  reorder_line_buffer((uint32_t *) row);

  epd_start_frame();

  for (int32_t i = 0; i < EPD_HEIGHT; i++) {
    // before are of interest: skip
    if (i < area.y) {
      skip_row(time);
      // start area of interest: set row data
    } else if (i == area.y) {
      epd_switch_buffer();
      memcpy(epd_get_current_buffer(), row, EPD_LINE_BYTES);
      epd_switch_buffer();
      memcpy(epd_get_current_buffer(), row, EPD_LINE_BYTES);

      write_row(time * 10);
      // load nop row if done with area
    } else if (i >= area.y + area.height) {
      skip_row(time);
      // output the same as before
    } else {
      write_row(time * 10);
    }
  }
  // Since we "pipeline" row output, we still have to latch out the last row.
  write_row(time * 10);

  epd_end_frame();
}

void epd_clear_area(Rect_t area) { epd_clear_area_cycles(area, 4, 50); }

void epd_clear_area_cycles(Rect_t area, int32_t cycles, int32_t cycle_time) {
  const int16_t white_time = cycle_time;
  const int16_t dark_time = cycle_time;

  for (int32_t c = 0; c < cycles; c++) {
    for (int32_t i = 0; i < 4; i++) {
      epd_push_pixels(area, dark_time, 0);
    }
    for (int32_t i = 0; i < 4; i++) {
      epd_push_pixels(area, white_time, 1);
    }
  }
}

Rect_t epd_full_screen() {
  Rect_t area = {.x = 0, .y = 0, .width = EPD_WIDTH, .height = EPD_HEIGHT};
  return area;
}

void epd_clear() { epd_clear_area(epd_full_screen()); }

void IRAM_ATTR calc_epd_input_4bpp(uint32_t *line_data, uint8_t *epd_input, uint8_t k, uint8_t *conversion_lut) {
  uint32_t *wide_epd_input = (uint32_t *) epd_input;
  uint16_t *line_data_16 = (uint16_t *) line_data;

  // this is reversed for little-endian, but this is later compensated
  // through the output peripheral.
  for (uint32_t j = 0; j < EPD_WIDTH / 16; j++) {
    uint16_t v1 = *(line_data_16++);
    uint16_t v2 = *(line_data_16++);
    uint16_t v3 = *(line_data_16++);
    uint16_t v4 = *(line_data_16++);
#if USER_I2S_REG
    // Cast to uint32_t before shifting to avoid undefined behaviour: uint8_t is promoted to
    // signed int, and shifting a 0xFF value left by 24 would overflow a 32-bit signed integer.
    uint32_t pixel = (uint32_t) conversion_lut[v1] << 16 | (uint32_t) conversion_lut[v2] << 24 |
                     (uint32_t) conversion_lut[v3] | (uint32_t) conversion_lut[v4] << 8;
#else
    uint32_t pixel = (uint32_t) conversion_lut[v1] | (uint32_t) conversion_lut[v2] << 8 |
                     (uint32_t) conversion_lut[v3] << 16 | (uint32_t) conversion_lut[v4] << 24;
#endif
    wide_epd_input[j] = pixel;
  }
}

void IRAM_ATTR calc_epd_input_1bpp(uint8_t *line_data, uint8_t *epd_input, DrawMode_t mode) {
  uint32_t *wide_epd_input = (uint32_t *) epd_input;

  // this is reversed for little-endian, but this is later compensated
  // through the output peripheral.
  for (uint32_t j = 0; j < EPD_WIDTH / 16; j++) {
    uint8_t v1 = *(line_data++);
    uint8_t v2 = *(line_data++);
    wide_epd_input[j] = (lut_1bpp[v1] << 16) | lut_1bpp[v2];
  }
}

static inline uint32_t min_u32(uint32_t x, uint32_t y) { return x < y ? x : y; }

void epd_draw_hline(int32_t x, int32_t y, int32_t length, uint8_t color, uint8_t *framebuffer) {
  for (int32_t i = 0; i < length; i++) {
    int32_t xx = x + i;
    epd_draw_pixel(xx, y, color, framebuffer);
  }
}

void epd_draw_vline(int32_t x, int32_t y, int32_t length, uint8_t color, uint8_t *framebuffer) {
  for (int32_t i = 0; i < length; i++) {
    int32_t yy = y + i;
    epd_draw_pixel(x, yy, color, framebuffer);
  }
}

void epd_draw_pixel(int32_t x, int32_t y, uint8_t color, uint8_t *framebuffer) {
  if (x < 0 || x >= EPD_WIDTH) {
    return;
  }
  if (y < 0 || y >= EPD_HEIGHT) {
    return;
  }
  uint8_t *buf_ptr = &framebuffer[y * EPD_WIDTH / 2 + x / 2];
  if (x % 2) {
    *buf_ptr = (*buf_ptr & 0x0F) | (color & 0xF0);
  } else {
    *buf_ptr = (*buf_ptr & 0xF0) | (color >> 4);
  }
}

void epd_draw_circle(int32_t x0, int32_t y0, int32_t r, uint8_t color, uint8_t *framebuffer) {
  int32_t f = 1 - r;
  int32_t ddF_x = 1;
  int32_t ddF_y = -2 * r;
  int32_t x = 0;
  int32_t y = r;

  epd_draw_pixel(x0, y0 + r, color, framebuffer);
  epd_draw_pixel(x0, y0 - r, color, framebuffer);
  epd_draw_pixel(x0 + r, y0, color, framebuffer);
  epd_draw_pixel(x0 - r, y0, color, framebuffer);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    epd_draw_pixel(x0 + x, y0 + y, color, framebuffer);
    epd_draw_pixel(x0 - x, y0 + y, color, framebuffer);
    epd_draw_pixel(x0 + x, y0 - y, color, framebuffer);
    epd_draw_pixel(x0 - x, y0 - y, color, framebuffer);
    epd_draw_pixel(x0 + y, y0 + x, color, framebuffer);
    epd_draw_pixel(x0 - y, y0 + x, color, framebuffer);
    epd_draw_pixel(x0 + y, y0 - x, color, framebuffer);
    epd_draw_pixel(x0 - y, y0 - x, color, framebuffer);
  }
}

void epd_fill_circle(int32_t x0, int32_t y0, int32_t r, uint8_t color, uint8_t *framebuffer) {
  epd_draw_vline(x0, y0 - r, 2 * r + 1, color, framebuffer);
  epd_fill_circle_helper(x0, y0, r, 3, 0, color, framebuffer);
}

static void epd_fill_circle_helper(int32_t x0, int32_t y0, int32_t r, int32_t corners, int32_t delta, uint8_t color,
                                   uint8_t *framebuffer) {
  int32_t f = 1 - r;
  int32_t ddF_x = 1;
  int32_t ddF_y = -2 * r;
  int32_t x = 0;
  int32_t y = r;
  int32_t px = x;
  int32_t py = y;

  delta++;  // Avoid some +1's in the loop

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    // These checks avoid double-drawing certain lines, important
    // for the SSD1306 library which has an INVERT drawing mode.
    if (x < (y + 1)) {
      if (corners & 1)
        epd_draw_vline(x0 + x, y0 - y, 2 * y + delta, color, framebuffer);
      if (corners & 2)
        epd_draw_vline(x0 - x, y0 - y, 2 * y + delta, color, framebuffer);
    }
    if (y != py) {
      if (corners & 1)
        epd_draw_vline(x0 + py, y0 - px, 2 * px + delta, color, framebuffer);
      if (corners & 2)
        epd_draw_vline(x0 - py, y0 - px, 2 * px + delta, color, framebuffer);
      py = y;
    }
    px = x;
  }
}

void epd_draw_oval(int x0, int y0, int rx, int ry, uint8_t color, uint8_t *framebuffer) {
  int x = 0;
  int y = ry;
  int64_t rxSq = (int64_t) rx * rx;
  int64_t rySq = (int64_t) ry * ry;
  int64_t twoRxSq = 2 * rxSq;
  int64_t twoRySq = 2 * rySq;
  int64_t px = 0;
  int64_t py = twoRxSq * y;

  // Region 1: Top and bottom edges
  int64_t p = round(rySq - (rxSq * ry) + (0.25 * rxSq));
  while (px < py) {
    epd_draw_pixel(x0 + x, y0 + y, color, framebuffer);
    epd_draw_pixel(x0 - x, y0 + y, color, framebuffer);
    epd_draw_pixel(x0 + x, y0 - y, color, framebuffer);
    epd_draw_pixel(x0 - x, y0 - y, color, framebuffer);

    x++;
    px += twoRySq;
    if (p < 0) {
      p += rySq + px;
    } else {
      y--;
      py -= twoRxSq;
      p += rySq + px - py;
    }
  }

  // Region 2: Left and right edges
  p = round(rySq * (x + 0.5) * (x + 0.5) + rxSq * (y - 1) * (y - 1) - rxSq * rySq);
  while (y >= 0) {
    epd_draw_pixel(x0 + x, y0 + y, color, framebuffer);
    epd_draw_pixel(x0 - x, y0 + y, color, framebuffer);
    epd_draw_pixel(x0 + x, y0 - y, color, framebuffer);
    epd_draw_pixel(x0 - x, y0 - y, color, framebuffer);

    y--;
    py -= twoRxSq;
    if (p > 0) {
      p += rxSq - py;
    } else {
      x++;
      px += twoRySq;
      p += rxSq - py + px;
    }
  }
}

void epd_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color, uint8_t *framebuffer) {
  epd_draw_hline(x, y, w, color, framebuffer);
  epd_draw_hline(x, y + h - 1, w, color, framebuffer);
  epd_draw_vline(x, y, h, color, framebuffer);
  epd_draw_vline(x + w - 1, y, h, color, framebuffer);
}

void epd_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color, uint8_t *framebuffer) {
  for (int32_t i = x; i < x + w; i++) {
    epd_draw_vline(i, y, h, color, framebuffer);
  }
}

void epd_write_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color, uint8_t *framebuffer) {
  int32_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int(x0, y0);
    _swap_int(x1, y1);
  }

  if (x0 > x1) {
    _swap_int(x0, x1);
    _swap_int(y0, y1);
  }

  int32_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int32_t err = dx / 2;
  int32_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      epd_draw_pixel(y0, x0, color, framebuffer);
    } else {
      epd_draw_pixel(x0, y0, color, framebuffer);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void epd_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color, uint8_t *framebuffer) {
  // Update in subclasses if desired!
  if (x0 == x1) {
    if (y0 > y1)
      _swap_int(y0, y1);
    epd_draw_vline(x0, y0, y1 - y0 + 1, color, framebuffer);
  } else if (y0 == y1) {
    if (x0 > x1)
      _swap_int(x0, x1);
    epd_draw_hline(x0, y0, x1 - x0 + 1, color, framebuffer);
  } else {
    epd_write_line(x0, y0, x1, y1, color, framebuffer);
  }
}

void epd_draw_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color,
                       uint8_t *framebuffer) {
  epd_draw_line(x0, y0, x1, y1, color, framebuffer);
  epd_draw_line(x1, y1, x2, y2, color, framebuffer);
  epd_draw_line(x2, y2, x0, y0, color, framebuffer);
}

void epd_fill_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color,
                       uint8_t *framebuffer) {
  int32_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    _swap_int(y0, y1);
    _swap_int(x0, x1);
  }
  if (y1 > y2) {
    _swap_int(y2, y1);
    _swap_int(x2, x1);
  }
  if (y0 > y1) {
    _swap_int(y0, y1);
    _swap_int(x0, x1);
  }

  if (y0 == y2) {  // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a)
      a = x1;
    else if (x1 > b)
      b = x1;
    if (x2 < a)
      a = x2;
    else if (x2 > b)
      b = x2;
    epd_draw_hline(a, y0, b - a + 1, color, framebuffer);
    return;
  }

  int32_t dx01 = x1 - x0;
  int32_t dy01 = y1 - y0;
  int32_t dx02 = x2 - x0;
  int32_t dy02 = y2 - y0;
  int32_t dx12 = x2 - x1;
  int32_t dy12 = y2 - y1;
  int32_t sa = 0;
  int32_t sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2)
    last = y1;  // Include y1 scanline
  else
    last = y1 - 1;  // Skip it

  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      _swap_int(a, b);
    epd_draw_hline(a, y, b - a + 1, color, framebuffer);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = (int32_t) dx12 * (y - y1);
  sb = (int32_t) dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b)
      _swap_int(a, b);
    epd_draw_hline(a, y, b - a + 1, color, framebuffer);
  }
}

void epd_copy_to_framebuffer(Rect_t image_area, uint8_t *image_data, uint8_t *framebuffer) {
  assert(image_data != NULL && framebuffer != NULL);

  for (uint32_t i = 0; i < image_area.width * image_area.height; i++) {
    uint32_t value_index = i;
    // for images of uneven width,
    // consume an additional nibble per row.
    if (image_area.width % 2) {
      value_index += i / image_area.width;
    }
    uint8_t val = (value_index % 2) ? (image_data[value_index / 2] & 0xF0) >> 4 : image_data[value_index / 2] & 0x0F;

    int32_t xx = image_area.x + i % image_area.width;
    if (xx < 0 || xx >= EPD_WIDTH) {
      continue;
    }
    int32_t yy = image_area.y + i / image_area.width;
    if (yy < 0 || yy >= EPD_HEIGHT) {
      continue;
    }
    uint8_t *buf_ptr = &framebuffer[yy * EPD_WIDTH / 2 + xx / 2];
    if (xx % 2) {
      *buf_ptr = (*buf_ptr & 0x0F) | (val << 4);
    } else {
      *buf_ptr = (*buf_ptr & 0xF0) | val;
    }
  }
}

void IRAM_ATTR epd_draw_grayscale_image(Rect_t area, uint8_t *data) { epd_draw_image(area, data, BLACK_ON_WHITE); }

void IRAM_ATTR epd_draw_frame_1bit(Rect_t area, uint8_t *ptr, DrawMode_t mode, int32_t time) {
  epd_start_frame();
  uint8_t line[EPD_WIDTH / 8];
  memset(line, 0, sizeof(line));

  if (area.x < 0) {
    ptr += -area.x / 8;
  }

  int32_t ceil_byte_width = (area.width / 8 + (area.width % 8 > 0));
  if (area.y < 0) {
    ptr += ceil_byte_width * -area.y;
  }

  for (int32_t i = 0; i < EPD_HEIGHT; i++) {
    if (i < area.y || i >= area.y + area.height) {
      skip_row(time);
      continue;
    }

    uint8_t *lp;
    bool shifted = 0;
    if (area.width == EPD_WIDTH && area.x == 0) {
      lp = ptr;
      ptr += EPD_WIDTH / 8;
    } else {
      uint8_t *buf_start = (uint8_t *) line;
      uint32_t line_bytes = ceil_byte_width;
      if (area.x >= 0) {
        buf_start += area.x / 8;
      } else {
        // reduce line_bytes to actually used bytes
        line_bytes += area.x / 8;
      }
      line_bytes = min_u32(line_bytes, EPD_WIDTH / 8 - (uint32_t) (buf_start - line));
      memcpy(buf_start, ptr, line_bytes);
      ptr += ceil_byte_width;

      // mask last n bits if width is not divisible by 8
      if (area.width % 8 != 0 && ceil_byte_width + 1 < EPD_WIDTH) {
        uint8_t mask = 0;
        for (int32_t s = 0; s < area.width % 8; s++) {
          mask = (mask << 1) | 1;
        }
        *(buf_start + line_bytes - 1) &= mask;
      }

      if (area.x % 8 != 0 && area.x < EPD_WIDTH) {
        // shift to right
        shifted = true;
        bit_shift_buffer_right(
            buf_start, min_u32(line_bytes + 1, (uint32_t) line + EPD_WIDTH / 8 - (uint32_t) buf_start), area.x % 8);
      }
      lp = line;
    }
    calc_epd_input_1bpp(lp, epd_get_current_buffer(), mode);
    epd_output_row(time);
    if (shifted) {
      memset(line, 0, sizeof(line));
    }
  }
  if (!skipping) {
    epd_output_row(time);
  }
  epd_end_frame();
}

void IRAM_ATTR epd_draw_image(Rect_t area, uint8_t *data, DrawMode_t mode) {
  uint8_t frame_count = 15;

  SemaphoreHandle_t fetch_sem = xSemaphoreCreateBinary();
  SemaphoreHandle_t feed_sem = xSemaphoreCreateBinary();
  if (fetch_sem == NULL || feed_sem == NULL) {
    ESP_LOGE("epd_driver", "Failed to create EPD semaphores (out of memory)");
    vSemaphoreDelete(fetch_sem);
    vSemaphoreDelete(feed_sem);
    return;
  }
  vTaskDelay(10);
  for (uint8_t k = 0; k < frame_count; k++) {
    OutputParams p1 = {
        .area = area,
        .data_ptr = data,
        .frame = k,
        .mode = mode,
        .done_smphr = fetch_sem,
    };
    OutputParams p2 = {
        .area = area,
        .data_ptr = data,
        .frame = k,
        .mode = mode,
        .done_smphr = feed_sem,
    };

    TaskHandle_t t1, t2;
    BaseType_t rc1 = xTaskCreatePinnedToCore((void (*)(void *)) provide_out, "provide_out", 8192, &p1, 2, &t1, 0);
    BaseType_t rc2 = xTaskCreatePinnedToCore((void (*)(void *)) feed_display, "render", 8192, &p2, 2, &t2, 1);
    if (rc1 != pdPASS || rc2 != pdPASS) {
      ESP_LOGE("epd_driver", "Failed to create EPD render tasks (out of memory)");
      if (rc1 == pdPASS)
        vTaskDelete(t1);
      if (rc2 == pdPASS)
        vTaskDelete(t2);
      break;
    }

    xSemaphoreTake(fetch_sem, portMAX_DELAY);
    xSemaphoreTake(feed_sem, portMAX_DELAY);

    vTaskDelete(t1);
    vTaskDelete(t2);
    vTaskDelay(5);
  }
  vSemaphoreDelete(fetch_sem);
  vSemaphoreDelete(feed_sem);
}

/******************************************************************************/
/***        local functions                                                 ***/
/******************************************************************************/

static void write_row(uint32_t output_time_dus) {
  // avoid too light output after skipping on some displays
  if (skipping) {
    // vTaskDelay(20);
  }
  skipping = 0;
  epd_output_row(output_time_dus);
}

static void skip_row(uint8_t pipeline_finish_time) {
  // output previously loaded row, fill buffer with no-ops.
  if (skipping == 0) {
    epd_switch_buffer();
    memset(epd_get_current_buffer(), 0, EPD_LINE_BYTES);
    epd_switch_buffer();
    memset(epd_get_current_buffer(), 0, EPD_LINE_BYTES);
    epd_output_row(pipeline_finish_time);
    // avoid tainting of following rows by
    // allowing residual charge to dissipate
    // vTaskDelay(10);
    /*
    unsigned counts = XTHAL_GET_CCOUNT() + 50 * 240;
    while (XTHAL_GET_CCOUNT() < counts) {
    };
    */
  } else if (skipping < 2) {
    epd_output_row(10);
  } else {
    // epd_output_row(5);
    epd_skip();
  }
  skipping++;
}

static void reorder_line_buffer(uint32_t *line_data) {
  for (uint32_t i = 0; i < EPD_LINE_BYTES / 4; i++) {
    uint32_t val = *line_data;
    *(line_data++) = val >> 16 | ((val & 0x0000FFFF) << 16);
  }
}

static void IRAM_ATTR reset_lut(uint8_t *lut_mem, DrawMode_t mode) {
  switch (mode) {
    case BLACK_ON_WHITE:
      memset(lut_mem, 0x55, (1 << 16));
      break;
    case WHITE_ON_BLACK:
    case WHITE_ON_WHITE:
      memset(lut_mem, 0xAA, (1 << 16));
      break;
    default:
      ESP_LOGW("epd_driver", "unknown draw mode %d!", mode);
      break;
  }
}

static void IRAM_ATTR update_LUT(uint8_t *lut_mem, uint8_t k, DrawMode_t mode) {
  if (mode == BLACK_ON_WHITE || mode == WHITE_ON_WHITE) {
    k = 15 - k;
  }

  // reset the pixels which are not to be lightened / darkened
  // any longer in the current frame
  for (uint32_t l = k; l < (1 << 16); l += 16) {
    lut_mem[l] &= 0xFC;
  }

  for (uint32_t l = (k << 4); l < (1 << 16); l += (1 << 8)) {
    for (uint32_t p = 0; p < 16; p++) {
      lut_mem[l + p] &= 0xF3;
    }
  }
  for (uint32_t l = (k << 8); l < (1 << 16); l += (1 << 12)) {
    for (uint32_t p = 0; p < (1 << 8); p++) {
      lut_mem[l + p] &= 0xCF;
    }
  }
  for (uint32_t p = (k << 12); p < ((k + 1) << 12); p++) {
    lut_mem[p] &= 0x3F;
  }
}

static void IRAM_ATTR bit_shift_buffer_right(uint8_t *buf, uint32_t len, int32_t shift) {
  uint8_t carry = 0x00;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = buf[i];
    buf[i] = (val << shift) | carry;
    carry = val >> (8 - shift);
  }
}

static void IRAM_ATTR nibble_shift_buffer_right(uint8_t *buf, uint32_t len) {
  uint8_t carry = 0xF;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = buf[i];
    buf[i] = (val << 4) | carry;
    carry = (val & 0xF0) >> 4;
  }
}

static void IRAM_ATTR provide_out(OutputParams *params) {
  uint8_t line[EPD_WIDTH / 2];
  memset(line, 255, EPD_WIDTH / 2);
  Rect_t area = params->area;
  uint8_t *ptr = params->data_ptr;

  if (params->frame == 0) {
    reset_lut(conversion_lut, params->mode);
  }

  update_LUT(conversion_lut, params->frame, params->mode);

  if (area.x < 0) {
    ptr += -area.x / 2;
  }
  if (area.y < 0) {
    ptr += (area.width / 2 + area.width % 2) * -area.y;
  }

  for (int32_t i = 0; i < EPD_HEIGHT; i++) {
    if (i < area.y || i >= area.y + area.height) {
      continue;
    }

    uint32_t *lp;
    bool shifted = false;
    if (area.width == EPD_WIDTH && area.x == 0) {
      lp = (uint32_t *) ptr;
      ptr += EPD_WIDTH / 2;
    } else {
      uint8_t *buf_start = (uint8_t *) line;
      uint32_t line_bytes = area.width / 2 + area.width % 2;
      if (area.x >= 0) {
        buf_start += area.x / 2;
      } else {
        // reduce line_bytes to actually used bytes
        line_bytes += area.x / 2;
      }
      line_bytes = min_u32(line_bytes, EPD_WIDTH / 2 - (uint32_t) (buf_start - line));
      memcpy(buf_start, ptr, line_bytes);
      ptr += area.width / 2 + area.width % 2;

      // mask last nibble for uneven width
      if (area.width % 2 == 1 && area.x / 2 + area.width / 2 + 1 < EPD_WIDTH) {
        *(buf_start + line_bytes - 1) |= 0xF0;
      }
      if (area.x % 2 == 1 && area.x < EPD_WIDTH) {
        shifted = true;
        // shift one nibble to right
        nibble_shift_buffer_right(buf_start,
                                  min_u32(line_bytes + 1, (uint32_t) line + EPD_WIDTH / 2 - (uint32_t) buf_start));
      }
      lp = (uint32_t *) line;
    }
    xQueueSendToBack(output_queue, lp, portMAX_DELAY);
    if (shifted) {
      memset(line, 255, EPD_WIDTH / 2);
    }
  }

  xSemaphoreGive(params->done_smphr);
  vTaskDelay(portMAX_DELAY);
}

static void IRAM_ATTR feed_display(OutputParams *params) {
  Rect_t area = params->area;
  const int32_t *contrast_lut = contrast_cycles_4;
  switch (params->mode) {
    case WHITE_ON_WHITE:
    case BLACK_ON_WHITE:
      contrast_lut = contrast_cycles_4;
      break;
    case WHITE_ON_BLACK:
      contrast_lut = contrast_cycles_4_white;
      break;
  }

  epd_start_frame();
  for (int32_t i = 0; i < EPD_HEIGHT; i++) {
    if (i < area.y || i >= area.y + area.height) {
      skip_row(contrast_lut[params->frame]);
      continue;
    }
    uint8_t output[EPD_WIDTH / 2];
    xQueueReceive(output_queue, output, portMAX_DELAY);
    calc_epd_input_4bpp((uint32_t *) output, epd_get_current_buffer(), params->frame, conversion_lut);
    write_row(contrast_lut[params->frame]);
  }
  if (!skipping) {
    // Since we "pipeline" row output, we still have to latch out the last row.
    write_row(contrast_lut[params->frame]);
  }
  epd_end_frame();

  xSemaphoreGive(params->done_smphr);
  vTaskDelay(portMAX_DELAY);
}

/******************************************************************************/
/***        END OF FILE                                                     ***/
/******************************************************************************/
