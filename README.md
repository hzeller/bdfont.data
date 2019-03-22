Putting BDF into the `.data` segment
====================================

(Note, this a somewhat hacky tool I use for personal projects; provided AS-IS).

Simple tool to generate bitmap-fonts stored in C-structs from BDF fonts to be
used in C-Programs.

The focus is to generate a somewhat compact representation of fonts suited
for embedded systems with little flash memory.

Fonts can have any Unicode characters from the Basic Multilingual Plane (the
first 16 bit), so it is easy to include special characters such as μ or π.
Because of the vast choice, you have to specify _which_ of these you'd like
to include into the compiled code to keep data compact. It is perfectly
ok to have 'sparse' fonts that only contain the characters you need for
your program strings.

```
make -C src
# Invoking tool. Multiple same characters are only included once.
src/generate-compiled-font path/to/font.bdf myfontname "01234567890μHelloWorld"
```

This generates a new files `font-myfontname.{h,c}`. Copy this together with
the runtime-support `client-lib/font-support.{h,c}` into your project and
compile there.

The `font-support` provides the runtime way to access the generated font.
Since the code-generation might evolve, font-support should be copied whenever
a font is generated to make sure it is compatible with that version.

At this point, the generated code is pretty specific to represent fonts in
'stripes', needed for SSD1306 type of displays.

The runtime-support currently is a pretty simple function `EmitGlyph()` that
expects callbacks with the bitmap data:

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