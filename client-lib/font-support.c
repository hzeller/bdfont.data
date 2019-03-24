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
  const uint8_t rle_mask = (glyph->rle_type == 1) ? 0x0f : 0x03;
  const uint8_t rle_shift = (glyph->rle_type == 1) ? 4 : 2;

  uint8_t stripe;
  uint8_t x;
  for (stripe = 0; stripe < font->stripes; ++stripe) {
    start_stripe(stripe, glyph->width, userdata);

    /* Empty data for empty stripes */
    if (stripe < glyph->stripe_begin || stripe >= glyph->stripe_end) {
      for (x = 0; x < glyph->width; ++x) emit(x, 0x00, userdata);
      continue;
    }

    /* Stripes with data */
    x = 0;
    while (x < glyph->width) {
      /* left and right margin are empty */
      if (x < glyph->left_margin || x >= glyph->width - glyph->right_margin) {
        emit(x++, 0x00, userdata);
        continue;
      }

#ifdef __AVR__
      uint8_t data_byte = pgm_read_byte(bits++);
#else
      uint8_t data_byte = *bits++;
#endif

      if (glyph->rle_type == 0) {
        emit(x++, data_byte, userdata);
      } else {
        uint8_t runlengths;
        for (runlengths = data_byte; runlengths; runlengths >>= rle_shift) {
          uint8_t repetition_count = runlengths & rle_mask;
#ifdef __AVR__
          const uint8_t data_byte = pgm_read_byte(bits++);
#else
          const uint8_t data_byte = *bits++;
#endif
          while (repetition_count--) {
            emit(x++, data_byte, userdata);
          }
        }
      }
    }
  }
  return glyph->width;
}
