#pragma once

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

#include <stddef.h>
#include <stdint.h>

namespace esphome::font {

using pte_Placement = int;

inline constexpr pte_Placement TEXT_VCENTER = 0x0;
inline constexpr pte_Placement TEXT_LEFT = 0x1;
inline constexpr pte_Placement TEXT_RIGHT = 0x2;
inline constexpr pte_Placement TEXT_HCENTER = 0x0;
inline constexpr pte_Placement TEXT_TOP = 0x10;
inline constexpr pte_Placement TEXT_BOTTOM = 0x20;
inline constexpr pte_Placement TEXT_CENTER = 0;

struct PteGlyph {
  uint32_t code;
  int width;
  int height;
  int xoffset;
  int yoffset;
  int xadvance;
  int ptr;
};

struct PteKern {
  uint32_t first;
  uint32_t second;

  int amount;
};

struct PteBaseFont {
  // The size of the font
  int m_size;

  // The actual font data
  const unsigned char *m_data;

  // The glyph data
  int m_number_glyphs;
  const PteGlyph *m_gylphs;

  // The kerning data
  int m_number_kerns;
  const PteKern *m_kerns;

  // Placement
  int m_line_height;
  int m_baseline;
};

struct PteFontT {
  // The base font
  const PteBaseFont *m_font;

  // Resizing data
  int m_ra;
  int m_rb;

  // Placement (of resized font)
  int m_line_height;
  int m_baseline;
};

//
// Text drawing function
//

// Draw text at x,y (in pixels).
// Parameters:
//      font        - the font to use, create using the pte_get_font() function
//      x           - x position to start drawing the text
//      y           - y position of the font's baseline
//      r           - the rotation (0, 90, 180 and 270)
//      text        - the text to render
//      size        - the number of characters in "text" or -1 to read until the nul character
//      c           - the colour to draw the text, this is passed directly to the hw_blend_pixel function
//
int pte_draw_text(PteFontT *font, int x, int y, int r, const char *text, size_t size, int c);

// Draw text using a rectangle for positioning.  Note: the text is *not* clipped to the rectangle
// Parameters:
//      o               - the placement within the rectangle to draw the text, see pte_Placement
//      font            - the font to use, create using the pte_get_font() function
//      x1, y1, x2, y2  - the rectangle
//      r               - the rotation (0, 90, 180 and 270)
//      text            - the text to render
//      size            - the number of characters in "text" or -1 to read until the nul character
//      c               - the colour to draw the text, this is passed directly to the hw_blend_pixel function
//
void pte_draw_text_rect(pte_Placement o, PteFontT *f, int x1, int y1, int x2, int y2, int r, const char *text,
                        size_t size, int c);

// Draw text using a rectangle for positioning.  The text is wrapped to fit within the rectangle.
//      o               - the placement within the rectangle to draw the text, see pte_Placement
//      font            - the font to use, create using the pte_get_font() function
//      x1, y1, x2, y2  - the rectangle
//      r               - the rotation (0, 90, 180 and 270)
//      text            - the text to render
//      size            - the number of characters in "text" or -1 to read until the nul character
//      c               - the colour to draw the text, this is passed directly to the hw_blend_pixel function
//
void pte_draw_text_rect_wrapped(pte_Placement o, PteFontT *f, int x1, int y1, int x2, int y2, int r, const char *text,
                                size_t size, int c);

// Determine the bounding rectangle for a string in pixels
// Parameters:
//      font        - the font to use, create using the pte_get_font() function
//      text        - the text to measure
//      size        - the number of characters in "text" or -1 to read until the nul character
//      dx          - the width of the string in pixels
//      dy          - the height of the string in pixels
//
void pte_measure_text(PteFontT *f, const char *text, size_t size, int *dx, int *dy);

// Get a font
PteFontT pte_get_font(const PteBaseFont *f, int size);

// Interface to the hardware
void hw_blend_pixel(int x, int y, int a, int col);

}  // namespace esphome::font
