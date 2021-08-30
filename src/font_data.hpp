#ifndef TTF_FONT_DATA_HPP
#define TTF_FONT_DATA_HPP

#include <ttf/assert.hpp>

#include <array>
#include <cstddef>
#include <vector>
#include <utility>

namespace ttf
{

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

inline tag_t tag_from_c_string(char const * str)
{
    TTF_ASSERT(std::strlen(str) == 4);

    auto begin = reinterpret_cast<std::uint8_t const *>(str);
    return tag_t{begin[0], begin[1], begin[2], begin[3]};
}

struct font_data
{
    struct cursor
    {
        cursor(font_data const & own, std::size_t offs):
            owner(own), offset(offs)
        {}

        cursor operator+=(std::size_t offs)
        {
            offset += offs;
            return *this;
        }

        template <typename T>
        T peek(std::size_t offs = 0) const
        {
            return owner.get<T>(offset+offs);
        }

        template <typename T>
        T read()
        {
            auto const offs = offset;
            offset += sizeof(T);
            return owner.get<T>(offs);
        }

        font_data const & owner;
        std::size_t offset;
    };

    font_data() = delete;
    font_data(font_data const &) = default;
    font_data(font_data &&) = delete;

    font_data(std::vector<std::byte> && data):
        bytes{std::move(data)}
    {}

    ~font_data() = default;

    font_data & operator=(font_data const &) = delete;
    font_data & operator=(font_data &&) = delete;

    cursor create_cursor(std::size_t offset) const
    {
        return cursor{*this, offset};
    }

    template <typename T>
    T get(std::size_t offset) const
    {
        using U = std::make_unsigned_t<T>;
        static constexpr auto size = sizeof(T);

        auto v = U{0};
        for(auto i = offset; i != offset + size; ++i)
        {
            v = (v<<8) | std::to_integer<U>(bytes[i]);
        }

        return static_cast<T>(v);
    }

    template<>
    tag_t get<tag_t>(std::size_t offset) const
    {
        return tag_t{
            get<std::uint8_t>(offset+0),
            get<std::uint8_t>(offset+1),
            get<std::uint8_t>(offset+2),
            get<std::uint8_t>(offset+3)};
    }

    template<>
    table_entry get<table_entry>(std::size_t offset) const
    {
        return table_entry{
            get<tag_t>(offset+0),
            get<std::uint32_t>(offset+4),
            get<std::uint32_t>(offset+8),
            get<std::uint32_t>(offset+12)};
    }

    template<>
    encoding_record get<encoding_record>(std::size_t offset) const
    {
        return encoding_record{
            get<platform_id>(offset+0),
            get<std::uint16_t>(offset+2),
            get<std::uint32_t>(offset+4) };
    }

    template<>
    glyph_header get<glyph_header>(std::size_t offset) const
    {
        return glyph_header{
            get<std::int16_t>(offset+0),
            get<std::int16_t>(offset+2),
            get<std::int16_t>(offset+4),
            get<std::int16_t>(offset+6),
            get<std::int16_t>(offset+8)};
    }

    std::vector<std::byte> const bytes;
};

} /* namespace ttf */

#endif /* TTF_FONT_DATA_HPP */
