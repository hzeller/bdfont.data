Putting BDF into the `.data` segment
====================================

(Note, this a somewhat hacky tool I use for personal projects; provided AS-IS).

Simple tool to generate bitmap-fonts to be compiled into static data C-structs
from BDF fonts, including support functions to access them.

The focus is to generate a somewhat compact representation of fonts suited
for embedded systems with little flash memory. In particular larger fonts
benefit a lot from this.

### Compaction

The compaction techniques are a balance between space-savings and easy
decoding (computation and code-size) in microcontrollers:

  * Allow to only include the subset of characters you need.
  * No fixed array sizes per Glyph which allows variable length encoding.
  * Empty space around a character is not stored.
  * Automatic per-Glyph selection of best encoding from a choice of direct
    data byte storage and two different run length encodings.
  * Choice of two run length encodings
    1) One byte storing repetition-counts in nibbles, followed by
       the bytes that should be repeated `[c1|c0] [b0] [b1]`.
       Larger fonts with longer repetitions of the same byte benefit from
       this encoding. `c0` is always non-zero; if `c1` is zero, `[b1]` is
       omitted.
    2) One byte storing counts in two bits followed by the bytes to
       be repeated: `[c3|c2|c1|c0] [b0] [b1] [b2] [b3]`.
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
src/bdfont-data-gen -c "01234567890μHelloWorld" path/to/font.bdf myfontname
```

Optionally install with
```
sudo make -C src install
```

#### Invocation Synopsis
```
usage: bdfont-data-gen [options] [<bdf-file> <fontname>]
Options:
  -c <inc-chars>: Characters to include in font. UTF8-string.
  -C <char-file>: Read characters to include from file.
  -d <directory>: Output files to given directory instead of ./
  -b <baseline> : Choose fixed baseline. This allows choice of pixel-exact vertical
                  alignment at compile-time vs. need for shifting at runtime.
  -p            : Plain bytes. Don't use RLE compression. Might
                  make (data+code) smaller if you set -DBDFONT_USE_RLE=0 to
                  compile your project (typically only for tiny fonts)
  -s            : Create bdfont-support.{h,c} files.

To generate font-code, two parameters are required:
 <bdf-file>     : Path to the input BDF font file.
 <fontname>     : The generated font is named like this.
 With -c or -C, you can specify which characters are included.
(Otherwise all glyphs in font are included which likely not fits in flash)
This outputs font-$(fontname).h font-$(fontname).c
```

Invocation with `<bdf-file> <fontname> <relevantchars>` generates the files
`font-myfontname.{h,c}` into the directory chosen with `-d`.

You also need the files `bdfont-supprt.{h,c}` in the target project, which
provides run-time support to access the font. You can emit these with
the `-s` option. Make sure to re-create these files whenever the version
of `bdfont-data-gen` changes, to be compatible with the generated files.

At this point, the generated code is pretty specific to represent fonts in
'stripes', needed for SSD1306 type of displays: Each byte represents a vertical
set of 8 pixels which is a single pixel wide in X-direction. Thus it is pretty
easy to scroll text in X-direction; in y-direction only in
multiples of 8 unless bit-shifting is applied.
Fonts might need multiple of these stripes.
Future versions of `bdfont-data-gen` might also create horizontal bitmaps.

In some cases (with very small fonts and/or very few characters), it can be
that the space-savings from the RLE encoding is eaten up by the additional
code needed to decode it (~90 bytes on AVR). In that case, you can
specify `-p` (for 'plain bytes') to `bdfont-data-gen` (this will increase
the data size of the font) and use the `-DBDFONT_USE_RLE=0` define when
compiling your target program (this will decrease the code size of the
decoding).

The [runtime-support](./client-lib/bdfont-support.h) provides functions and
macros to access and decode the generated font.
Finding a glyph in the font is provided by `bdfont_find_glyph()`.
Decoding is done with either the `bdfont_emit_glyph()` function or the
`BDFONT_EMIT_GLYPH()` macro.

```c
/**
 * Emit the bytes for a glyph with the given basic plane unicode "codepoint"
 * Returns width that has been drawn or 0 if the character was not defined.
 *
 * This calls callbacks to two functions: one to start a new stripe, providing
 * information about which stripe and the expected width.
 * Then an EmitFun() call that emits a single byte at given x-position
 * representing 8 vertical pixels.
 *
 * Both functions get passed in some void pointer with user-data.
 *
 * Returns width of the character or 0 if it does not exist in the font.
 */
typedef void (*StartStripe)(uint8_t stripe, uint8_t width, void *userdata);
typedef void (*EmitFun)(uint8_t x, uint8_t bits, void *userdata);
uint8_t bdfont_emit_glyph(const struct FontData *font, uint16_t codepoint,
                          StartStripe start_stripe, EmitFun emit,
			  void *userdata);
```

There is also a version of the above that works with a macro call, which can
help reducing generated code overhad.
The macro approach allows for somewhat readable 'closure'-like code even in
plain C.
All the usual caveats with macros apply, in particular if you expand it
multiple times, you'll blow up the code-segment; applied thoughtfully, it can
result in more readable code that allows (on AVR) in the order of >100 bytes
space savings than the callback version.

Simple ASCII Example:
```c
int xpos = 0;
uint8_t *write_pos;
for (const char *txt = "Hello World"; *txt; ++txt) {
  xpos += BDFONT_EMIT_GLYPH(&font_foo, *txt, true,
                            { write_pos = framebuffer + stripe * 128 + xpos; },
                            { *write_pos++ = b; },
		            {});
}
```

The generated code works with Harvard arch AVR PROGMEM as well as von-Neumann
memory models (`#ifdef __AVR__` choses different code-variants).

Typically, you'd generate the font files by adding a rule to a Makefile. I
usually have the relevant chars in a file `<fontname>.chars` which is getting
updated whenever new characters are needed. In the following example, a
`font-smalltext.c` is generated from a file `smalltext.bdf` using the
characters in `smalltext.chars`. Whenever that set of chars is updated,
make automatically generates a new version of `font-smalltext.c`:


```Makefile
mybinary : mybinary.c font-smalltext.c

font-%.c: %.chars
	bdfont-data-gen -s $*.bdf $* -C $<
```