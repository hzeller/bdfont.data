Putting BDF into the `.data` segment
====================================

(Note, this a somewhat hacky tool I use for personal projects; provided AS-IS).

Simple tool to generate bitmap-fonts stored in static data C-structs
from BDF fonts.

The focus is to generate a somewhat compact representation of fonts suited
for embedded systems with little flash memory.

### Compaction

The compaction techniques are a balance between space-savings and easy
decoding in microcontrollers:

  * Allow to only include the subset of characters you need.
  * No fixed array sizes per Glyph which allows variable length encoding.
  * Empty space around a character is not stored.
  * Per-Glyph selection of optimal encoding between direct data byte storage
    and two different run length encodings.
  * Choice of two RLE encodings
    1) One byte storing repetition-counts in nibbles, followed by
       the bytes that should be repeated `[c1|c0] [b0] [b1]`.
       Larger fonts with longer repetitions of the same byte benefit from
       this encoding. `c0` is always non-zero; if `c1` is zero, `[b1]` is
       omitted.
    2) One byte storing counts in two bits `[c3|c2|c1|c0] [b0] [b1] [b2] [b3]`.
       `c0` is always non-zero. The top counts can be zero; for each
       zero `cn`, the corresponding `[bn]` is omitted.

### Unicode

Fonts can have any Unicode characters from the Basic Multilingual Plane (the
first 16 bit), so it is easy to include special characters such as μ or π.
Because of the vast choice, you have to specify _which_ characters you'd like
to include into the compiled code to keep data compact. It is perfectly
ok to have 'sparse' fonts that only contain the characters you need for
your program strings.

### Build and use
```bash
make -C src
# Invoking tool. Multiple instances of same character is only included once.
src/generate-compiled-font path/to/font.bdf myfontname "01234567890μHelloWorld"
```

#### Invocation Synopsis
```
usage: generate-compiled-font [options] <bdf-file> <fontname> <relevantchars>
Options:
  -b <baseline> : Choose fixed baseline. This allows choice of pixel-exact vertical
                  alignment at compile-time vs. need for shifting at runtime.

General parameters:
 <bdf-file>     : Path to the input BDF font file
 <fontname>     : The generated font is named like this
 <relevantchars>: A UTF8 string with all the characters that should be included in the font

ouputs font-$(fontname).h font-$(fontname).c
containting relevant characters
```

This generates the files `font-myfontname.{h,c}`. Copy this together with
the runtime-support `client-lib/font-support.{h,c}` into your project and
compile there. Write some adapting code to your screen using the provided
function.

The `font-support` provides the runtime way to access the generated font.
Since the code-generation might evolve, font-support should be copied whenever
a font is generated to make sure it is compatible with that version.

At this point, the generated code is pretty specific to represent fonts in
'stripes', needed for SSD1306 type of displays: Each byte represents a vertical
set of 8 pixels which is a single pixel wide in X-direction. Thus it is pretty
easy to scroll text in X-direction; in y-direction only in
multiples of 8 unless bit-shifting is applied.
Fonts might need multiple of these stripes.

The runtime-support currently is a pretty simple function `EmitGlyph()` that
expects callbacks that it calls with the bitmap data:

```c
/* Emit the bytes for a glyph with the given basic plane unicode "codepoint"
 * Returns width that has been drawn or 0 if the character was not defined.
 *
 * This calls callbacks to two functions: one to start a new stripe, providing
 * information about which stripe and the expected width. Then an EmitFun() call
 * that emits a single byte at given x-position representing 8 vertical pixels.
 * Both functions get passed in some void pointer with user-data.
 */
typedef void (*StartStripe)(uint8_t stripe, uint8_t width, void *userdata);
typedef void (*EmitFun)(uint8_t x, uint8_t bits, void *userdata);
uint8_t EmitGlyph(const struct FontData *font, uint16_t codepoint,
                  StartStripe start_stripe, EmitFun emit, void *userdata);
```

The generated code works with Harvard arch AVR PROGMEM as well as von-Neumann
memory models (`#ifdef __AVR__` choses different code-variants).