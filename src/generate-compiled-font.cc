/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include "bdf-font.h"
#include "bdfont-support.h"
#include "utf8-internal.h"

#include "font-support-str.inc"  // Verbatim bdfont-support.{h,cc}

// params: 4x fontname
static constexpr char kHeaderTemplate[] =
  R"(/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * DO NOT EDIT; autogenerated by http://github.com/hzeller/bdfont.data
 * Font-File: %s
 *      Font: %s
 *      Size: %d   Baseline at: %d
 * For chars: %s
 */
#ifndef FONT_%s_
#define FONT_%s_

#include "bdfont-support.h"

/* font containing %d characters */
extern const struct FontData PROGMEM font_%s;

#endif /* FONT_%s_ */
)";

static constexpr char kCodeHeader[] =
  R"(/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * DO NOT EDIT; autogenerated by http://github.com/hzeller/bdfont.data
 * Font-File: %s
 *      Font: %s
 *      Size: %d   Baseline at: %d
 * For chars: %s
 */

#include "font-%s.h"

)";

static int usage(const char *prog) {
  fprintf(stderr, "usage: %s [options] [<bdf-file> <fontname>]\n", prog);
  fprintf(stderr, "Options:\n"
          "  -c <inc-chars>: Characters to include in font. UTF8-string.\n"
          "  -C <char-file>: Read characters to include from file.\n"
          "  -d <directory>: Output files to given directory instead of ./\n"
          "  -b <baseline> : Choose fixed baseline. This allows "
          "choice of pixel-exact vertical\n"
          "                  alignment at compile-time vs. need for "
          "shifting at runtime.\n"
          "  -s            : Create bdfont-support.{h,c} files.\n"
          "\n");
  fprintf(stderr, "To generate font-code, two parameters are required:\n"
          " <bdf-file>     : Path to the input BDF font file.\n"
          " <fontname>     : The generated font is named like this.\n"
          " With -c or -C, you can specify which characters are included.\n"
          "(Otherwise all glyphs in font are included which likely not fits in"
          " flash)\n");
  fprintf(stderr, "This outputs font-$(fontname).h font-$(fontname).c\n");
  return 1;
}

class RLECompressor {
public:
  RLECompressor(int sections) : sections_(sections) {}
  int sections() const { return sections_; }

  void AddByte(FILE *out, uint8_t b) {
    if (count[current_section] == 0) {
      bytes[current_section] = b;
      count[current_section] = 1;
    }
    else if (bytes[current_section] == b && count[current_section] < section_capacity()) {
      count[current_section]++;
    }
    else {
      current_section++;
      if (current_section >= sections_) Emit(out);
      bytes[current_section] = b;
      count[current_section] = 1;
    }
  }

  int FinishLine(FILE *out) {
    Emit(out);
    int ret = emitted_bytes;
    emitted_bytes = 0;
    return ret;
  }

  void Emit(FILE *out) {
    if (count[3] > 0) {
      if (out) fprintf(out,
                       "  0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,",
                       encode_sections(),
                       bytes[0], bytes[1], bytes[2], bytes[3]);
      emitted_bytes += 5;
    }
    else if (count[2] > 0) {
      if (out) fprintf(out, "  0x%02x,0x%02x,0x%02x,0x%02x,",
                       encode_sections(),
                       bytes[0], bytes[1], bytes[2]);
      emitted_bytes += 4;
    }
    else if (count[1] > 0) {
      if (out) fprintf(out, "  0x%02x,0x%02x,0x%02x,",
                       encode_sections(), bytes[0], bytes[1]);
      emitted_bytes += 3;
    }
    else {
      if (out) fprintf(out, "  0x%02x,0x%02x,",
                       encode_sections(), bytes[0]);
      emitted_bytes += 2;
    }
    count[0] = count[1] = count[2] = count[3] = 0;
    current_section = 0;
  }

private:
  int section_capacity() {
    switch (sections_) {
    case 2: return 0x0f;
    case 4: return 0x03;
    default:
      assert(false);
    }
    return 0;
  }
  uint8_t encode_sections() {
    switch (sections_) {
    case 2: return count[1] << 4 | count[0];
    case 4: return (count[3] << 6 | count[2] << 4
                    | count[1] << 2 | count[0]);
    }
    return 0;
  }

  const int sections_;
  uint8_t current_section = 0;
  uint8_t count[4] = {};
  uint8_t bytes[4];
  int emitted_bytes = 0;
};

class CollectFontMeta : public BitCanvas {
public:
  CollectFontMeta(bool fixed_baseline, int chosen_min)
    : fixed_baseline_(fixed_baseline),
      min_height_(fixed_baseline ? chosen_min : 1000) {
  }

