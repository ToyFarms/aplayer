/*
 * synced_lyrics.cpp
 *
 * Synced ("timestamped") lyrics live in different places depending on
 * format, and none of them are exposed through TagLib's plain C bindings
 * (tag_c.h only gives you the generic PropertyMap + a "PICTURE" complex
 * property). So this one piece of functionality talks to TagLib's C++
 * classes directly and exposes a small extern "C" function that
 * metadata.c calls into.
 *
 * Per-format handling:
 *
 *  - MP3 / ID3v2: read the native SYLT frame(s) via
 *    TagLib::ID3v2::SynchronizedLyricsFrame, which already stores
 *    millisecond timestamps -- no parsing needed, just copy them out.
 *
 *  - Ogg Vorbis / FLAC (Xiph comments): there's no dedicated synced-lyrics
 *    field in the Vorbis Comment spec. In practice taggers stuff an LRC
 *    ("[mm:ss.xx] line") formatted blob into a plain text field, commonly
 *    named LYRICS, SYNCEDLYRICS, or UNSYNCEDLYRICS. We scan those fields
 *    and parse LRC line-by-line if timestamps are present.
 *
 *  - MP4/M4A: iTunes stores lyrics in the "\xa9lyr" atom (plain text) or,
 *    for some tools (matches the TTML format your original Python's
 *    _build_ttml_format produces), a freeform
 *    "----:com.apple.iTunes:lyrics" atom containing TTML XML with
 *    <p begin="HH:MM:SS.mmm" end="...">text</p> elements. We check for
 *    TTML first, then fall back to LRC-in-plain-text on "\xa9lyr".
 *
 * Adding another format: add a branch below keyed off dynamic_cast on
 * TagLib::FileRef::file(), following the same pattern.
 */

#include "metagen.h"

#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/synchronizedlyricsframe.h>
#include <taglib/oggfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/flacfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/tpropertymap.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4item.h>
#include <taglib/tstring.h>
#include <taglib/tstringlist.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>

namespace {

struct RawLine {
    std::string text;
    int start_ms;
};

/* Parses "[mm:ss.xx]lyric text" style LRC lines. A single line may carry
 * more than one timestamp tag (repeated lines at different times), which
 * we expand into separate entries. Lines without any timestamp are
 * ignored -- if the whole blob has no timestamps at all, the caller
 * should treat it as plain unsynced lyrics instead (handled already by
 * the plain "LYRICS"/"\xa9lyr" path in metadata.c / this file).
 */
std::vector<RawLine> parse_lrc(const std::string &blob) {
    std::vector<RawLine> out;
    static const std::regex tag_re(R"(\[(\d{1,3}):(\d{2})(?:\.(\d{1,3}))?\])");

    std::istringstream stream(blob);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::vector<int> timestamps_ms;
        auto begin = std::sregex_iterator(line.begin(), line.end(), tag_re);
        auto end = std::sregex_iterator();
        size_t last_tag_end = 0;

        for (auto it = begin; it != end; ++it) {
            std::smatch m = *it;
            int minutes = std::atoi(m[1].str().c_str());
            int seconds = std::atoi(m[2].str().c_str());
            int frac_ms = 0;
            if (m[3].matched) {
                std::string frac = m[3].str();
                while (frac.size() < 3) frac += '0';
                frac_ms = std::atoi(frac.substr(0, 3).c_str());
            }
            timestamps_ms.push_back(minutes * 60000 + seconds * 1000 + frac_ms);
            last_tag_end = static_cast<size_t>(m.position(0) + m.length(0));
        }

        if (timestamps_ms.empty()) continue; /* not a synced line */

        std::string text = line.substr(last_tag_end);
        /* Trim leading whitespace left over after the timestamp tag(s) */
        size_t first_non_space = text.find_first_not_of(" \t");
        if (first_non_space != std::string::npos) text = text.substr(first_non_space);
        else text.clear();

        for (int ms : timestamps_ms) out.push_back({text, ms});
    }

    std::sort(out.begin(), out.end(),
              [](const RawLine &a, const RawLine &b) { return a.start_ms < b.start_ms; });
    return out;
}

/* Parses TTML <p begin="HH:MM:SS.mmm" end="...">text</p> elements, the
 * format produced by e.g. an Apple-Music-style lyrics exporter (and by
 * the _build_ttml_format() helper in the original Python code this
 * library mirrors). Nested markup inside <p> is stripped to plain text.
 */
int ttml_timestamp_to_ms(const std::string &ts) {
    int h = 0, m = 0; double s = 0;
    if (std::sscanf(ts.c_str(), "%d:%d:%lf", &h, &m, &s) == 3) {
        return h * 3600000 + m * 60000 + static_cast<int>(s * 1000 + 0.5);
    }
    /* also allow bare mm:ss.mmm */
    if (std::sscanf(ts.c_str(), "%d:%lf", &m, &s) == 2) {
        return m * 60000 + static_cast<int>(s * 1000 + 0.5);
    }
    return 0;
}

