#include "metagen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#  include <strings.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <audioproperties.h>
#include <fileref.h>
#include <tag.h>
#include <tbytevector.h>
#include <tfile.h>
#include <tpropertymap.h>
#include <tstring.h>
#include <tstringlist.h>
#include <tvariant.h>

#include <flacfile.h>
#include <id3v2frame.h>
#include <id3v2tag.h>
#include <mp4file.h>
#include <mp4item.h>
#include <mp4tag.h>
#include <mpegfile.h>
#include <oggfile.h>
#include <synchronizedlyricsframe.h>
#include <vorbisfile.h>
#include <xiphcomment.h>

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>

static char *xstrdup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *out = (char *)malloc(n);
    if (out)
        memcpy(out, s, n);
    return out;
}

static int key_is(const char *key, const char *name)
{
#if defined(_WIN32)
    return _stricmp(key, name) == 0;
#else
    return strcasecmp(key, name) == 0;
#endif
}

static double parse_double_or(const char *s, int *ok)
{
    if (!s || !*s)
    {
        *ok = 0;
        return 0.0;
    }
    char *end = NULL;
    double v = strtod(s, &end);
    *ok = (end != s);
    return v;
}

static std::string to_std_string(const TagLib::String &s)
{
    return s.to8Bit(true);
}

static char *dup_string(const TagLib::String &s)
{
    return xstrdup(to_std_string(s).c_str());
}

static char *join_string_list(const TagLib::StringList &values)
{
    if (values.isEmpty())
        return NULL;

    std::string out;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
            out += "; ";
        out += to_std_string(values[i]);
    }
    return xstrdup(out.c_str());
}

static void extract_pictures(TagLib::File *tfile, metadata_t *meta)
{
    TagLib::List<TagLib::VariantMap> pics = tfile->complexProperties("PICTURE");
    size_t n = pics.size();
    if (n == 0)
        return;

    picture_t *out = (picture_t *)calloc(n, sizeof(picture_t));
    if (!out)
        return;

    size_t i = 0;
    for (TagLib::List<TagLib::VariantMap>::ConstIterator pit = pics.begin();
         pit != pics.end(); ++pit, ++i)
    {
        const TagLib::VariantMap &attrs = *pit;
        for (TagLib::VariantMap::ConstIterator ait = attrs.begin();
             ait != attrs.end(); ++ait)
        {
            std::string key = to_std_string(ait->first);
            const TagLib::Variant &v = ait->second;

            if (key_is(key.c_str(), "data") &&
                v.type() == TagLib::Variant::ByteVector)
            {
                TagLib::ByteVector bv = v.toByteVector();
                out[i].size = bv.size();
                out[i].data = (unsigned char *)malloc(bv.size());
                if (out[i].data)
                    memcpy(out[i].data, bv.data(), bv.size());
            }
            else if (key_is(key.c_str(), "mimeType") &&
                     v.type() == TagLib::Variant::String)
            {
                out[i].mime_type = dup_string(v.toString());
            }
            else if (key_is(key.c_str(), "description") &&
                     v.type() == TagLib::Variant::String)
            {
                out[i].description = dup_string(v.toString());
            }
            else if (key_is(key.c_str(), "pictureType") &&
                     v.type() == TagLib::Variant::String)
            {
                out[i].picture_type = dup_string(v.toString());
            }
        }
    }

    meta->pictures = out;
    meta->n_pictures = n;
}

namespace
{

struct RawLine
{
    std::string text;
    int start_ms;
};

std::vector<RawLine> parse_lrc(const std::string &blob)
{
    std::vector<RawLine> out;
    static const std::regex tag_re(R"(\[(\d{1,3}):(\d{2})(?:\.(\d{1,3}))?\])");

    std::istringstream stream(blob);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::vector<int> timestamps_ms;
        auto begin = std::sregex_iterator(line.begin(), line.end(), tag_re);
        auto end = std::sregex_iterator();
        size_t last_tag_end = 0;

        for (auto it = begin; it != end; ++it)
        {
            std::smatch m = *it;
            int minutes = std::atoi(m[1].str().c_str());
            int seconds = std::atoi(m[2].str().c_str());
            int frac_ms = 0;
            if (m[3].matched)
            {
                std::string frac = m[3].str();
                while (frac.size() < 3)
                    frac += '0';
                frac_ms = std::atoi(frac.substr(0, 3).c_str());
            }
            timestamps_ms.push_back(minutes * 60000 + seconds * 1000 + frac_ms);
            last_tag_end = static_cast<size_t>(m.position(0) + m.length(0));
        }

        if (timestamps_ms.empty())
            continue; /* not a synced line */

        std::string text = line.substr(last_tag_end);
        size_t first_non_space = text.find_first_not_of(" \t");
        if (first_non_space != std::string::npos)
            text = text.substr(first_non_space);
        else
            text.clear();

        for (int ms : timestamps_ms)
            out.push_back({text, ms});
    }

