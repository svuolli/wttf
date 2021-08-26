#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/*
 * References:
 *  https://docs.microsoft.com/en-us/typography/opentype/spec/otff
 *  https://github.com/nothings/stb/blob/master/stb_truetype.h
 */

namespace ttf
{

enum class platform_id: std::uint16_t
{
    unicode = 0,
    macintosh = 1,
    iso = 2,
    windows = 3,
    custom = 4
};

static constexpr uint16_t windows_unicode_bmp_encoding_id = 1;
static constexpr uint16_t windows_unicode_full_encoding_id = 10;

using tag_t = std::array<std::uint8_t, 4>;

tag_t from_c_string(char const * str)
{
    assert(std::strlen(str) == 4);

    auto begin = reinterpret_cast<std::uint8_t const *>(str);
    return tag_t{begin[0], begin[1], begin[2], begin[3]};
}

std::string to_string(tag_t t)
{
    std::string s;
    for(auto b: t)
    {
        s += static_cast<char>(b);
    }
    return s;
}

struct table_entry
{
    tag_t tag;
    std::uint32_t checksum;
    std::uint32_t offset;
    std::uint32_t length;
    static constexpr size_t byte_size = 16;
};

struct encoding_record
{
    platform_id platform;
    std::uint16_t encoding_id;
    std::uint32_t subtable_offset;
    static constexpr size_t byte_size = 8;
};

class parser
{
    public:
    parser(std::vector<std::byte> && d):
        m_data(std::forward<decltype(m_data)>(d))
    {
        m_cmap = find_table("cmap");
        m_loca = find_table("loca");
        m_head = find_table("head");
        m_glyf = find_table("glyf");
        m_hhea = find_table("hhea");
        m_hmtx = find_table("hmtx");
        m_kern = find_table("kern");
        m_gpos = find_table("GPOS");

        auto const num_cmap_tables = get_value<std::uint16_t>(m_cmap + 2);

        for(auto i = 0u; i != num_cmap_tables; ++i)
        {
            auto const record_offset = m_cmap + 4 + encoding_record::byte_size * i;
            auto const record = get_value<encoding_record>(record_offset);

            switch(record.platform)
            {
                case platform_id::windows:
                    if(record.encoding_id == windows_unicode_bmp_encoding_id ||
                       record.encoding_id == windows_unicode_full_encoding_id)
                    {
                        m_cmap_index = m_cmap + record.subtable_offset;
                    }
                    break;
                case platform_id::unicode:
                    m_cmap_index = m_cmap + record.subtable_offset;
                    break;
                default:
                    break;
            }
        }

        auto const cmap_format = get_value<std::uint16_t>(m_cmap_index);
        switch(cmap_format)
        {
            case 0:
                m_glyph_index_fn = &parser::format0_glyph_index;
            case 4:
                m_glyph_index_fn = &parser::format4_glyph_index;
                break;
            case 6:
                m_glyph_index_fn = &parser::format6_glyph_index;

            default:
                break;
        }
    }

    std::uint32_t version() const
    {
        return get_value<uint32_t>(0);
    }

    std::uint16_t glyph_index(int codepoint) const
    {
        if(m_glyph_index_fn)
            return std::invoke(m_glyph_index_fn, this, codepoint);

        return 0;
    }

    private:
    template <typename T>
    T get_value(std::size_t offset) const
    {
        using U = std::make_unsigned_t<T>;
        static constexpr auto size = sizeof(T);

        auto v = U{0};
        for(auto i = offset; i != offset + size; ++i)
        {
            v = (v<<8) | std::to_integer<U>(m_data[i]);
        }

        return static_cast<T>(v);
    }

    template<>
    tag_t get_value<tag_t>(std::size_t offset) const
    {
        return tag_t{
            get_value<std::uint8_t>(offset+0),
            get_value<std::uint8_t>(offset+1),
            get_value<std::uint8_t>(offset+2),
            get_value<std::uint8_t>(offset+3)};
    }

    template<>
    table_entry get_value<table_entry>(std::size_t offset) const
    {
        return table_entry{
            get_value<tag_t>(offset+0),
            get_value<std::uint32_t>(offset+4),
            get_value<std::uint32_t>(offset+8),
            get_value<std::uint32_t>(offset+12)};
    }

    template<>
    encoding_record get_value<encoding_record>(std::size_t offset) const
    {
        return encoding_record{
            get_value<platform_id>(offset+0),
            get_value<std::uint16_t>(offset+2),
            get_value<std::uint32_t>(offset+4) };
    }

