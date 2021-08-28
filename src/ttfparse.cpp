#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef TTF_ASSERT
#ifndef NDEBUG
#include <cassert>
#define TTF_ASSERT(x) assert(x)
#else
#define TTF_ASSERT(x)
#endif
#endif

/*
 * References:
 *  https://docs.microsoft.com/en-us/typography/opentype/spec/otff
 *  https://github.com/nothings/stb/blob/master/stb_truetype.h
 */

namespace ttf
{

using matrix_2x2 = std::array<float, 2*2>;

struct transform
{
    matrix_2x2 m{1.0f, 0.0f, 0.0f, 1.0f};
    float tx{0};
    float ty{0};

    constexpr std::pair<float, float>
    apply(float const x, float const y) const
    {
        auto const sx = std::sqrtf(m[0]*m[0] + m[1]*m[1]);
        auto const sy = std::sqrtf(m[2]*m[2] + m[3]*m[3]);

        auto ox = sx * (m[0]*x + m[2]*y + tx);
        auto oy = sy * (m[1]*x + m[3]*y + ty);

        return {ox, oy};
    }
};

class shape
{
    public:
    struct vertex
    {
        float x;
        float y;
        bool on_curve;
    };

    using contour_t = std::vector<vertex>;

    shape() = default;
    shape(shape const &) = default;
    shape(shape &&) = default;
    shape(
        float min_x, float min_y,
        float max_x, float max_y,
        std::size_t contours):
        m_contours{},
        m_min_x{min_x},
        m_min_y{min_y},
        m_max_x{max_x},
        m_max_y{max_y}
    {
        m_contours.reserve(contours);
    }

    ~shape() = default;

    shape & operator=(shape const &) = default;
    shape & operator=(shape &&) = default;

    std::size_t num_contours() const
    {
        return m_contours.size();
    }

    contour_t const & contour(std::size_t i) const
    {
        return m_contours[i];
    }

    float min_x() const { return m_min_x; }
    float min_y() const { return m_min_y; }
    float max_x() const { return m_max_x; }
    float max_y() const { return m_max_y; }
    float width() const { return m_max_x - m_min_x; }
    float height() const { return m_max_y - m_min_y; }

    bool empty() const { return m_contours.empty(); }

    void add_contour()
    {
        m_contours.emplace_back();
    }

    void add_contour(std::size_t s)
    {
        m_contours.emplace_back();
        m_contours.back().reserve(s);
    }

    void add_vertex(float x, float y, bool on_curve)
    {
        m_flat |= !on_curve;
        m_contours.back().push_back({x, y, on_curve});
    }

    void add_shape(shape const & s, transform const & t = {})
    {
        auto const min_p = t.apply(s.m_min_x, s.m_min_y);
        auto const max_p = t.apply(s.m_max_x, s.m_max_y);
        m_min_x = empty() ? min_p.first : std::min(min_p.first, m_min_x);
        m_min_y = empty() ? min_p.second : std::min(min_p.second, m_min_y);
        m_max_x = empty() ? max_p.first : std::max(max_p.first, m_max_x);
        m_max_y = empty() ? max_p.second : std::max(max_p.second, m_max_y);

        m_contours.reserve(m_contours.size() + s.num_contours());

        for(auto i=0u; i<s.num_contours(); ++i)
        {
            auto const & cont = s.contour(i);
            add_contour(cont.size());

            for(auto const & v1: cont)
            {
                auto const & v2 = t.apply(v1.x, v1.y);
                add_vertex(v2.first, v2.second, v1.on_curve);
            }
        }
    }

    void transform(ttf::transform const & t)
    {
        if(empty())
            return;

        auto const minp = t.apply(m_min_x, m_min_y);
        auto const maxp = t.apply(m_max_x, m_max_y);

        m_min_x = minp.first;
        m_min_y = minp.second;
        m_max_x = maxp.first;
        m_max_y = maxp.second;

        for(auto & cont: m_contours)
        {
            for(auto & v1: cont)
            {
                auto v2 = t.apply(v1.x, v1.y);
                v1.x = v2.first;
                v1.y = v2.second;
            }
        }
    }