  void SetPixel(int x, int y, bool) {
    if (y >= max_height_) max_height_ = y;
    if (fixed_baseline_) {
      baseline_satisfied_ &= (y >= min_height_);
    } else if (y < min_height_) {
      min_height_ = y;
    }
  }
  void NextChar() { count_++; }

  int stripes() const { return (max_height_ - min_height_ +8)/8; }
  int offset_y() const { return min_height_; }
  bool baseline_satisfied() const { return baseline_satisfied_; }

private:
  const bool fixed_baseline_;
  bool baseline_satisfied_ = true;
  int min_height_ = 1000;
  int max_height_ = -1;
  int count_ = 0;
};

class GlyphEmitter : protected BitCanvas {
  static constexpr int kMaxFontWidth = 64;  // limit of our bdf-font
public:
  GlyphEmitter(FILE *out,
               int offset_y, int stripes)
    : out_(out),
      offset_y_(offset_y), stripes_(stripes),
      data_(new uint8_t [kMaxFontWidth * stripes_]), emitted_bytes_(0) {
    fprintf(out_, "static const uint8_t PROGMEM _font_data[] = {");
  }

  bool Emit(const Font& font, uint32_t codepoint) {
    Reset(font.CharacterWidth(codepoint));
    font.DrawGlyph(this, 0, font.baseline(), false, codepoint);
    return FinishChar(codepoint);
  }

  void EmitChars() {
    fprintf(out_, "};\n\n");

    fprintf(out_,
            "static const struct GlyphData PROGMEM _font_glyphs[] = {\n");
    for (const auto &g : glyphs_) {
      if (g.codepoint < 0x80) {
        fprintf(out_, "  {.codepoint = '%s%c',    ",
                g.codepoint == '\'' ? "\\" : "", g.codepoint);
      } else {
        fprintf(out_, "  {.codepoint = 0x%04x, ",
                g.codepoint);
      }
      fprintf(out_, ".width=%2d, "
              ".stripe_begin=%d, .stripe_end=%d, "
              ".left_margin=%d, .right_margin=%d, "
              ".rle_type=%d, .data_offset=%4d},\n",
              g.width,
              g.stripe_begin, g.stripe_end, g.left_margin, g.right_margin,
              g.rle_type, g.data_offset);
    }

    fprintf(out_, "};\n\n");
  }

protected:
  // Bitcanvas interface
  void SetPixel(int x, int y, bool on) final {
    if (!on) return;
    y -= offset_y_;
    assert(y >= 0);  // Otherwise the offset has been calculated wrong
    int stripe = y / 8;
    int bit = y % 8;
    if (stripe < g_.stripe_begin)
      g_.stripe_begin = stripe;
    if (stripe >= last_stripe_) last_stripe_ = stripe+1;
    if (x < min_x_) min_x_ = x;
    if (x > max_x_) max_x_ = x;
    data_[stripe * kMaxFontWidth + x] |= (1 << bit);
  }

