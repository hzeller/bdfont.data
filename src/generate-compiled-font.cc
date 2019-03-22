
#include <assert.h>
#include <libgen.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include <string>
#include <set>
#include <vector>

#include "bdf-font.h"
#include "utf8-internal.h"
#include "font-support.h"

// params: 4x fontname
static constexpr char kHeaderTemplate[] =
    R"(/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * http://github.com/hzeller/bdfont.data
 * DO NOT EDIT; autogenerated by bdfont.data from %s
 */
#ifndef FONT_%s_
#define FONT_%s_

#include "font-support.h"

/* font containing %d characters */
extern const struct FontData PROGMEM font_%s;

#endif /* FONT_%s_ */
)";

static constexpr char kCodeHeader[] =
    R"(/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * http://github.com/hzeller/bdfont.data
 * DO NOT EDIT: autogenerated by bdfont.data from %s
 * for characters "%s"
 */

#include "font-%s.h"

)";

static int usage(const char *prog) {
    fprintf(stderr, "usage: %s <bdf-file> <fontname> <relevantchars>\n", prog);
    fprintf(stderr, "ouputs font-$(fontname).h font-$(fontname).c\n"
            "containting relevant characters\n");
    return 1;
}

class CollectFontMeta : public BitCanvas {
public:
    void SetPixel(int x, int y, bool) {
        if (x >= width_) width_ = x+1;
        if (y >= max_height_) max_height_ = y;
        if (y < min_height_) min_height_ = y;
        //printf("(%d,%d) h=%d\n", x, y, max_height_ - min_height_);
    }
    void NextChar() { count_++; }

    //int width() const { return width_; }
    int pages() const { return (max_height_ - min_height_ +8)/8; }
    int offset_y() const { return min_height_; }

private:
    int width_ = -1;
    int min_height_ = 1000;
    int max_height_ = -1;
    int count_ = 0;
};

class GlyphEmitter : protected BitCanvas {
    static constexpr int kMaxFontWidth = 64;  // limit of our bdf-font
public:
    GlyphEmitter(FILE *out,
                 int offset_y, int pages)
        : out_(out),
          offset_y_(offset_y), pages_(pages),
          data_(new uint8_t [kMaxFontWidth * pages_]), emitted_bytes_(0) {
        fprintf(out_, "static const uint8_t PROGMEM _font_data[] = {\n");

    }

    void Emit(const Font& font, uint32_t codepoint) {
        Reset(font.CharacterWidth(codepoint));
        font.DrawGlyph(this, 0, font.baseline(), false, codepoint);
        FinishChar(codepoint);
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
            fprintf(out_, " .width=%2d, "
                    ".page_offset = %d, .pages = %d, "
                    ".rle_type = %d, .data_offset = %4d},\n",
                    g.width,
                    g.page_offset, g.pages, g.rle_type, g.data_offset);
        }

        fprintf(out_, "};\n\n");
    }

