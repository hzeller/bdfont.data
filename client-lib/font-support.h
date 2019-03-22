/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Henner Zeller <h.zeller@acm.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SSD1306_FONT_SUPPORT_
#define SSD1306_FONT_SUPPORT_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Treat these types as opaque types as they might change.
 * Only use the functions below to access the fonts.
 */
struct GlyphData {
  int16_t codepoint;        /* Unicode 16 code-point. */
  uint8_t width;            /* Individual width of this one. */
  uint8_t page_offset : 4;  /* Empty Y-pages skipped in data. */
  uint8_t pages : 4;        /* pages of data filled with data. */
  /* In total we use x_pixel * pages bytes starting from data_offset. */
  uint16_t data_offset;     /* Pointer into bits array. */
} __attribute__((packed));

struct FontData {
  uint16_t available_glyphs; /* Number of glyphs in this font. */
  uint8_t pages;             /* max height in 'pages', 8 bit stripes */
  const uint8_t *bits;
  const struct GlyphData *glyphs;
} __attribute__((packed));

/* If this code is used in AVR, data is stuffed away into PROGMEM memory. */
#ifdef __AVR__
#  include <avr/pgmspace.h>
#else
#  define PROGMEM
#endif

/* Emit the bytes for a glyph with the given basic plade unicode "codepoint"
 * Returns width that has been drawn or 0 if the character was not defined.
 * This calls callbacks to two functions: one to start a new stripe, providing
 * information about which stripe and the expected width. Then a single call
 * that emits a single byte representing 8 vertical pixels.
 * Both functions get passed in some opaque user-data.
 */
typedef void (*StartStripe)(uint8_t stripe, uint8_t width, void *userdata);
typedef void (*EmitFun)(uint8_t x, uint8_t bits, void *userdata);
uint8_t EmitGlyph(const struct FontData *font, uint16_t codepoint,
                  StartStripe start_stripe, EmitFun emit, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_FONT_SUPPORT_ */
