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

#include "font-support.h"

#include <stdlib.h>

static int glyph_compare(const void *key, const void *element) {
#ifdef __AVR__
  /* Glyph pointer is pointing to PROGMEM. First two bytes are codepoint */
  int codepoint = pgm_read_word(element);
#else
  int codepoint = ((const struct GlyphData*)element)->codepoint;
#endif
  int search_codepoint = *(const uint16_t*)(key);
  return search_codepoint - codepoint;
}


const struct GlyphData *find_glyph(const struct FontData *font,
                                   int16_t codepoint) {
#ifdef __AVR__
  struct FontData unpacked_font;
  memcpy_P(&unpacked_font, font, sizeof(unpacked_font));
  font = &unpacked_font;
#endif
  return (const struct GlyphData*)
    (bsearch(&codepoint, font->glyphs, font->available_glyphs,
             sizeof(struct GlyphData), glyph_compare));
}
