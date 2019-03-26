/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * This is part of http://github.com/hzeller/bdfont.data
 *
 * Copyright (C) 2019 Henner Zeller <h.zeller@acm.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

/**
 * Find glyph for given codepoint or NULL if it does not exist.
 * Note: on AVR, this returns a pointer to PROGMEM memory.
 */
const struct GlyphData *find_glyph(const struct FontData *font,
                                   int16_t codepoint);

/**
 * Emit the bytes for a glyph with the given basic plane unicode "codepoint"
 * Returns width that has been drawn or 0 if the character was not defined.
 *
 * This calls callbacks to two functions: one to start a new stripe, providing
 * information about which stripe and the expected width.
 * Then an EmitFun() call that emits a single byte at given x-position
 * representing 8 vertical pixels.
 * Both functions get passed in some void pointer with user-data.
 * TODO: function callbacks might not be the most code-space efficient on AVR,
 *       find some readable alternative.
 */
typedef void (*StartStripe)(uint8_t stripe, uint8_t width, void *userdata);
typedef void (*EmitFun)(uint8_t x, uint8_t bits, void *userdata);
uint8_t EmitGlyph(const struct FontData *font, uint16_t codepoint,
                  StartStripe start_stripe, EmitFun emit, void *userdata);

/* If this code is used in AVR, data is stuffed away into PROGMEM memory.
 * so needs to be dealt with specially
 */
#ifdef __AVR__
#  include <avr/pgmspace.h>
#  define _bdfont_data_get_bits(b) pgm_read_byte(b)
#  define _bdfont_data_unpack_memory(Type, variable)                    \
  Type _unpacked_##variable;                                            \
  memcpy_P(&_unpacked_##variable, variable, sizeof(_unpacked_##variable)); \
  variable = &_unpacked_##variable
#else
#  define PROGMEM
#  define _bdfont_data_get_bits(b) *b
#  define _bdfont_data_unpack_memory(Type, variable) do {} while(0)
#endif

/**
 * This is a macro version of the EmitGlyph() function call above.
 * Similar to that, it allows to pass in a "font" pointer, a
 * 16Bit-"codepoint" and a block to be called with access to the data.
 * The "start_stripe_call" and "emit_call" should be {}-braced blocks
 * of simple code to be executed for each start of a new stripe and
 * new byte (Note that the macro preprocessor might get confused with
 * too complicated {} blocks, so keep it simple).
 *
 * Within "start_stripe_call", there is a variable "stripe" and "glyph_width"
 * in scope.
 * Within "emit_call", a variable "x", "b" (the byte to be written) (as well
 * as "stripe" and "glyph_width").
 *
 * Simple Example (Iterating through an ASCII string and display):
   int xpos = 0;
   uint8_t *write_pos;
   for (const char *txt = "Hello World"; *txt; ++txt) {
     xpos += EMIT_GLYPH(&font_foo, *txt, 1,
                        { write_pos = framebuffer + stripe * 128 + xpos; },
                        { *write_pos++ = b; });
   }
 *
 * Why macro ? This is a pure C-compatible way that allows the compiler to see
 * more optimization opportunities (e.g. optimize out blocks of code if
 * "emit_empty_bytes" is set to false), so that in embedded devices a few
 * tens of bytes code-saving can happen.
 * For everything else, using the EmitGlyph() function is probably the more
 * sane way.
 *
 * Note1, the "emit_call" block is expanded multiple times, so keep it
 * short or make a function call.
 * Note2, this uses the gcc statement expression extension to return a value.
 */
#define EMIT_GLYPH(font, codepoint, emit_empty_bytes,                   \
                   start_stripe_call, emit_call) ({                     \
    int _return_glyph_width = 0;                     \
    const struct FontData *_font = (font);                              \
    _bdfont_data_unpack_memory(struct FontData, _font);                 \
    const struct GlyphData *_glyph = find_glyph(_font, codepoint);      \
    if (_glyph != NULL) {                                               \
      _bdfont_data_unpack_memory(struct GlyphData, _glyph);             \
      const uint8_t *_bits = _font->bits + _glyph->data_offset;         \
      const uint8_t _rle_mask = (_glyph->rle_type == 1) ? 0x0f : 0x03;  \
      const uint8_t _rle_shift = (_glyph->rle_type == 1) ? 4 : 2;       \
                                                                        \
      uint8_t _stripe;                                                  \
      uint8_t _x;                                                       \
      _return_glyph_width = _glyph->width;                              \
      const uint8_t glyph_width __attribute__((unused)) = _glyph->width; \
      for (_stripe = 0; _stripe < _font->stripes; ++_stripe) {          \
        const uint8_t stripe = _stripe;                                 \
        do { start_stripe_call } while(0); /* contain break/continue */ \
                                                                        \
        /* Empty data for empty stripes */                              \
        if (_stripe < _glyph->stripe_begin || _stripe >= _glyph->stripe_end) { \
          if (emit_empty_bytes) {                                       \
            for (_x = 0; _x < _glyph->width; ++_x) {                    \
              const uint8_t b = 0x00;                                   \
              const uint8_t __attribute__((unused)) x = _x;             \
              do { emit_call } while(0); /* contain break/continue */   \
            }                                                           \
          }                                                             \
          continue;                                                     \
        }                                                               \
                                                                        \
        /* Stripes with data */                                         \
        _x = 0;                                                         \
        while (_x < _glyph->width) {                                    \
          /* left and right margin are empty */                         \
          if (_x < _glyph->left_margin ||                               \
              _x >= _glyph->width - _glyph->right_margin) {             \
            if (emit_empty_bytes) {                                     \
              const uint8_t b = 0x00;                                   \
              const uint8_t __attribute__((unused)) x = _x;             \
              do { emit_call } while(0); /* contain break/continue */   \
            }                                                           \
            _x++;                                                       \
            continue;                                                   \
          }                                                             \
                                                                        \
          uint8_t _data_byte = _bdfont_data_get_bits(_bits++);          \
                                                                        \
          if (_glyph->rle_type == 0) {                                  \
            const uint8_t b = _data_byte;                               \
            const uint8_t x __attribute__((unused)) = _x;               \
            do { emit_call } while(0); /* contain break/continue */     \
            _x++;                                                       \
          } else {                                                      \
            uint8_t _rlcounts;                                          \
            for (_rlcounts = _data_byte; _rlcounts; _rlcounts >>= _rle_shift) { \
              uint8_t _repetition_count = _rlcounts & _rle_mask;        \
              _data_byte = _bdfont_data_get_bits(_bits++);              \
              while (_repetition_count--) {                             \
                const uint8_t b = _data_byte;                           \
                const uint8_t x __attribute__((unused)) = _x;           \
                do { emit_call } while(0); /* contain break/continue */ \
                _x++;                                                   \
              }                                                         \
            }                                                           \
          }                                                             \
        }                                                               \
      }                                                                 \
    }                                                                   \
    _return_glyph_width;                                                \
  })                                                                    \


#ifdef __cplusplus
}
#endif

#endif /* _BDFONT_DATA_FONT_SUPPORT_ */
