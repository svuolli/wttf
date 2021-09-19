#include <ttf/typeface.hpp>
#include "font_data.hpp"

#include <functional>

namespace ttf
{

template <typename T> T typeface::get(std::size_t offset) const
{
    return m_data->get<T>(offset);
}

typeface::typeface() = default;
typeface::typeface(typeface const &) = default;

typeface::typeface(std::vector<std::byte> && data):
    typeface{std::make_shared<font_data const>(std::move(data)), 0}
{}

typeface::typeface(font_collection const & collection, std::size_t index):
    typeface{collection.m_data, 0}
{}

typeface::typeface(
    std::shared_ptr<font_data const> const & data, std::size_t offset):
    m_data{data},
    m_data_offset{offset}
{
    auto const cmap = find_table("cmap");
    auto const num_cmap_tables = get<std::uint16_t>(cmap + 2);

    for(auto i = 0u; i != num_cmap_tables; ++i)
    {
        auto const record_offset = cmap + 4 + encoding_record::byte_size * i;
        auto const record = get<encoding_record>(record_offset);

        switch(record.platform)
        {
            case platform_id::windows:
                if(record.encoding_id == windows_unicode_bmp_encoding_id ||
                   record.encoding_id == windows_unicode_full_encoding_id)
                {
                    m_cmap_index = cmap + record.subtable_offset;
                }
                break;
            case platform_id::unicode:
                m_cmap_index = cmap + record.subtable_offset;
                break;
            default:
                break;
        }
    }

        // Format of cmap
    switch(get<std::uint16_t>(m_cmap_index))
    {
        case 0:
            m_glyph_index_fn = &typeface::format0_glyph_index;
        case 4:
            m_glyph_index_fn = &typeface::format4_glyph_index;
            break;
        case 6:
            m_glyph_index_fn = &typeface::format6_glyph_index;

        default:
            // Unknown or unsupported cmap format
            TTF_ASSERT(false);
            break;
    }

    TTF_ASSERT(m_cmap_index != 0);

    auto const head = find_table("head");
    switch(get<std::uint16_t>(head + 50))
    {
        case 0:
            m_glyph_offset_fn = &typeface::format0_glyph_offset;
            break;
        case 1:
            m_glyph_offset_fn = &typeface::format1_glyph_offset;
            break;

        default:
            // Unknown indexToLocFormat
            TTF_ASSERT(false);
            break;
    }

    auto const maxp = find_table("maxp");
    m_num_glyphs = maxp ? get<std::uint16_t>(maxp + 4) : 0xFFFF;

    m_loca = find_table("loca");
    m_glyf = find_table("glyf");
    m_hmtx = find_table("hmtx");

    auto hhea = find_table("hhea");
    if(hhea)
    {
        m_metrics.ascent =
            static_cast<float>(get<std::int16_t>(hhea + 4));
        m_metrics.descent =
            static_cast<float>(get<std::int16_t>(hhea + 6));
        m_metrics.line_gap =
            static_cast<float>(get<std::int16_t>(hhea + 8));
        m_number_of_h_metrics = get<std::uint16_t>(hhea + 34);
    }

    m_kern = find_table("kern");
    if(m_kern)
    {
        auto const version = get<std::uint16_t>(m_kern);
        auto const n_tables = get<std::uint16_t>(m_kern + 2);
        if(n_tables > 0 && version == 0) // We only know version 0
        {
            auto find_sub_table = [n_tables, this]() -> std::uint32_t
            {
                auto offset = m_kern + 4;
                for(auto table = 0u; table != n_tables; ++table)
                {
                    auto const version = get<std::uint16_t>(offset);
                    auto const length = get<std::uint16_t>(offset + 2);
                    auto const coverage = get<std::uint16_t>(offset + 4);
                    auto const format = coverage >> 8;
                    auto const horizontal = ((coverage & 1) != 0);
                    if(version == 0 && format == 0 && horizontal)
                    {
                        return offset;
                    }
                    offset += length;
                }

                return 0;
            };

            auto const sub_table = find_sub_table();
            auto const n_pairs = get<std::uint16_t>(sub_table + 6);

            auto stream = m_data->create_cursor(sub_table+14);
            for(auto i = 0u; i !=n_pairs; ++i)
            {
                auto const left = stream.read<std::uint16_t>();
                auto const right = stream.read<std::uint16_t>();
                auto const value = stream.read<std::int16_t>();
                m_kerning_tables[left][right] = static_cast<float>(value);
            }
        }
    }
}

typeface::~typeface() = default;

typeface & typeface::operator=(const typeface &) = default;

std::size_t typeface::glyph_index(int codepoint) const
{
    return
        m_glyph_index_fn ? std::invoke(m_glyph_index_fn, this, codepoint) : 0;
}

shape typeface::glyph_shape(std::uint16_t glyph_index) const
{
    auto const glyph_offs = glyph_offset(glyph_index);
    if(!glyph_offs)
        return {};

    auto const num_contours = get<std::int16_t>(glyph_offs);
    if(num_contours > 0)
    {
        return simple_glyph_shape(glyph_offs);
    }
    else if(num_contours < 0)
    {
        return composite_glyph_shape(glyph_offs);
    }

    return {};
}

glyph_metrics typeface::metrics(std::uint16_t glyph_index) const
{
    auto adv = 0.0f;
    auto lsb = 0.0f;

    if(glyph_index < m_number_of_h_metrics)
    {
        auto const offset = m_hmtx + glyph_index * 4;
        adv = static_cast<float>(get<std::uint16_t>(offset + 0));
        lsb = static_cast<float>(get<std::int16_t>(offset + 2));
    }
    else
    {
        adv = static_cast<float>(
            get<std::uint16_t>(m_hmtx + 4 * (m_number_of_h_metrics-1)));
        lsb = static_cast<float>(get<std::int16_t>(
                m_hmtx +
                4 * m_number_of_h_metrics +
                2 * (glyph_index - m_number_of_h_metrics)));
    }

    auto const offset = glyph_offset(glyph_index);

    auto const x_min = static_cast<float>(get<std::int16_t>(offset + 2));
    auto const y_min = static_cast<float>(get<std::int16_t>(offset + 4));
    auto const x_max = static_cast<float>(get<std::int16_t>(offset + 6));
    auto const y_max = static_cast<float>(get<std::int16_t>(offset + 8));

    return {lsb, adv, x_min, y_min, x_max, y_max};
}

float typeface::kerning(std::uint16_t glyph1, std::uint16_t glyph2) const
{
    if(m_kern == 0)
    {
        return 0.0f;
    }

    auto const table = m_kerning_tables.find(glyph1);
    if(table == std::cend(m_kerning_tables))
    {
        return 0.0f;
    }

    auto const val = table->second.find(glyph2);
    if(val == std::cend(table->second))
    {
        return 0.0f;
    }

    return val->second;
}

std::uint16_t typeface::format0_glyph_index(int codepoint) const
{
    auto const length = get<std::uint16_t>(m_cmap_index + 2);
    if(codepoint < length-6)
    {
        return get<std::uint8_t>(m_cmap_index + 6 + codepoint);
    }
    return 0;
}

std::uint16_t typeface::format4_glyph_index(int codepoint) const
{
    if(codepoint > 0xFFFF) // Format 4 only handles BPM
    {
        return 0;
    }

    auto const seg_count = get<std::uint16_t>(m_cmap_index+6) >> 1;
    auto search_range = get<std::uint16_t>(m_cmap_index+8);
    auto entry_selector = get<std::uint16_t>(m_cmap_index+10);
    auto const range_shift = get<std::uint16_t>(m_cmap_index+12);

    auto const end_count = m_cmap_index + 14;
    auto search = end_count;    

    if(codepoint > get<std::uint16_t>(search + range_shift))
    {
        search += range_shift;
    }

    search -= 2;

    while(entry_selector)
    {
        search_range >>= 1;
        if(codepoint > get<std::uint16_t>(search + search_range))
            search += search_range;
        --entry_selector;
    }

    search += 2;

    auto const end_code = m_cmap_index + 14;
    auto const item = (search - end_count) >> 1;
    auto const start = get<std::uint16_t>(end_code + seg_count*2 + 2 + 2*item);
    if(codepoint < start)
    {
        return 0;
    }

    auto const offset = get<std::uint16_t>(end_code + seg_count*6 + 2 + 2*item);

    if(offset == 0)
    {
        return
            codepoint +
            get<std::uint16_t>(end_code + seg_count*4 + 2 + 2*item);
    }

    return get<std::uint16_t>(
        end_code + offset + (codepoint-start)*2 + seg_count*6 + 2 + 2*item);
}

std::uint16_t typeface::format6_glyph_index(int codepoint) const
{
    auto const first_code = get<std::uint16_t>(m_cmap_index + 6);
    auto const entry_count = get<std::uint16_t>(m_cmap_index + 8);

    if(codepoint >= first_code && codepoint < (first_code + entry_count))
    {
        return get<std::uint16_t>(
            m_cmap_index + 10 + (codepoint - first_code)*2);
    }

    return 0;
}

std::uint32_t typeface::format0_glyph_offset(std::uint16_t glyph_index) const
{
    auto const g1 = m_glyf + get<std::uint16_t>(m_loca + glyph_index * 2) * 2;
    auto const g2 = m_glyf + get<std::uint16_t>(m_loca + glyph_index * 2 + 2) * 2;

    return g1==g2 ? 0 : g1;
}

std::uint32_t typeface::format1_glyph_offset(std::uint16_t glyph_index) const
{
    auto const g1 = m_glyf + get<std::uint32_t>(m_loca + glyph_index * 4);
    auto const g2 = m_glyf + get<std::uint32_t>(m_loca + glyph_index * 4 + 4);

    return g1==g2 ? 0 : g1;
}

std::uint32_t typeface::glyph_offset(std::uint16_t glyph_index) const
{
    if(glyph_index >= m_num_glyphs || m_glyph_offset_fn == nullptr)
    {
        return 0;
    }

    return std::invoke(m_glyph_offset_fn, this, glyph_index);
}


std::uint32_t typeface::find_table(char const * tag) const
{
    auto const t = tag_from_c_string(tag);
    auto const num_tables = get<std::uint16_t>(m_data_offset+4);

    for(auto i = 0u; i != num_tables; ++i)
    {
        auto const offs = m_data_offset + 12 + (i*table_entry::byte_size);
        auto const entry = get<table_entry>(offs);
        if(entry.tag == t)
        {
            return entry.offset;
        }
    }

    return 0u;
}

shape typeface::simple_glyph_shape(std::uint32_t const glyph_offset) const
{
    auto const gh = get<glyph_header>(glyph_offset);

    struct vertex
    {
        std::int16_t x{0};
        std::int16_t y{0};
        std::uint8_t flags{0};
    };

    auto const end_pts_of_countours_offset =
        glyph_offset + glyph_header::byte_size;
    auto const instruction_length = get<std::uint16_t>(
        end_pts_of_countours_offset + gh.number_of_contours * 2);

    auto end_pts = m_data->create_cursor(end_pts_of_countours_offset);
    auto points = m_data->create_cursor(
            end_pts_of_countours_offset +
            gh.number_of_contours*2 +
            2 +
            instruction_length);

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

shape typeface::composite_glyph_shape(std::uint32_t const glyph_offset) const
{
    auto data = m_data->create_cursor(glyph_offset+10);

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

}