    private:
    std::vector<contour_t> m_contours;
    float m_min_x;
    float m_min_y;
    float m_max_x;
    float m_max_y;
    bool m_flat{true};
};

class parser
{
    public:
    parser(std::vector<std::byte> && d):
        m_data(std::forward<decltype(m_data)>(d))
    {
        // TODO: assert that version() is one of the supported ones
        // But which ones are supported?

        m_cmap = find_table("cmap");
        m_loca = find_table("loca");
        m_head = find_table("head");
        m_glyf = find_table("glyf");
        m_hhea = find_table("hhea");
        m_hmtx = find_table("hmtx");
        m_kern = find_table("kern");
        m_gpos = find_table("GPOS");
        m_maxp = find_table("maxp");

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
        TTF_ASSERT(m_cmap_index != 0);

        // Format of cmap
        switch(get_value<std::uint16_t>(m_cmap_index))
        {
            case 0:
                m_glyph_index_fn = &parser::format0_glyph_index;
            case 4:
                m_glyph_index_fn = &parser::format4_glyph_index;
                break;
            case 6:
                m_glyph_index_fn = &parser::format6_glyph_index;

            default:
                // Unknown or unsupported cmap format
                TTF_ASSERT(false);
                break;
        }

        // Format of loca table
        switch(get_value<std::uint16_t>(m_head + 50))
        {
            case 0:
                m_glyph_offset_fn = &parser::format0_glyph_offset;
                break;
            case 1:
                m_glyph_offset_fn = &parser::format1_glyph_offset;
                break;

            default:
                // Unknown indexToLocFormat
                TTF_ASSERT(false);
                break;
        }

        m_num_glyphs = m_maxp ? get_value<std::uint16_t>(m_maxp + 4) : 0xffff;
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

    shape glyph_shape(std::uint16_t glyph_index) const
    {
        auto const glyph_offs = glyph_offset(glyph_index);
        if(!glyph_offs)
            return {};

        auto const gh = get_value<glyph_header>(*glyph_offs);

        if(gh.number_of_contours > 0) // Simple description
        {
            return simple_glyph_shape(*glyph_offs, gh);
        }
        else if(gh.number_of_contours < 0) // Composite description
        {
            return composite_glyph_shape(*glyph_offs, gh);
        }
        else
        {
            return {};
        }
    }

    private:
    using tag_t = std::array<std::uint8_t, 4>;

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

    enum simple_glyph_flags: uint8_t
    {
        on_curve_point = 0x01,
        x_short_vector = 0x02,
        y_short_vector = 0x04,
        repeat_flag = 0x08,
        x_is_same_or_positive_x_short_vector = 0x10,
        y_is_same_or_positive_y_short_vector = 0x20,
        overlap_simple = 0x40
    };

    enum composite_glyph_flags: uint16_t
    {
        arg_1_and_arg_2_are_words = 0x0001,
        args_are_xy_values = 0x0002,
        round_xy_to_grid = 0x004,
        we_have_a_scale = 0x008,
        more_components = 0x0020,
        we_have_x_and_y_scale = 0x0040,
        we_have_a_two_by_two = 0x0080,
        we_have_instructions = 0x0100,
        use_my_metrics = 0x0200,
        overlap_compound = 0x0400,
        scaled_component_offset = 0x0800,
        unscaled_component_offset = 0x100,
        reserved = 0xE010
    };

    struct table_entry
    {
        tag_t tag;
        std::uint32_t checksum;
        std::uint32_t offset;
        std::uint32_t length;
        static constexpr std::size_t byte_size = 16;
    };

    struct encoding_record
    {
        platform_id platform;
        std::uint16_t encoding_id;
        std::uint32_t subtable_offset;
        static constexpr std::size_t byte_size = 8;
    };

    struct glyph_header
    {
        std::int16_t number_of_contours;
        std::int16_t x_min;
        std::int16_t y_min;
        std::int16_t x_max;
        std::int16_t y_max;
        static constexpr std::size_t byte_size = 10;
    };

    struct data_cursor
    {
        data_cursor(parser const & own, std::size_t offs):
            owner(own), offset(offs)
        {}

        data_cursor operator+=(std::size_t offs)
        {
            offset += offs;
            return *this;
        }

        template <typename T>
        T peek(std::size_t offs = 0) const
        {
            return owner.get_value<T>(offset+offs);
        }

        template <typename T>
        T read()
        {
            auto const offs = offset;
            offset += sizeof(T);
            return owner.get_value<T>(offs);
        }

        parser const & owner;
        std::size_t offset;
    };

    static tag_t tag_from_c_string(char const * str)
    {
        TTF_ASSERT(std::strlen(str) == 4);

        auto begin = reinterpret_cast<std::uint8_t const *>(str);
        return tag_t{begin[0], begin[1], begin[2], begin[3]};
    }

    using glyph_index_fn_t = std::uint16_t (parser::*)(int) const;
    using glyph_offset_fn_t =
        std::optional<std::uint32_t> (parser::*)(std::uint16_t) const;

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

    template<>
    glyph_header get_value<glyph_header>(std::size_t offset) const
    {
        return glyph_header{
            get_value<std::int16_t>(offset+0),
            get_value<std::int16_t>(offset+2),
            get_value<std::int16_t>(offset+4),
            get_value<std::int16_t>(offset+6),
            get_value<std::int16_t>(offset+8)};
    }

    std::uint32_t find_table(char const * tag, std::size_t extra_offset = 0) const
    {
        auto const t = tag_from_c_string(tag);

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

        return get_value<std::uint16_t>(
            end_code + offset + (codepoint-start)*2 + seg_count*6 + 2 + 2*item);
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

    std::optional<std::uint32_t> format0_glyph_offset(std::uint16_t glyph_index) const
    {
        auto const g1 = m_glyf + get_value<std::uint16_t>(m_loca + glyph_index * 2) * 2;
        auto const g2 = m_glyf + get_value<std::uint16_t>(m_loca + glyph_index * 2 + 2) * 2;

        using ret_t = std::optional<std::uint32_t>;

        return g1==g2 ? ret_t{} : ret_t{g1};
    }

    std::optional<std::uint32_t> format1_glyph_offset(std::uint16_t glyph_index) const
    {
        auto const g1 = m_glyf + get_value<std::uint32_t>(m_loca + glyph_index * 4);
        auto const g2 = m_glyf + get_value<std::uint32_t>(m_loca + glyph_index * 4 + 4);

        using ret_t = std::optional<std::uint32_t>;

        return g1==g2 ? ret_t{} : ret_t{g1};
    }

    std::optional<std::uint32_t> glyph_offset(std::uint16_t glyph_index) const
    {
        if(glyph_index >= m_num_glyphs || m_glyph_offset_fn == nullptr)
            return std::nullopt;

        return std::invoke(m_glyph_offset_fn, this, glyph_index);
    }

    shape simple_glyph_shape(
        std::uint32_t const glyph_offset, glyph_header const & gh) const
    {
        struct vertex
        {
            std::int16_t x{0};
            std::int16_t y{0};
            std::uint8_t flags{0};
        };

        auto const end_pts_of_countours_offset =
            glyph_offset + glyph_header::byte_size;
        auto const instruction_length = get_value<std::uint16_t>(
            end_pts_of_countours_offset + gh.number_of_contours * 2);

        auto end_pts = data_cursor{*this, end_pts_of_countours_offset};
        auto points = data_cursor{
            *this,
            end_pts_of_countours_offset + gh.number_of_contours*2 + 2 + instruction_length};

        auto const num_points =
            std::size_t{1} +
            end_pts.peek<std::uint16_t>((gh.number_of_contours-1) * 2);

        auto vertices = std::vector<vertex>{num_points};

        // Read flags
        auto repeat = 0u;
        auto current_flags = std::uint8_t{0};
        for(auto i=0u; i < num_points; ++i)
        {
            if(repeat == 0)
            {
                current_flags = points.read<std::uint8_t>();
                if(current_flags & 8)
                {
                    repeat = points.read<std::uint8_t>();
                }
            }
            else
            {
                --repeat;
            }
            vertices[i].flags = current_flags;
        }

        // Read x coordinates
        auto current_x = std::int16_t{0};
        for(auto i=0u; i < num_points; ++i)
        {
            auto & v = vertices[i];
            current_flags = v.flags;
            if(current_flags & simple_glyph_flags::x_short_vector)
            {
                auto const x = points.read<std::uint8_t>();
                current_x +=
                    (current_flags & simple_glyph_flags::x_is_same_or_positive_x_short_vector) ?
                    x : -x;
            }
            else
            {
                if(!(current_flags & simple_glyph_flags::x_is_same_or_positive_x_short_vector))
                {
                    current_x += points.read<std::int16_t>();
                }
            }
            v.x = current_x;
        }

        // Read y coordinates
        auto current_y = std::uint16_t{0};
        for(auto i=0u; i < num_points; ++i)
        {
            auto & v = vertices[i];
            current_flags = v.flags;
            if(current_flags & simple_glyph_flags::y_short_vector)
            {
                auto const y = points.read<std::uint8_t>();
                current_y +=
                    (current_flags & simple_glyph_flags::y_is_same_or_positive_y_short_vector) ?
                    y : -y;
            }
            else
            {
                if(!(current_flags & simple_glyph_flags::y_is_same_or_positive_y_short_vector))
                {
                    current_y += points.read<std::int16_t>();
                }
            }
            v.y = current_y;
        }

        auto s = shape{
            static_cast<float>(gh.x_min),
            static_cast<float>(gh.y_min),
            static_cast<float>(gh.x_max),
            static_cast<float>(gh.y_max),
            static_cast<std::size_t>(gh.number_of_contours)};

        auto next_contour = std::uint16_t{0};
        for(auto i=0u; i < num_points; ++i)
        {
            if(next_contour == i)
            {
                next_contour = end_pts.read<std::uint16_t>() + 1;
                s.add_contour(next_contour-i);
            }

            auto & v = vertices[i];
            s.add_vertex(v.x, v.y, v.flags & simple_glyph_flags::on_curve_point);
        }

        return s;
    }

    shape composite_glyph_shape(
        std::uint32_t const glyph_offset, glyph_header const & gh) const
    {
        auto data = data_cursor{*this, glyph_offset+10};

        auto flags =
            static_cast<uint16_t>(composite_glyph_flags::more_components);

        auto result = shape{};

        while(flags & composite_glyph_flags::more_components)
        {
            flags = data.read<std::uint16_t>();
            auto glyph_index = data.read<std::uint16_t>();

            auto t = transform{};
            auto point1 = std::uint16_t{0};
            auto point2 = std::uint16_t{0};

            if(flags & composite_glyph_flags::args_are_xy_values)
            {
                if(flags & composite_glyph_flags::arg_1_and_arg_2_are_words)
                {
                    t.tx = data.read<std::int16_t>();
                    t.ty = data.read<std::int16_t>();
                }
                else
                {
                    t.tx = data.read<std::int8_t>();
                    t.ty = data.read<std::int8_t>();
                }
            }
            else
            {
                if(flags & composite_glyph_flags::arg_1_and_arg_2_are_words)
                {
                    point1 = data.read<std::uint16_t>();
                    point2 = data.read<std::uint16_t>();
                }
                else
                {
                    point1 = data.read<std::uint8_t>();
                    point2 = data.read<std::uint8_t>();
                }
            }

            if(flags & composite_glyph_flags::we_have_a_scale)
            {
                auto const scale = data.read<std::int16_t>()/16384.0f;
                t.m[0] = scale;
                t.m[3] = scale;
            }
            else if(flags & composite_glyph_flags::we_have_x_and_y_scale)
            {
                t.m[0] = data.read<std::int16_t>()/16384.0f;
                t.m[3] = data.read<std::int16_t>()/16384.0f;
            }
            else if(flags & composite_glyph_flags::we_have_a_two_by_two)
            {
                for(auto & e: t.m)
                {
                    e = data.read<std::int16_t>()/16384.0f;
                }
            }

            result.add_shape(glyph_shape(glyph_index), t);
        }

        return result;
    }

    glyph_index_fn_t m_glyph_index_fn{nullptr};
    glyph_offset_fn_t m_glyph_offset_fn{nullptr};

    std::vector<std::byte> m_data;
    std::uint32_t m_cmap{0};
    std::uint32_t m_loca{0};
    std::uint32_t m_head{0};
    std::uint32_t m_glyf{0};
    std::uint32_t m_hhea{0};
    std::uint32_t m_hmtx{0};
    std::uint32_t m_kern{0};
    std::uint32_t m_gpos{0};
    std::uint32_t m_maxp{0};

    std::uint32_t m_cmap_index{0};
    std::uint16_t m_num_glyphs{0};
};

}

auto const html_head = 
R"html(

<!DOCTYPE html>
<html>
<body>

<canvas id="myCanvas" width="{}" height="{}" style="border:1px solid #d3d3d3;">
Your browser does not support the HTML canvas tag.</canvas>

<script>
var c = document.getElementById("myCanvas");
var ctx = c.getContext("2d");
ctx.scale(0.5, -0.5);
ctx.translate({}, {});

ctx.beginPath();

)html";

auto const html_foot =
R"html(

ctx.fill();

</script>

</body>
</html>
)html";