    std::uint32_t find_table(char const * tag, std::size_t extra_offset = 0) const
    {
        auto const t = from_c_string(tag);

        auto num_tables = get_value<std::uint16_t>(4+extra_offset);

        for(auto i = 0u; i != num_tables; ++i)
        {
            auto const ofs = extra_offset + 12 + (i*table_entry::byte_size);
            auto e = get_value<table_entry>(ofs);
            if(e.tag == t)
            {
                return e.offset;
            }
        }

        return 0;
    }

    std::uint16_t format0_glyph_index(int codepoint) const
    {
        auto const length = get_value<std::uint16_t>(m_cmap_index + 2);
        if(codepoint < length-6)
        {
            return get_value<std::uint8_t>(m_cmap_index + 6 + codepoint);
        }
        return 0;
    }

    std::uint16_t format4_glyph_index(int codepoint) const
    {
        if(codepoint > 0xFFFF) // Format 4 only handles BPM
        {
            return 0;
        }

        auto const seg_count = get_value<std::uint16_t>(m_cmap_index+6) >> 1;
        auto search_range = get_value<std::uint16_t>(m_cmap_index+8);
        auto entry_selector = get_value<std::uint16_t>(m_cmap_index+10);
        auto const range_shift = get_value<std::uint16_t>(m_cmap_index+12);

        auto const end_count = m_cmap_index + 14;
        auto search = end_count;    

        if(codepoint > get_value<std::uint16_t>(search + range_shift))
        {
            search += range_shift;
        }

        search -= 2;

        while(entry_selector)
        {
            search_range >>= 1;
            if(codepoint > get_value<std::uint16_t>(search + search_range))
                search += search_range;
            --entry_selector;
        }

        search += 2;

        auto const end_code = m_cmap_index + 14;
        auto const item = (search - end_count) >> 1;
        auto const start = get_value<std::uint16_t>(end_code + seg_count*2 + 2 + 2*item);
        if(codepoint < start)
        {
            return 0;
        }

        auto const offset = get_value<std::uint16_t>(end_code + seg_count*6 + 2 + 2*item);

        if(offset == 0)
        {
            return
                codepoint +
                get_value<std::uint16_t>(end_code + seg_count*4 + 2 + 2*item);
        }

        return get_value<std::uint16_t>(end_code + offset + (codepoint-start)*2 + seg_count*6 + 2 + 2*item);
    }

    std::uint16_t format6_glyph_index(int codepoint) const
    {
        auto const first_code = get_value<std::uint16_t>(m_cmap_index + 6);
        auto const entry_count = get_value<std::uint16_t>(m_cmap_index + 8);

        if(codepoint >= first_code && codepoint < (first_code + entry_count))
        {
            return get_value<std::uint16_t>(m_cmap_index + 10 + (codepoint - first_code)*2);
        }

        return 0;
    }

    using glyph_index_fn_t = std::uint16_t (parser::*)(int) const;
    glyph_index_fn_t m_glyph_index_fn{nullptr};

    std::vector<std::byte> m_data;
    std::uint32_t m_cmap{0};
    std::uint32_t m_loca{0};
    std::uint32_t m_head{0};
    std::uint32_t m_glyf{0};
    std::uint32_t m_hhea{0};
    std::uint32_t m_hmtx{0};
    std::uint32_t m_kern{0};
    std::uint32_t m_gpos{0};

    std::uint32_t m_cmap_index{0};
};

}

int main(int argc, char const * argv[])
{
    if(argc < 2)
    {
        std::cerr << fmt::format("{}: too few arguments\n", argv[0]);
        return -1;
    }

    using file_stream = std::basic_ifstream<std::byte>;
    using file_buff_iterator = std::istreambuf_iterator<std::byte>;

    std::ifstream fs{argv[1], std::ios::binary};
    auto contents = std::vector<std::byte>{};
    std::transform(
        std::istreambuf_iterator<char>{fs}, std::istreambuf_iterator<char>{},
        std::back_insert_iterator{contents},
        [](auto c) { return static_cast<std::byte>(c); });

    fmt::print("read file \"{}\", {} bytes\n", argv[1], contents.size());

    auto info = ttf::parser{std::move(contents)};

    for(auto cp = 32u; cp < 0xFFFF; ++cp)
    {
        auto const gi = info.glyph_index(cp);
        if(gi)
        {
            fmt::print("cp: {}, gi: {}\n", cp, gi);
        }
    }

    return 0;
}
