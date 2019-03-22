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

struct GlyphData {
  int16_t codepoint;        /* Unicode 16 code-point. */
  uint8_t width;            /* Individual width of this one. */
  uint8_t x_offset;         /* Where bytes start in x-direction. */
  uint8_t x_pixel;          /* How many pixels we use in X-direction. */
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

/* Find the GlyphData for the given "codepoint" in "font". If none exist,
 * returns NULL. Note: on AVR systems, this points to PROGMEM memory.
 */
const struct GlyphData *find_glyph(const struct FontData *font,
                                   int16_t codepoint);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_FONT_SUPPORT_ */