  // RLE
  // Either encoded as 2 bytes or 3 bytes.
  // The first byte is divided into two nibbles: to top nibble shows
  // how many times the second byte is repeated. The lower nibble shows how
  // many times the third byte is repeated. If the lower nibble is 0, then
  // the third byte is skipped.
  bool FinishChar(uint16_t codepoint) {
    // Since this is meant for embedded devices, we have limited the
    // size of the variable holding the offsets (bdfont-support.h). Make
    // sure we don't go over this (which can be if -c or -C are omitted).
    constexpr int kMaxFontOffsetCapacityBits = 14;
    if (emitted_bytes_ > (2 << kMaxFontOffsetCapacityBits)) {
      fprintf(stderr, "Reached capacity: GlyphData::font_offset uses more "
              "than %d bits\n", kMaxFontOffsetCapacityBits);
      fprintf(stderr, "Please limit characters to include with -c or -C\n");
      return false;
    }
    g_.codepoint = codepoint;
    g_.data_offset = emitted_bytes_;
    if (last_stripe_ > 0) {
      g_.stripe_end = last_stripe_;
    }
    if (g_.stripe_end > 0) {
      g_.left_margin = (min_x_ <= 0xf) ? min_x_ : 0xf;
      int right_margin = g_.width - max_x_ - 1;
      g_.right_margin = (right_margin <= 0xf) ? right_margin : 0xf;
    }

    RLECompressor c2(2);
    RLECompressor c4(4);

    // First, let's test if RLE will bring us anything, and which is better
    const int baseline_bytes = (g_.stripe_end - g_.stripe_begin)*g_.width;
    int c2_bytes = 0;
    int c4_bytes = 0;
    int x;
    for (int p = g_.stripe_begin; p < g_.stripe_end; ++p) {
      for (x = g_.left_margin; x < g_.width-g_.right_margin; ++x) {
        uint8_t b = data_[p * kMaxFontWidth + x];
        c2.AddByte(NULL, b);
        c4.AddByte(NULL, b);
      }
      c2_bytes += c2.FinishLine(NULL);
      c4_bytes += c4.FinishLine(NULL);
    }

    const bool should_use_compression
      = std::min(c2_bytes, c4_bytes) < baseline_bytes;
    // Only if 4-section compressor is better, use that; otherwise nibble
    // decompression is easier to do in head when reading data.
    RLECompressor *const best_compress = (c4_bytes < c2_bytes) ? &c4 : &c2;

    if (g_.codepoint < 0x80) {
      fprintf(out_, "\n  /* codepoint '%s%c' %s */\n",
              g_.codepoint == '\'' ? "\\" : "",
              g_.codepoint,
              should_use_compression
              ? (best_compress->sections() == 4 ? "RLE/4" : "RLE/nibble")
              : "plain bytes");
    } else {
      fprintf(out_, "\n  /* codepoint U+%04x %s */\n",
              g_.codepoint,
              should_use_compression
              ? (best_compress->sections() == 4 ? "RLE/4" : "RLE/nibble")
              : "plain bytes");
    }

    if (!should_use_compression) {
      g_.rle_type = 0;
      for (int p = g_.stripe_begin; p < g_.stripe_end; ++p) {
        fprintf(out_, "  ");
        for (x = g_.left_margin; x < g_.width - g_.right_margin; ++x) {
          fprintf(out_, "0x%02x,", data_[p * kMaxFontWidth + x]);
          emitted_bytes_++;
        }
        fprintf(out_, "\n");
      }
    }
    else {
      g_.rle_type = (best_compress->sections() == 4) ? 2 : 1;
      for (int p = g_.stripe_begin; p < g_.stripe_end; ++p) {
        for (x = g_.left_margin; x < g_.width - g_.right_margin; ++x) {
          best_compress->AddByte(out_, data_[p * kMaxFontWidth + x]);
        }
        emitted_bytes_ += best_compress->FinishLine(out_);
        fprintf(out_, "\n");
      }
    }
    glyphs_.push_back(g_);
    return true;
  }

private:
  void Reset(int width) {
    bzero(data_, kMaxFontWidth * stripes_);
    g_.width = width;
    g_.stripe_begin = stripes_ - 1;
    g_.stripe_end = 0;
    g_.left_margin = g_.right_margin = 0;
    last_stripe_ = -1;
    min_x_ = width;
    max_x_ = 0;
  }

  FILE *const out_;
  const int offset_y_;
  const int stripes_;

  int last_stripe_;
  int min_x_, max_x_;

  uint8_t *data_;
  int emitted_bytes_;
  GlyphData g_;
  std::vector<GlyphData> glyphs_;
};

