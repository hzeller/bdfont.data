/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Henner Zeller <h.zeller@acm.org>
 * This is part of http://github.com/hzeller/bdfont.data
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

static const struct GlyphData *find_glyph(const struct FontData *font,
                                          int16_t codepoint) {
  return (const struct GlyphData*)
    (bsearch(&codepoint, font->glyphs, font->available_glyphs,
             sizeof(struct GlyphData), glyph_compare));
}

uint8_t EmitGlyph(const struct FontData *font, uint16_t codepoint,
                  StartStripe start_stripe, EmitFun emit, void *userdata) {
  /* TODO: Look at assembly to make smaller */
#ifdef __AVR__
  struct FontData unpacked_font;
  memcpy_P(&unpacked_font, font, sizeof(unpacked_font));
  font = &unpacked_font;
#endif
  const struct GlyphData *glyph = find_glyph(font, codepoint);
  if (glyph == NULL) return 0;
#ifdef __AVR__
  struct GlyphData unpacked_glyph;
  memcpy_P(&unpacked_glyph, glyph, sizeof(unpacked_glyph));
  glyph = &unpacked_glyph;
#endif
  const uint8_t *bits = font->bits + glyph->data_offset;
  uint8_t page = 0;
  uint8_t x = 0;
  /* Emit empty bits for offset pages */
  for (/**/; page < glyph->page_offset; ++page) {
    start_stripe(page, glyph->width, userdata);
    for (x = 0; x < glyph->width; ++x) emit(x, 0x00, userdata);
  }

  /* Pages with data */
  for (/**/; page < glyph->page_offset+glyph->pages; ++page) {
    start_stripe(page, glyph->width, userdata);
    x = 0;
    /* Left margin */
    for (/**/; x < glyph->left_margin; ++x) emit(x, 0x00, userdata);

    /* Meat of the data */
    if (glyph->rle_type == 0) {
      uint8_t data_w = glyph->width - glyph->left_margin - glyph->right_margin;
      for (/**/; x < data_w; ++x) {
#ifdef __AVR__
        uint8_t data_byte = pgm_read_byte(bits++);
#else
        uint8_t data_byte = *bits++;
#endif
        emit(x, data_byte, userdata);
      }
    }
    else {
      const uint8_t mask = (glyph->rle_type == 1) ? 0x03 : 0x0f;
      const uint8_t shift = (glyph->rle_type == 1) ? 2 : 4;
      while (x < glyph->width - glyph->right_margin) {
#ifdef __AVR__
        uint8_t runlengths = pgm_read_byte(bits++);
#else
        uint8_t runlengths = *bits++;
#endif
        while (runlengths) {
          uint8_t byte_count = runlengths & mask;
#ifdef __AVR__
          uint8_t data_byte = pgm_read_byte(bits++);
#else
          uint8_t data_byte = *bits++;
#endif
          uint8_t i;
          for (i = 0; i < byte_count; ++i, ++x) {
            emit(x, data_byte, userdata);
          }
          runlengths >>= shift;
        }
      }
    }

    /* Right margin. */
    for (/**/; x < glyph->width; ++x) emit(x, 0x00, userdata);
  }

  /* Remaining, empty pages */
  for (/**/; page < font->pages; ++page) {
    start_stripe(page, glyph->width, userdata);
    for (x = 0; x < glyph->width; ++x) emit(x, 0x00, userdata);
  }
  return glyph->width;
}