    std::sort(out.begin(), out.end(), [](const RawLine &a, const RawLine &b) {
        return a.start_ms < b.start_ms;
    });
    return out;
}

int ttml_timestamp_to_ms(const std::string &ts)
{
    int h = 0, m = 0;
    double s = 0;
    if (std::sscanf(ts.c_str(), "%d:%d:%lf", &h, &m, &s) == 3)
    {
        return h * 3600000 + m * 60000 + static_cast<int>(s * 1000 + 0.5);
    }

    if (std::sscanf(ts.c_str(), "%d:%lf", &m, &s) == 2)
    {
        return m * 60000 + static_cast<int>(s * 1000 + 0.5);
    }

    return 0;
}

std::vector<RawLine> parse_ttml(const std::string &xml)
{
    std::vector<RawLine> out;
    static const std::regex p_re(
        R"RX(<p\b[^>]*\bbegin="([^"]+)"[^>]*>(.*?)</p>)RX", std::regex::icase);
    static const std::regex tag_strip_re(R"(<[^>]*>)");

    auto begin = std::sregex_iterator(xml.begin(), xml.end(), p_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it)
    {
        std::smatch m = *it;
        int ms = ttml_timestamp_to_ms(m[1].str());
        std::string text = std::regex_replace(m[2].str(), tag_strip_re, "");
        out.push_back({text, ms});
    }
    return out;
}

bool looks_synced(const std::string &s)
{
    static const std::regex tag_re(R"(\[\d{1,3}:\d{2}(?:\.\d{1,3})?\])");
    return std::regex_search(s, tag_re);
}

std::vector<RawLine> extract_from_mpeg(TagLib::MPEG::File &file)
{
    std::vector<RawLine> out;
    TagLib::ID3v2::Tag *id3 = file.ID3v2Tag(false);
    if (!id3)
        return out;

    const TagLib::ID3v2::FrameList &frames = id3->frameList("SYLT");
    for (auto frame : frames)
    {
        auto *sylt =
            dynamic_cast<TagLib::ID3v2::SynchronizedLyricsFrame *>(frame);
        if (!sylt)
            continue;
        for (const auto &pair : sylt->synchedText())
        {
            out.push_back(
                {to_std_string(pair.text), static_cast<int>(pair.time)});
        }
    }
    std::sort(out.begin(), out.end(), [](const RawLine &a, const RawLine &b) {
        return a.start_ms < b.start_ms;
    });
    return out;
}

std::vector<RawLine> extract_from_xiph(TagLib::Ogg::XiphComment *xiph)
{
    std::vector<RawLine> out;
    if (!xiph)
        return out;

    static const char *candidate_keys[] = {"SYNCEDLYRICS", "LYRICS",
                                           "UNSYNCEDLYRICS"};
    const TagLib::PropertyMap &props = xiph->properties();

    for (const char *key : candidate_keys)
    {
        auto it = props.find(key);
        if (it == props.end())
            continue;
        for (const auto &value : it->second)
        {
            std::string text = to_std_string(value);
            if (!looks_synced(text))
                continue;
            std::vector<RawLine> parsed = parse_lrc(text);
            out.insert(out.end(), parsed.begin(), parsed.end());
        }
        if (!out.empty())
            break;
    }
    return out;
}

std::vector<RawLine> extract_from_mp4(TagLib::MP4::File &file)
{
    std::vector<RawLine> out;
    TagLib::MP4::Tag *tag = file.tag();
    if (!tag)
        return out;

    const TagLib::MP4::ItemMap &items = tag->itemMap();

    auto ttml_it = items.find("----:com.apple.iTunes:lyrics");
    if (ttml_it != items.end())
    {
        TagLib::StringList sl = ttml_it->second.toStringList();
        if (!sl.isEmpty())
        {
            out = parse_ttml(to_std_string(sl.front()));
        }
        else
        {
            TagLib::ByteVectorList bvl = ttml_it->second.toByteVectorList();
            if (!bvl.isEmpty())
            {
                const TagLib::ByteVector &bv = bvl.front();
                out = parse_ttml(std::string(bv.data(), bv.size()));
            }
        }
    }

    if (out.empty())
    {
        auto lyr_it = items.find("\xC2\xA9lyr");
        if (lyr_it == items.end())
            lyr_it = items.find("\251lyr");
        if (lyr_it != items.end())
        {
            TagLib::StringList sl = lyr_it->second.toStringList();
            if (!sl.isEmpty())
            {
                std::string text = to_std_string(sl.front());
                if (looks_synced(text))
                    out = parse_lrc(text);
            }
        }
    }

    return out;
}

std::vector<RawLine> extract_from_taglib_file(TagLib::File *tfile)
{
    if (!tfile)
        return {};

    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(tfile))
    {
        return extract_from_mpeg(*mpeg);
    }
    else if (auto *vorbis = dynamic_cast<TagLib::Ogg::Vorbis::File *>(tfile))
    {
        return extract_from_xiph(vorbis->tag());
    }
    else if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(tfile))
    {
        return extract_from_xiph(flac->xiphComment(false));
    }
    else if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(tfile))
    {
        return extract_from_mp4(*mp4);
    }

    return {};
}

}