void draw_contour(ttf::shape::contour_t const & c)
{
    // fmt::print("ctx.beginPath();\n");
    fmt::print("ctx.moveTo({}, {});\n", c[0].x, c[0].y);

    auto prev_on_curve = true;
    auto cx = 0;
    auto cy = 0;
    for(auto i = 1u; i < c.size(); ++i)
    {
        auto const & v = c[i];
        if(v.on_curve)
        {
            if(prev_on_curve)
            {
                fmt::print("ctx.lineTo({}, {});\n", v.x, v.y);
            }
            else
            {
                fmt::print("ctx.quadraticCurveTo({}, {}, {}, {});\n", cx, cy, v.x, v.y);
            }
        }
        else
        {
            if(!prev_on_curve)
            {
                fmt::print("ctx.quadraticCurveTo({}, {}, {}, {});\n", cx, cy, (v.x+cx)/2, (v.y+cy)/2);
            }
            cx = v.x;
            cy = v.y;
        }
        prev_on_curve = v.on_curve;
    }

    auto const & v = c[0];
    if(prev_on_curve)
    {
        fmt::print("ctx.lineTo({}, {});\n", v.x, v.y);
    }
    else
    {
        fmt::print("ctx.quadraticCurveTo({}, {}, {}, {});\n", cx, cy, v.x, v.y);
    }

    // fmt::print("ctx.stroke();\n\n");
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

    auto info = ttf::parser{std::move(contents)};
    auto const s = info.glyph_shape(info.glyph_index(197));

    fmt::print(html_head, (s.width()+1)/2, (s.height()+2)/2, -s.min_x(), -s.max_y());

    for(auto i=0; i < s.num_contours(); ++i)
    {
        draw_contour(s.contour(i));
    }

    fmt::print(html_foot);

    return 0;
}
