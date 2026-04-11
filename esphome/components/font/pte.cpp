/*
Copyright (c) 2015, Matt Pyne
All rights reserved.

The original source and documentation for this library is available at https://github.com/matt123p/portable-type-engine


Redistribution and use in source and binary forms, with or without modification, are permitted
provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions
and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse
or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "pte.h"

#include "esphome/core/defines.h"

#include <cstdlib>
#include <cstring>

#ifdef USE_FONT_PTE

namespace esphome::font {

static uint32_t extract_codepoint(const char *utf8_str, size_t *length) {
  const uint8_t *current = reinterpret_cast<const uint8_t *>(utf8_str);
  uint32_t code_point = 0;
  uint8_t c1 = *current++;

  if (c1 == 0) {
    *length = 0;
    return 0;
  }

  if (c1 < 0x80) {
    code_point = c1;
  } else if ((c1 & 0xE0) == 0xC0) {
    uint8_t c2 = *current++;
    if ((c2 & 0xC0) != 0x80) {
      *length = 0;
      return 0;
    }
    code_point = static_cast<uint32_t>(c1 & 0x1F) << 6;
    code_point |= static_cast<uint32_t>(c2 & 0x3F);
    if (code_point <= 0x7F) {
      *length = 0;
      return 0;
    }
  } else if ((c1 & 0xF0) == 0xE0) {
    uint8_t c2 = *current++;
    uint8_t c3 = *current++;
    if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
      *length = 0;
      return 0;
    }
    code_point = static_cast<uint32_t>(c1 & 0x0F) << 12;
    code_point |= static_cast<uint32_t>(c2 & 0x3F) << 6;
    code_point |= static_cast<uint32_t>(c3 & 0x3F);
    if (code_point <= 0x7FF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
      *length = 0;
      return 0;
    }
  } else if ((c1 & 0xF8) == 0xF0) {
    uint8_t c2 = *current++;
    uint8_t c3 = *current++;
    uint8_t c4 = *current++;
    if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) {
      *length = 0;
      return 0;
    }
    code_point = static_cast<uint32_t>(c1 & 0x07) << 18;
    code_point |= static_cast<uint32_t>(c2 & 0x3F) << 12;
    code_point |= static_cast<uint32_t>(c3 & 0x3F) << 6;
    code_point |= static_cast<uint32_t>(c4 & 0x3F);
    if (code_point <= 0xFFFF || code_point > 0x10FFFF) {
      *length = 0;
      return 0;
    }
  } else {
    *length = 0;
    return 0;
  }

  *length = current - reinterpret_cast<const uint8_t *>(utf8_str);
  return code_point;
}

// Find a character in a font
static const PteGlyph *find_char(uint32_t c, const PteBaseFont *f) {
  // Do a binary search to find the character
  int l = 0;
  int h = f->m_number_glyphs - 1;

  // Is it a top or bottom code?
  if (c == f->m_gylphs[l].code) {
    return &(f->m_gylphs[l]);
  }
  if (c == f->m_gylphs[h].code) {
    return &(f->m_gylphs[h]);
  }

  // Is it outside of this array?
  if (c < f->m_gylphs[l].code) {
    // We don't have the character
    return NULL;
  }
  if (c > f->m_gylphs[h].code) {
    // We don't have the character
    return nullptr;
  }

  while (h - l > 1) {
    // Find a the mid-point
    int m = (h - l) / 2 + l;

    // Do we have it?
    if (c == f->m_gylphs[m].code) {
      return &(f->m_gylphs[m]);
    }

    // Ok, higher or lower?
    if (c < f->m_gylphs[m].code) {
      // Lower
      h = m;
    } else {
      // Higher
      l = m;
    }
  }

  // Not found!
  return nullptr;
}

static int search_kern(uint32_t c, const PteBaseFont *f) {
  // Do a binary search to find the character
  int l = 0;
  int h = f->m_number_kerns - 1;

  if (!f->m_kerns || f->m_number_kerns == 0) {
    return -1;
  }

  // Is it a top or bottom code?
  if (c == f->m_kerns[l].first) {
    return l;
  }
  if (c == f->m_kerns[h].first) {
    return h;
  }

  // Is it outside of this array?
  if (c < f->m_kerns[l].first) {
    // We don't have the character
    return -1;
  }
  if (c > f->m_kerns[h].first) {
    // We don't have the character
    return -1;
  }

  while (h - l > 1) {
    // Find a the mid-point
    int m = (h - l) / 2 + l;

    // Do we have it?
    if (c == f->m_kerns[m].first) {
      return m;
    }

    // Ok, higher or lower?
    if (c < f->m_kerns[m].first) {
      // Lower
      h = m;
    } else {
      // Higher
      l = m;
    }
  }

  // Not found!
  return -1;
}

static const PteKern *find_kern(uint32_t c1, uint32_t c2, const PteBaseFont *f) {
  // Find the first kern of this type in the array
  int mid_p = search_kern(c1, f);

  // Now scan up/down to see if we have the pair
  int d;
  for (d = -1; d <= 1; d += 2) {
    int p = mid_p;
    while (p != -1 && p != f->m_number_kerns && f->m_kerns[p].first == c1) {
      if (f->m_kerns[p].second == c2) {
        return &(f->m_kerns[p]);
      }
      p += d;
    }
  }

  return nullptr;
}

// Bitblt a horizontal line from a compressed source
static void blt_horz_cmprs_resize(const unsigned char **ptr, int *col, int *pixels_to_go, int src_width, int dst_x,
                                  int dst_y, int pixel_xinc, int pixel_yinc, int ra, int rb, int sub_offset_x,
                                  int lines, int overspill, int plot_col) {
  unsigned char line_acc[128];

  int x_start = -((rb * sub_offset_x) / ra) / rb;
  int x = x_start;
  int c = rb;
  int p = 0;
  int count = lines;
  int div = 0;

  std::memset(line_acc, 0, sizeof(line_acc));

  // Accumulate a horizontal line of data
  while (count > 0) {
    if (x >= 0 && x < src_width) {
      // Decode the compressed font data
      while (*pixels_to_go == 0) {
        if (!*col) {
          ++(*ptr);
        }
        *col = !*col;
        if (*col) {
          *pixels_to_go = (**ptr) >> 4;
        } else {
          *pixels_to_go = (**ptr) & 0xf;
        }
      }

      if (*col) {
        ++line_acc[p];
      }

      --(*pixels_to_go);
    }
    div += (lines + overspill);

    ++x;

    // Move on the divider
    c -= ra;

    // Do we output a pixel, either because of the divider (c)
    // or because this is the last row of pixels (count == 0)?
    if (c <= 0 || count == 0) {
      // Are we on the the last line we are looking at?
      if (count <= 1) {
        // Yes, so output a pixel
        if (line_acc[p] > 0) {
          hw_blend_pixel(dst_x, dst_y, (line_acc[p] << 8) / div, plot_col);
        }

        dst_x += pixel_xinc;
        dst_y += pixel_yinc;
      }

      if (x >= src_width) {
        // Move to the next line
        --count;

        // Reset for the next line
        p = 0;
        x = x_start;
        c = rb;
      } else {
        // Count up for the next pixel
        c += rb;
        ++p;
      }
      div = 0;
    }
  }
}

// Draw text on the canvas
int pte_draw_text(PteFontT *f, int x, int y, int r, const char *text, size_t size, int c) {
  uint32_t last_char = UINT32_MAX;
  size_t consumed = 0;
  const char *current_text = text;
  const PteBaseFont *bf = f->m_font;

  int pixel_xinc = 1;
  int pixel_yinc = 0;
  int line_xinc = 0;
  int line_yinc = 1;
  switch (r) {
    case 90:
      pixel_xinc = 0;
      pixel_yinc = 1;
      line_xinc = -1;
      line_yinc = 0;
      break;
    case 180:
      pixel_xinc = -1;
      pixel_yinc = 0;
      line_xinc = 0;
      line_yinc = -1;
      break;
    case 270:
      pixel_xinc = 0;
      pixel_yinc = -1;
      line_xinc = 1;
      line_yinc = 0;
      break;
    default:
      break;
  }

  x = (x * f->m_rb) / f->m_ra;
  y = (y * f->m_rb) / f->m_ra;
  while (*current_text != 0 && (size == static_cast<size_t>(-1) || consumed < size)) {
    size_t length = 0;
    uint32_t codepoint = extract_codepoint(current_text, &length);
    if (length == 0) {
      break;
    }
    current_text += length;
    consumed += length;

    const PteGlyph *g = find_char(codepoint, bf);
    // Bitblt this character across
    if (g != nullptr) {
      const PteKern *k;

      int acc = 0;
      int col = 1;
      int pixels_to_go = 0;
      int cy = 0;
      int last_cy = 0;
      int finished = 0;
      const unsigned char *ptr = bf->m_data + g->ptr;
      int offset_x;
      int sub_offset_x;
      int offset_y;
      int sub_offset_y;
      int sub_offset_dx;
      int sub_offset_dy;

      k = find_kern(last_char, codepoint, bf);
      if (k != nullptr) {
        x += k->amount * pixel_xinc;
        y += k->amount * pixel_yinc;
      }

      switch (r) {
        case 90:
          offset_y = ((y + g->xoffset) * f->m_ra) / f->m_rb;
          sub_offset_y = ((y + g->xoffset) * f->m_ra) % f->m_rb;
          offset_x = ((x + g->yoffset) * f->m_ra) / f->m_rb;
          sub_offset_x = ((x + g->yoffset) * f->m_ra) % f->m_rb;
          if (!sub_offset_x) {
            --offset_x;
          }
          break;
        case 180:
          offset_x = ((x - g->xoffset) * f->m_ra) / f->m_rb;
          sub_offset_x = f->m_rb - ((x - g->xoffset) * f->m_ra) % f->m_rb - 1;
          offset_y = ((y + g->yoffset) * f->m_ra) / f->m_rb;
          sub_offset_y = ((y + g->yoffset) * f->m_ra) % f->m_rb;
          if (sub_offset_x) {
            --offset_x;
          }
          if (sub_offset_y) {
            ++offset_y;
          }
          break;
        case 270:
          offset_x = ((x - g->yoffset) * f->m_ra) / f->m_rb;
          sub_offset_x = f->m_rb - ((x - g->yoffset) * f->m_ra) % f->m_rb - 1;
          offset_y = ((y - g->xoffset) * f->m_ra) / f->m_rb;
          sub_offset_y = f->m_rb - ((y - g->xoffset) * f->m_ra) % f->m_rb - 1;
          if (sub_offset_y) {
            --offset_y;
          }
          break;
        default:
          offset_x = ((x + g->xoffset) * f->m_ra) / f->m_rb;
          sub_offset_x = ((x + g->xoffset) * f->m_ra) % f->m_rb;
          offset_y = ((y - g->yoffset) * f->m_ra) / f->m_rb;
          sub_offset_y = f->m_rb - ((y - g->yoffset) * f->m_ra) % f->m_rb - 1;
          if (sub_offset_y) {
            --offset_y;
          }
          break;
      }

      switch (r) {
        case 270:
        case 90:
          sub_offset_dx = sub_offset_y;
          sub_offset_dy = sub_offset_x;
          break;
        default:
          sub_offset_dx = sub_offset_x;
          sub_offset_dy = sub_offset_y;
          break;
      }

      acc = f->m_rb;
      while (!finished) {
        acc -= f->m_ra;

        if (acc <= 0) {
          int lines = cy - last_cy;
          int overspill = 0;

          if (sub_offset_dy > 0) {
            overspill = cy - (sub_offset_dy * cy) / f->m_rb;
            lines -= overspill;
            cy -= overspill;
            sub_offset_dy = 0;
          } else if (cy >= g->height) {
            overspill = cy - g->height;
            lines -= overspill;
            finished = 1;
          }

          if (lines > 0) {
            const int destination_x = offset_x;
            const int destination_y = offset_y;
            const int subpixel_offset_x = sub_offset_dx;
            blt_horz_cmprs_resize(&ptr, &col, &pixels_to_go, g->width, destination_x, destination_y, pixel_xinc,
                                  pixel_yinc, f->m_ra, f->m_rb, subpixel_offset_x, lines, overspill, c);
          }
          offset_x += line_xinc;
          offset_y += line_yinc;

          last_cy = cy;
          acc += f->m_rb;
        }

        ++cy;
      }

      switch (r) {
        case 90:
          y += g->xadvance;
          break;
        case 180:
          x -= g->xadvance;
          break;
        case 270:
          y -= g->xadvance;
          break;
        default:
          x += g->xadvance;
          break;
      }

      last_char = codepoint;
    }
  }

  return (x * f->m_ra) / f->m_rb;
}

// How big will the text be?
void pte_measure_text(PteFontT *f, const char *text, size_t size, int *dx, int *dy) {
  uint32_t last_char = UINT32_MAX;
  size_t consumed = 0;
  const char *current_text = text;
  const PteBaseFont *bf = f->m_font;
  *dx = 0;
  while (*current_text != 0 && (size == static_cast<size_t>(-1) || consumed < size)) {
    size_t length = 0;
    uint32_t codepoint = extract_codepoint(current_text, &length);
    if (length == 0) {
      break;
    }
    current_text += length;
    consumed += length;

    const PteGlyph *g = find_char(codepoint, bf);
    if (g != nullptr) {
      const PteKern *k = find_kern(last_char, codepoint, bf);
      if (k != nullptr) {
        *dx += g->xadvance + k->amount;
      } else {
        *dx += g->xadvance;
      }

      last_char = codepoint;
    }
  }

  *dy = f->m_line_height;
  *dx = (*dx * f->m_ra) / f->m_rb;
}

// Centre the text in a rectangle
void pte_draw_text_rect(pte_Placement o, PteFontT *f, int x1, int y1, int x2, int y2, int r, const char *text,
                        size_t size, int c) {
  int dx;
  int dy;
  int x = 0;
  int y = 0;
  pte_measure_text(f, text, size, &dx, &dy);

  switch (r) {
    case 270:
    case 90: {
      int t = x1;
      x1 = y1;
      y1 = t;
      t = x2;
      x2 = y2;
      y2 = t;
      break;
    }
    default:
      break;
  }

  switch (o & 0xf) {
    case TEXT_VCENTER:
      x = ((x2 - x1) - dx) / 2 + x1;
      break;
    case TEXT_LEFT:
      x = x1;
      break;
    case TEXT_RIGHT:
      x = x2 - dx - 1;
      break;
    default:
      break;
  }

  switch (o & 0xf0) {
    case TEXT_HCENTER:
      y = ((y2 - y1) - f->m_baseline) / 2 + y1;
      break;
    case TEXT_TOP:
      y = y1;
      break;
    case TEXT_BOTTOM:
      y = y2 - f->m_baseline;
      break;
    default:
      break;
  }
  y += f->m_baseline;

  pte_draw_text(f, x, y, r, text, size, c);
}

void pte_draw_text_rect_wrapped(pte_Placement o, PteFontT *f, int x1, int y1, int x2, int y2, int r, const char *text,
                                size_t size, int c) {
  int rect_width = x2 - x1;
  int line_height = f->m_line_height;
  const char *word_start = text;
  const char *line_start = text;
  const char *word_end = text;
  int dx;
  int dy;

  while (*word_start && y1 < (y2 - line_height)) {
    word_end = word_start;
    while (*word_end && *word_end != ' ' && *word_end != '\n') {
      word_end++;
    }
    size_t prev_length = word_start - line_start;
    size_t line_length = word_end - line_start;

    pte_measure_text(f, line_start, line_length, &dx, &dy);
    if (dx > rect_width) {
      if (line_start == word_start) {
        // Single word is too long to fit in the line, force break
        word_start = word_end;
      } else {
        // Draw the current line and start a new one
        pte_draw_text_rect(o, f, x1, y1, x2, y2, r, line_start, prev_length, c);
        y1 += line_height;
        line_start = word_start;
        continue;
      }
    } else {
      word_start = word_end;
      if (*word_start == ' ') {
        word_start++;
      }
    }

    if (*word_start == '\n') {
      pte_draw_text_rect(o, f, x1, y1, x2, y2, r, line_start, line_length, c);
      y1 += line_height;
      word_start++;
      line_start = word_start;
    }
  }

  if (y1 < (y2 - line_height) && line_start < word_start) {
    size_t line_length = word_end - line_start;
    pte_draw_text_rect(o, f, x1, y1, x2, y2, r, line_start, line_length, c);
  }
}

// Get a font
PteFontT pte_get_font(const PteBaseFont *f, int size) {
  PteFontT result;
  result.m_ra = size;
  result.m_rb = f->m_size;
  result.m_font = f;
  result.m_line_height = (f->m_line_height * result.m_ra) / result.m_rb;
  result.m_baseline = (f->m_baseline * result.m_ra) / result.m_rb;

  return result;
}

}  // namespace esphome::font

#endif