static void attach_synced_lyrics(TagLib::File *tfile, metadata_t *meta)
{
    std::vector<RawLine> lines = extract_from_taglib_file(tfile);
    if (lines.empty())
        return;

    auto *out = static_cast<synced_lyric_line_t *>(
        std::malloc(lines.size() * sizeof(synced_lyric_line_t)));
    if (!out)
        return;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        out[i].text = strdup(lines[i].text.c_str());
        out[i].start_ms = lines[i].start_ms;
    }

    meta->lyrics_synced = out;
    meta->n_lyrics_synced = lines.size();
}

extern "C" metadata_t *metadata_read(const char *path)
{
#if defined(_WIN32)
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen <= 0)
        return NULL;
    std::wstring wpath(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], wlen);
    if (!wpath.empty() && wpath.back() == L'\0')
        wpath.pop_back();
    TagLib::FileRef ref(wpath.c_str());
#else
    TagLib::FileRef ref(path);
#endif

    if (ref.isNull() || !ref.file())
        return NULL;

    TagLib::File *tfile = ref.file();

    metadata_t *meta = (metadata_t *)calloc(1, sizeof(metadata_t));
    if (!meta)
        return NULL;

    if (TagLib::AudioProperties *ap = tfile->audioProperties())
    {
        meta->duration_seconds = ap->lengthInSeconds();
        meta->bitrate_kbps = ap->bitrate();
        meta->sample_rate_hz = ap->sampleRate();
        meta->channels = ap->channels();
    }

    TagLib::Tag *basic = tfile->tag();
    TagLib::PropertyMap props = tfile->properties();

    size_t extra_cap = 8, extra_n = 0;
    kv_t *extras = (kv_t *)malloc(extra_cap * sizeof(kv_t));

    for (TagLib::PropertyMap::ConstIterator it = props.begin();
         it != props.end(); ++it)
    {
        std::string key = to_std_string(it->first);
        const TagLib::StringList &values = it->second;
        char *joined = join_string_list(values);

        int consumed = 1;

        if (key_is(key.c_str(), "TITLE"))
        {
            meta->title = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "ARTIST"))
        {
            size_t n = values.size();
            if (n > 0)
            {
                meta->artists = (char **)calloc(n, sizeof(char *));
                if (meta->artists)
                {
                    for (size_t a = 0; a < n; ++a)
                        meta->artists[a] = dup_string(values[a]);
                    meta->n_artists = n;
                }
            }
        }
        else if (key_is(key.c_str(), "ALBUM"))
        {
            meta->album = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "ALBUMARTIST"))
        {
            meta->album_artist = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "GENRE"))
        {
            meta->genre = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "COMMENT"))
        {
            meta->comment = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "DATE") ||
                 key_is(key.c_str(), "ORIGINALDATE") ||
                 key_is(key.c_str(), "YEAR"))
        {
            if (!meta->recording_time)
            {
                meta->recording_time = joined;
                joined = NULL;
            }
        }
        else if (key_is(key.c_str(), "TRACKNUMBER"))
        {
            meta->track_number = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "DISCNUMBER"))
        {
            meta->disc_number = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "ISRC"))
        {
            meta->isrc = joined;
            joined = NULL;
        }
        else if (key_is(key.c_str(), "LYRICS"))
        {
            size_t n = values.size();
            if (n > 0)
            {
                meta->lyrics = (lyric_line_t *)calloc(n, sizeof(lyric_line_t));
                if (meta->lyrics)
                {
                    for (size_t l = 0; l < n; ++l)
                        meta->lyrics[l].text = dup_string(values[l]);
                    meta->n_lyrics = n;
                }
            }
        }
        else if (key_is(key.c_str(), "REPLAYGAIN_TRACK_GAIN"))
        {
            int ok;
            double v = parse_double_or(joined, &ok);
            meta->track_gain.value = v;
            meta->track_gain.has_value = ok;
        }
        else if (key_is(key.c_str(), "REPLAYGAIN_TRACK_PEAK"))
        {
            int ok;
            double v = parse_double_or(joined, &ok);
            meta->track_peak.value = v;
            meta->track_peak.has_value = ok;
        }
        else if (key_is(key.c_str(), "REPLAYGAIN_ALBUM_GAIN"))
        {
            int ok;
            double v = parse_double_or(joined, &ok);
            meta->album_gain.value = v;
            meta->album_gain.has_value = ok;
        }
        else if (key_is(key.c_str(), "REPLAYGAIN_ALBUM_PEAK"))
        {
            int ok;
            double v = parse_double_or(joined, &ok);
            meta->album_peak.value = v;
            meta->album_peak.has_value = ok;
        }
        else if (key_is(key.c_str(), "POPULARIMETER"))
        {
            if (joined)
            {
                char *copy = xstrdup(joined);
                char *first_bar = strchr(copy, '|');
                if (first_bar)
                {
                    char *rating_start = first_bar + 1;
                    char *second_bar = strchr(rating_start, '|');
                    if (second_bar)
                        *second_bar = '\0';
                    if (*rating_start)
                    {
                        meta->popularity.value =
                            (unsigned int)strtoul(rating_start, NULL, 10);
                        meta->popularity.has_value = 1;
                    }
                }
                free(copy);
            }
        }
        else
        {
            consumed = 0;
        }

        if (!consumed)
        {
            if (extra_n == extra_cap)
            {
                extra_cap *= 2;
                kv_t *grown = (kv_t *)realloc(extras, extra_cap * sizeof(kv_t));
                if (grown)
                    extras = grown;
            }
            if (extra_n < extra_cap)
            {
                extras[extra_n].key = xstrdup(key.c_str());
                extras[extra_n].value = joined;
                joined = NULL;
                extra_n++;
            }
        }

        free(joined);
    }

    meta->extra_properties = extras;
    meta->n_extra_properties = extra_n;

    if (basic)
    {
        if (!meta->title)
            meta->title = dup_string(basic->title());
        if (!meta->album)
            meta->album = dup_string(basic->album());
        if (!meta->genre)
            meta->genre = dup_string(basic->genre());
        if (!meta->comment)
            meta->comment = dup_string(basic->comment());
        if (meta->n_artists == 0)
        {
            TagLib::String a = basic->artist();
            if (!a.isEmpty())
            {
                meta->artists = (char **)calloc(1, sizeof(char *));
                if (meta->artists)
                {
                    meta->artists[0] = dup_string(a);
                    meta->n_artists = 1;
                }
            }
        }
        if (!meta->recording_time)
        {
            unsigned int year = basic->year();
            if (year > 0)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%u", year);
                meta->recording_time = xstrdup(buf);
            }
        }
        if (!meta->track_number)
        {
            unsigned int track = basic->track();
            if (track > 0)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%u", track);
                meta->track_number = xstrdup(buf);
            }
        }
    }

    extract_pictures(tfile, meta);
    attach_synced_lyrics(tfile, meta);

    return meta;
}

extern "C" void metadata_free(metadata_t *meta)
{
    if (!meta)
        return;

    free(meta->title);
    for (size_t i = 0; i < meta->n_artists; ++i)
        free(meta->artists[i]);
    free(meta->artists);
    free(meta->album);
    free(meta->album_artist);
    free(meta->genre);
    free(meta->comment);
    free(meta->recording_time);
    free(meta->track_number);
    free(meta->disc_number);
    free(meta->isrc);

    for (size_t i = 0; i < meta->n_lyrics; ++i)
        free(meta->lyrics[i].text);
    free(meta->lyrics);

    for (size_t i = 0; i < meta->n_lyrics_synced; ++i)
        free(meta->lyrics_synced[i].text);
    free(meta->lyrics_synced);

    for (size_t i = 0; i < meta->n_pictures; ++i)
    {
        free(meta->pictures[i].mime_type);
        free(meta->pictures[i].description);
        free(meta->pictures[i].picture_type);
        free(meta->pictures[i].data);
    }
    free(meta->pictures);

    for (size_t i = 0; i < meta->n_extra_properties; ++i)
    {
        free(meta->extra_properties[i].key);
        free(meta->extra_properties[i].value);
    }
    free(meta->extra_properties);

    free(meta);
}
