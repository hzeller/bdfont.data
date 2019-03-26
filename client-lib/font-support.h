/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Henner Zeller <h.zeller@acm.org>
 *
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

#ifndef _BDFONT_DATA_FONT_SUPPORT_
#define _BDFONT_DATA_FONT_SUPPORT_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Treat these types as opaque types as they might change.
 * Only use the functions below to access the fonts.
 */
struct GlyphData {
  /* Public interface */
  uint16_t codepoint;        /* Unicode 16 code-point. */
  uint8_t width;             /* Individual width of this one. */

  /* Private interface, might change without notice */
  uint8_t left_margin  : 4;  /* Left empty space */
  uint8_t right_margin : 4;  /* Right empty space */
  uint8_t stripe_begin : 4;  /* Empty Y-stripes skipped in data. */
  uint8_t stripe_end   : 4;  /* end filled stripes */
  uint16_t data_offset : 14; /* Pointer into bits array. */
  uint8_t rle_type     : 2;  /* 0: none; 1: 2x4-bit count; 2: 4x2-bit count */
} __attribute__((packed));

struct FontData {
  /* Public interface */
  uint16_t available_glyphs; /* Number of glyphs in this font. */
  uint8_t baseline;          /* Position of baseline from rendering top */
  uint8_t stripes;           /* height in 8px high stripes. */

  /* Private interface, might change without notice */
  const uint8_t *bits;
  const struct GlyphData *glyphs;
} __attribute__((packed));

/* If this code is used in AVR, data is stuffed away into PROGMEM memory. */
#ifdef __AVR__
#  include <avr/pgmspace.h>
#else
#  define PROGMEM
#endif

/* Find glyph for given codepoint or NULL if it does not exist.
 * Note: on AVR, this returns a pointer to PROGMEM memory
 */
const struct GlyphData *find_glyph(const struct FontData *font,
                                   int16_t codepoint);

/* Emit the bytes for a glyph with the given basic plane unicode "codepoint"
 * Returns width that has been drawn or 0 if the character was not defined.
 *
 * This calls callbacks to two functions: one to start a new stripe, providing
 * information about which stripe and the expected width. Then an EmitFun() call
 * that emits a single byte at given x-position representing 8 vertical pixels.
 * Both functions get passed in some void pointer with user-data.
 * TODO: function callbacks might not be the most code-space efficient on AVR,
 *       find some readable alternative.
 */
typedef void (*StartStripe)(uint8_t stripe, uint8_t width, void *userdata);
typedef void (*EmitFun)(uint8_t x, uint8_t bits, void *userdata);
uint8_t EmitGlyph(const struct FontData *font, uint16_t codepoint,
                  StartStripe start_stripe, EmitFun emit, void *userdata);

#ifdef __AVR__
#  define _font_get_bits(b) pgm_read_byte(b)
#else
#  define _font_get_bits(b) *b
#endif

#ifdef __AVR__
#  define _unpack_memory(Type, variable)        \
  Type _unpacked_##variable;                                            \
  memcpy_P(&_unpacked_##variable, variable, sizeof(_unpacked_##variable)); \
  variable = &_unpacked_##variable
#else
#  define _unpack_memory(Type, variable) do {} while(0)
#endif

#define EMIT_GLYPH(font_in, codepoint, emit_empty_bytes, start_stripe, emit) { \
    const struct FontData *_font = (font_in);                           \
    _unpack_memory(struct FontData, _font);                             \
    const struct GlyphData *_glyph = find_glyph(_font, codepoint);      \
    if (_glyph == NULL) return 0;                                       \
    _unpack_memory(struct GlyphData, _glyph);                           \
    const uint8_t *_bits = _font->bits + _glyph->data_offset;           \
    const uint8_t _rle_mask = (_glyph->rle_type == 1) ? 0x0f : 0x03;    \
    const uint8_t _rle_shift = (_glyph->rle_type == 1) ? 4 : 2;         \
                                                                        \
    uint8_t _stripe;                                                    \
    uint8_t _x;                                                         \
    const uint8_t glyph_width = _glyph->width;                          \
    for (_stripe = 0; _stripe < _font->stripes; ++_stripe) {            \
      { const uint8_t stripe = _stripe;                                 \
        start_stripe }                                                  \
                                                                        \
      /* Empty data for empty stripes */                                \
      if (_stripe < _glyph->stripe_begin || _stripe >= _glyph->stripe_end) { \
        if (emit_empty_bytes) {                                         \
          for (_x = 0; _x < _glyph->width; ++_x) {                      \
            const uint8_t b = 0x00;                                     \
            const uint8_t x = _x;                                       \
            emit;                                                       \
          }                                                             \
        }                                                               \
        continue;                                                       \
      }                                                                 \
                                                                        \
      /* Stripes with data */                                           \
      _x = 0;                                                           \
      while (_x < _glyph->width) {                                      \
        /* left and right margin are empty */                           \
        if (_x < _glyph->left_margin ||                                 \
            _x >= _glyph->width - _glyph->right_margin) {               \
          if (emit_empty_bytes) {                                       \
            const uint8_t b = 0x00;                                     \
            const uint8_t x = _x;                                       \
            emit;                                                       \
          }                                                             \
          _x++;                                                         \
          continue;                                                     \
        }                                                               \
                                                                        \
        uint8_t _data_byte = _font_get_bits(_bits++);                   \
                                                                        \
        if (_glyph->rle_type == 0) {                                    \
          const uint8_t b = _data_byte;                                 \
          const uint8_t x = _x;                                         \
          emit;                                                         \
          _x++;                                                         \
        } else {                                                        \
          uint8_t _rlcounts;                                            \
          for (_rlcounts = _data_byte; _rlcounts; _rlcounts >>= _rle_shift) { \
            uint8_t _repetition_count = _rlcounts & _rle_mask;          \
            _data_byte = _font_get_bits(_bits++);                       \
            while (_repetition_count--) {                               \
              const uint8_t b = _data_byte;                             \
              const uint8_t x = _x;                                     \
              emit ;                                                    \
              _x++;                                                     \
            }                                                           \
          }                                                             \
        }                                                               \
      }                                                                 \
    }                                                                   \
  }                                                                     \

#ifdef __cplusplus
}
#endif

#endif /* _BDFONT_DATA_FONT_SUPPORT_ */
