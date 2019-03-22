// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Very simple graphics library to do simple things.
//
// Might be useful to consider using Cairo instead and just have an interface
// between that and the Canvas. Well, this is a quick set of things to get
// started (and nicely self-contained).
#ifndef BDF_FONT_H
#define BDF_FONT_H

#include <map>
#include <stdint.h>
#include "bitcanvas.h"

class Font {
public:
  // Initialize font, but it is only usable after LoadFont() has been called.
  Font();
  ~Font();

  bool LoadFont(const char *path);

  // Return height of font in pixels. Returns -1 if font has not been loaded.
  int height() const { return font_height_; }

  // Return baseline. Pixels from the topline to the baseline.
  int baseline() const { return base_line_; }

  // Return width of given character, or -1 if font is not loaded or character
  // does not exist.
  int CharacterWidth(uint32_t unicode_codepoint) const;

  // Draws the unicode character at position "x","y"
  // with "color" on "background_color" (background_color can be NULL for
  // transparency.
  // The "y" position is the baseline of the font.
  // If we don't have it in the font, draws the replacement character "�" if
  // available.
  // Returns how much we advance on the screen, which is the width of the
  // character or 0 if we didn't draw any chracter.
  int DrawGlyph(BitCanvas *c, int x, int y, bool inverse,
                uint32_t unicode_codepoint) const;

  // Create a new font derived from this font, which represents an outline
  // of the original font, essentially pixels tracing around the original
  // letter.
  // This can be used in situations in which it is desirable to frame a letter
  // in a different color to increase contrast.
  // The ownership of the returned pointer is passed to the caller.
  Font *CreateOutlineFont() const;

private:
  Font(const Font& x);  // No copy constructor. Use references or pointer instead.

  struct Glyph;
  typedef std::map<uint32_t, Glyph*> CodepointGlyphMap;

  const Glyph *FindGlyph(uint32_t codepoint) const;

  int font_height_;
  int base_line_;
  CodepointGlyphMap glyphs_;
};

// -- Some utility functions.

// Draw text, a standard NUL terminated C-string encoded in UTF-8,
// "kerning_offset" allows for additional spacing between characters (can be
// negative)
// Returns how many pixels we advanced on the screen.
int DrawText(BitCanvas *c, const Font &font, int x, int y,
             const char *utf8_text,
             bool inverse = false, int kerning_offset = 0);

#endif