protected:
    // Bitcanvas interface
    void SetPixel(int x, int y, bool on) final {
        if (!on) return;
        y -= offset_y_;
        assert(y >= 0);
        int page = y / 8;
        int bit = y % 8;
        if (page < current_glyph_.page_offset)
            current_glyph_.page_offset = page;
        if (page >= last_page_) last_page_ = page+1;
        data_[page * kMaxFontWidth + x] |= (1 << bit);
    }

    class Compressor {
    public:
        Compressor(int sections) : sections_(sections) {}
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
                                 "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,",
                                 encode_sections(),
                                 bytes[0], bytes[1], bytes[2], bytes[3]);
                emitted_bytes += 5;
            }
            else if (count[2] > 0) {
                if (out) fprintf(out, "0x%02x,0x%02x,0x%02x,0x%02x,",
                                 encode_sections(),
                                 bytes[0], bytes[1], bytes[2]);
                emitted_bytes += 4;
            }
            else if (count[1] > 0) {
                if (out) fprintf(out, "0x%02x,0x%02x,0x%02x,",
                                 encode_sections(), bytes[0], bytes[1]);
                emitted_bytes += 3;
            }
            else {
                if (out) fprintf(out, "0x%02x,0x%02x,",
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

    // RLE
    // Either encoded as 2 bytes or 3 bytes.
    // The first byte is divided into two nibbles: to top nibble shows
    // how many times the second byte is repeated. The lower nibble shows how
    // many times the third byte is repeated. If the lower nibble is 0, then
    // the third byte is skipped.
    void FinishChar(uint16_t codepoint) {
        current_glyph_.codepoint = codepoint;
        current_glyph_.data_offset = emitted_bytes_;
        if (last_page_ > 0) {
            current_glyph_.pages = last_page_ - current_glyph_.page_offset;
        }
        Compressor c2(2);
        Compressor c4(4);

        // First, let's test if RLE will bring us anything, and which is better
        const int baseline_bytes = current_glyph_.pages*current_glyph_.width;
        int c2_bytes = 0;
        int c4_bytes = 0;
        for (int p = 0; p < current_glyph_.pages; ++p) {
            for (int x = 0; x < current_glyph_.width; ++x) {
                uint8_t b = data_[(p+current_glyph_.page_offset)
                                  * kMaxFontWidth + x];
                c2.AddByte(NULL, b);
                c4.AddByte(NULL, b);
            }
            c2_bytes += c2.FinishLine(NULL);
            c4_bytes += c4.FinishLine(NULL);
        }

        const bool should_use_compression
            = std::min(c2_bytes, c4_bytes) < baseline_bytes;
        if (current_glyph_.codepoint < 0x80) {
            fprintf(out_, "/* codepoint '%s%c' %s */\n",
                    current_glyph_.codepoint == '\'' ? "\\" : "",
                    current_glyph_.codepoint,
                    should_use_compression ? "RLE" : "plain bytes");
        } else {
            fprintf(out_, "/* codepoint 0x%04x %s */\n",
                    current_glyph_.codepoint,
                    should_use_compression ? "RLE" : "plain bytes");
        }

        if (!should_use_compression) {
            current_glyph_.rle_type = 0;
            for (int p = 0; p < current_glyph_.pages; ++p) {
                for (int x = 0; x < current_glyph_.width; ++x) {
                    fprintf(out_, "0x%02x,",
                            data_[(p+current_glyph_.page_offset)
                                  * kMaxFontWidth + x]);
                    emitted_bytes_++;
                }
                fprintf(out_, "\n");
            }
        }
        else {
            Compressor *c = (c2_bytes < c4_bytes) ? &c2 : &c4;
            current_glyph_.rle_type = (c->sections() == 4) ? 1 : 2;
            for (int p = 0; p < current_glyph_.pages; ++p) {
                for (int x = 0; x < current_glyph_.width; ++x) {
                    c->AddByte(out_, data_[(p+current_glyph_.page_offset)
                                           * kMaxFontWidth + x]);
                }
                emitted_bytes_ += c->FinishLine(out_);
                fprintf(out_, "\n");
            }
        }
        glyphs_.push_back(current_glyph_);
    }

private:
    void Reset(int width) {
        bzero(data_, kMaxFontWidth * pages_);
        current_glyph_.width = width;
        current_glyph_.page_offset = pages_ - 1;
        current_glyph_.pages = 0;
        last_page_ = -1;
    }

    FILE *const out_;
    const int offset_y_;
    const int pages_;

    int last_page_;

    uint8_t *data_;
    int emitted_bytes_;
    GlyphData current_glyph_;
    std::vector<GlyphData> glyphs_;
};

int main(int argc, char *argv[]) {
    if (argc <= 3) return usage(argv[0]);

    const char *const bdf_font = argv[1];
    const char *const fontname = argv[2];
    const char *const utf8_text = argv[3];

    Font font;
    if (!font.LoadFont(bdf_font)) {
        fprintf(stderr, "Couldn't load font %s\n", bdf_font);
        return usage(argv[0]);
    }

    std::set<uint16_t> relevant_chars;
    for (const char *utfchars = utf8_text; *utfchars; /**/) {
        const uint32_t cp = utf8_next_codepoint(utfchars);
        relevant_chars.insert(cp);
    }

    if (relevant_chars.empty()) {
        fprintf(stderr, "relevant chars is empty?. Not creating output\n");
        return usage(argv[0]);
    }

    // Get some metadata.
    CollectFontMeta meta_collector;
    for (auto c : relevant_chars) {
        font.DrawGlyph(&meta_collector, 0, font.baseline(), false, c);
        meta_collector.NextChar();
    }

    const char *const font_file_basename = basename(strdup(bdf_font));

    // Generate header.
    std::string header_filename = std::string("font-") + fontname + ".h";
    FILE *header = fopen(header_filename.c_str(), "w");
    fprintf(header, kHeaderTemplate, font_file_basename,
            fontname, fontname, (int)relevant_chars.size(), fontname, fontname);
    fclose(header);

    // Generate code.

    // Glyphs and glyph data
    std::string code_filename = std::string("font-") + fontname + ".c";
    FILE *code_file = fopen(code_filename.c_str(), "w");
    fprintf(code_file, kCodeHeader, font_file_basename, utf8_text,
            fontname);
    GlyphEmitter glyph_emitter(code_file,
                               meta_collector.offset_y(),
                               meta_collector.pages());
    for (auto c : relevant_chars) {
        glyph_emitter.Emit(font, c);
    }

    glyph_emitter.EmitChars();

    // final font.
    fprintf(code_file, "const struct FontData PROGMEM font_%s = {\n"
            "  .available_glyphs = %d,\n"
            "  .pages = %d,\n"
            "  .bits = _font_data,\n"
            "  .glyphs = _font_glyphs\n};\n",
            fontname, (int)relevant_chars.size(),
            meta_collector.pages());
    fclose(code_file);
}