static bool GenerateFontFile(const char *bdf_font, const char *fontname,
                             const std::string& utf8_characters,
                             int chosen_baseline,
                             const std::string& directory) {
  Font font;
  if (!font.LoadFont(bdf_font)) {
    fprintf(stderr, "Couldn't load font %s\n", bdf_font);
    return false;
  }

  std::set<uint16_t> relevant_chars;
  if (utf8_characters.empty()) {
    font.GetCodepoints(&relevant_chars);
  } else {
    for (const char *utfchars = utf8_characters.c_str(); *utfchars; /**/) {
      const uint32_t cp = utf8_next_codepoint(utfchars);
      if (font.CharacterWidth(cp) < 0) {
        fprintf(stderr, "Excluding codepoint U+%04x, which is not "
                "included in font\n", cp);
        continue;
      }
      relevant_chars.insert(cp);
    }
  }

  if (relevant_chars.empty()) {
    fprintf(stderr, "relevant chars is empty?. Not creating output\n");
    return false;
  }

  // Get some metadata.
  CollectFontMeta meta_collector(chosen_baseline > -1,
                                 font.baseline() - chosen_baseline);
  for (auto c : relevant_chars) {
    font.DrawGlyph(&meta_collector, 0, font.baseline(), false, c);
    meta_collector.NextChar();
  }

  if (!meta_collector.baseline_satisfied()) {
    fprintf(stderr, "Could not satisfy requested baseline %d: "
            "characters would be truncated at the top!\n", chosen_baseline);
    return false;
  }

  const char *const font_file_basename = basename(strdup(bdf_font));

  const char *const display_chars =
    utf8_characters.empty()
    ? "(all characters from font)"
    : (utf8_characters.size() < 120) ? utf8_characters.c_str() : "(large list)";

  // Generate header.
  std::string header_filename = directory + "/font-" + fontname + ".h";
  FILE *header_file = fopen(header_filename.c_str(), "w");
  if (header_file == nullptr) {
    perror(header_filename.c_str());
    return false;
  }
  fprintf(header_file, kHeaderTemplate, font_file_basename,
          font.fontname().c_str(),
          font.height(), font.baseline() - meta_collector.offset_y(),
          display_chars,
          fontname, fontname,
          (int)relevant_chars.size(),
          fontname, fontname);
  fclose(header_file);

  // Generate code.

  // Glyphs and glyph data
  std::string code_filename = directory + "/font-" + fontname + ".c";
  FILE *code_file = fopen(code_filename.c_str(), "w");
  if (code_file == nullptr) {
    perror(code_filename.c_str());
    return false;
  }
  fprintf(code_file, kCodeHeader, font_file_basename,
          font.fontname().c_str(),
          font.height(), font.baseline() - meta_collector.offset_y(),
          display_chars,
          fontname);
  GlyphEmitter glyph_emitter(code_file,
                             meta_collector.offset_y(),
                             meta_collector.stripes());
  for (auto c : relevant_chars) {
    if (!glyph_emitter.Emit(font, c))
      return false;
  }

  glyph_emitter.EmitChars();

  // final font.
  fprintf(code_file, "const struct FontData PROGMEM font_%s = {\n"
          "  .available_glyphs = %d,\n"
          "  .baseline = %d,\n"
          "  .stripes = %d,\n"
          "  .bits = _font_data,\n"
          "  .glyphs = _font_glyphs\n};\n",
          fontname, (int)relevant_chars.size(),
          font.baseline() - meta_collector.offset_y(),
          meta_collector.stripes());
  fclose(code_file);

  return true;
}

static bool WriteFile(const std::string& directory, const char *name,
                      const char *str) {
  std::string code_filename = directory + "/" + name;
  FILE *code_file = fopen(code_filename.c_str(), "w");
  if (code_file == nullptr) {
    perror(code_filename.c_str());
    return false;
  }
  fwrite(str, 1, strlen(str), code_file);
  fclose(code_file);
  return true;
}

static bool GenerateSupportFiles(const std::string& dir) {
  if (!WriteFile(dir, "bdfont-support.h", bdfont_support_h))
    return false;
  if (!WriteFile(dir, "bdfont-support.c", bdfont_support_c))
    return false;
  return true;
}

static bool ReadFileIntoString(const char *filename, std::string *out) {
  char buffer[1024];
  int fd = open(filename, O_RDONLY);
  if (fd < 0) { perror(filename); return false; };
  int r;
  while ((r = read(fd, buffer, sizeof(buffer))) > 0) {
    out->append(buffer, r);
  }
  // If there is a newline at the very end, it was probably not meant to be
  // there. Still possible to include a \n if it is in the middle of text.
  if (out->at(out->size()-1) == '\n')
    out->resize(out->size()-1);
  return true;
}

int main(int argc, char *argv[]) {
  int chosen_baseline = -1;
  std::string directory = ".";
  bool create_support_files = false;
  std::string relevant_chars;

  int opt;
  while ((opt = getopt(argc, argv, "b:d:sc:C:")) != -1) {
    switch (opt) {
    case 'b':
      chosen_baseline = atoi(optarg);
      break;
    case 'd':
      directory = optarg;
      break;
    case 's':
      create_support_files = true;
      break;
    case 'c':
      relevant_chars = optarg;
      break;
    case 'C':
      if (!ReadFileIntoString(optarg, &relevant_chars))
        return usage(argv[0]);
      break;
      // TODO: provide regex-pattern to include relevant chars "[0-9][a-zA-Z]"
    default:
      return usage(argv[0]);
    }
  }

  int any_operation = 0;
  if (create_support_files) {
    ++any_operation;
    if (!GenerateSupportFiles(directory)) return 1;
  }
  if (argc - optind > 0 || !relevant_chars.empty()) {
    ++any_operation;
    if (argc - optind != 2) {
      fprintf(stderr, "Expected bdf-file and fontname\n");
      return usage(argv[0]);
    }
    if (!GenerateFontFile(argv[optind], argv[optind+1], relevant_chars,
                          chosen_baseline, directory)) {
      return 1;
    }
  }

  if (!any_operation) {
    fprintf(stderr, "No operation selected.\n"
            " * Supply font-parameters to create compilable font files\n"
            " * Use -s to generate support files\n"
            );
    return usage(argv[0]);
  }
  return 0;
}