std::vector<RawLine> parse_ttml(const std::string &xml) {
    std::vector<RawLine> out;
    static const std::regex p_re(
        R"RX(<p\b[^>]*\bbegin="([^"]+)"[^>]*>(.*?)</p>)RX",
        std::regex::icase);
    static const std::regex tag_strip_re(R"(<[^>]*>)");

    auto begin = std::sregex_iterator(xml.begin(), xml.end(), p_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::smatch m = *it;
        int ms = ttml_timestamp_to_ms(m[1].str());
        std::string text = std::regex_replace(m[2].str(), tag_strip_re, "");
        out.push_back({text, ms});
    }
    return out;
}

bool looks_synced(const std::string &s) {
    static const std::regex tag_re(R"(\[\d{1,3}:\d{2}(?:\.\d{1,3})?\])");
    return std::regex_search(s, tag_re);
}

std::string to_std_string(const TagLib::String &s) {
    return s.to8Bit(true); /* UTF-8 */
}

/* -------------------- MP3 / ID3v2 -------------------- */
std::vector<RawLine> extract_from_mpeg(TagLib::MPEG::File &file) {
    std::vector<RawLine> out;
    TagLib::ID3v2::Tag *id3 = file.ID3v2Tag(false);
    if (!id3) return out;

    const TagLib::ID3v2::FrameList &frames = id3->frameList("SYLT");
    for (auto frame : frames) {
        auto *sylt = dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
        if (!sylt) continue;
        for (const auto &pair : sylt->synchedText()) {
            out.push_back({to_std_string(pair.text), static_cast<int>(pair.time)});
        }
    }
    std::sort(out.begin(), out.end(),
              [](const RawLine &a, const RawLine &b) { return a.start_ms < b.start_ms; });
    return out;
}

/* -------------------- Xiph comment (Ogg Vorbis / FLAC) -------------------- */
std::vector<RawLine> extract_from_xiph(TagLib::Ogg::XiphComment *xiph) {
    std::vector<RawLine> out;
    if (!xiph) return out;

    static const char *candidate_keys[] = {"SYNCEDLYRICS", "LYRICS", "UNSYNCEDLYRICS"};
    const TagLib::PropertyMap &props = xiph->properties();

    for (const char *key : candidate_keys) {
        auto it = props.find(key);
        if (it == props.end()) continue;
        for (const auto &value : it->second) {
            std::string text = to_std_string(value);
            if (!looks_synced(text)) continue;
            std::vector<RawLine> parsed = parse_lrc(text);
            out.insert(out.end(), parsed.begin(), parsed.end());
        }
        if (!out.empty()) break; /* first matching field wins */
    }
    return out;
}

/* -------------------- MP4 -------------------- */
std::vector<RawLine> extract_from_mp4(TagLib::MP4::File &file) {
    std::vector<RawLine> out;
    TagLib::MP4::Tag *tag = file.tag();
    if (!tag) return out;

    const TagLib::MP4::ItemMap &items = tag->itemMap();

    /* Preferred: TTML in the freeform "----:com.apple.iTunes:lyrics" atom.
     * Freeform atoms carry their own iTunes data-type flag; a "text" (UTF8)
     * atom comes back from TagLib as a StringList, but many taggers write
     * TTML lyrics with the "XML" data-type flag instead, which TagLib
     * exposes as a ByteVectorList of raw bytes -- so we check both. */
    auto ttml_it = items.find("----:com.apple.iTunes:lyrics");
    if (ttml_it != items.end()) {
        TagLib::StringList sl = ttml_it->second.toStringList();
        if (!sl.isEmpty()) {
            out = parse_ttml(to_std_string(sl.front()));
        } else {
            TagLib::ByteVectorList bvl = ttml_it->second.toByteVectorList();
            if (!bvl.isEmpty()) {
                const TagLib::ByteVector &bv = bvl.front();
                out = parse_ttml(std::string(bv.data(), bv.size()));
            }
        }
    }

    /* Fallback: LRC-style text crammed into the standard "\xa9lyr" atom */
    if (out.empty()) {
        auto lyr_it = items.find("\xC2\xA9lyr"); /* UTF-8 for U+00A9 'lyr' */
        if (lyr_it == items.end()) lyr_it = items.find("\251lyr");
        if (lyr_it != items.end()) {
            TagLib::StringList sl = lyr_it->second.toStringList();
            if (!sl.isEmpty()) {
                std::string text = to_std_string(sl.front());
                if (looks_synced(text)) out = parse_lrc(text);
            }
        }
    }

    return out;
}

} /* anonymous namespace */

extern "C" int extract_synced_lyrics(const char *path,
                                           synced_lyric_line_t **out_lines,
                                           size_t *out_count) {
    *out_lines = nullptr;
    *out_count = 0;

    TagLib::FileRef ref(path);
    if (ref.isNull() || !ref.file()) return -1;

    std::vector<RawLine> lines;

    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(ref.file())) {
        lines = extract_from_mpeg(*mpeg);
    } else if (auto *vorbis = dynamic_cast<TagLib::Ogg::Vorbis::File *>(ref.file())) {
        lines = extract_from_xiph(vorbis->tag());
    } else if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(ref.file())) {
        lines = extract_from_xiph(flac->xiphComment(false));
    } else if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(ref.file())) {
        lines = extract_from_mp4(*mp4);
    }
    /* Formats without a modeled synced-lyrics source (WAV/RIFF INFO, APE,
     * etc.) simply fall through with an empty result. */

    if (lines.empty()) return 0;

    auto *out = static_cast<synced_lyric_line_t *>(
        std::malloc(lines.size() * sizeof(synced_lyric_line_t)));
    if (!out) return -1;

    for (size_t i = 0; i < lines.size(); ++i) {
        out[i].text = strdup(lines[i].text.c_str());
        out[i].start_ms = lines[i].start_ms;
    }

    *out_lines = out;
    *out_count = lines.size();
    return 0;
}

extern "C" void free_synced_lyric_lines(synced_lyric_line_t *lines, size_t count) {
    if (!lines) return;
    for (size_t i = 0; i < count; ++i) std::free(lines[i].text);
    std::free(lines);
}
