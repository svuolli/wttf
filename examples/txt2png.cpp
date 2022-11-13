#include "pngsaver.hpp"

#include <wttf/typeface.hpp>
#include <wttf/rasterizer.hpp>

#include <unicode/unistr.h>
#include <fmt/format.h>
#include <png.h>

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{

wttf::typeface load_font(std::filesystem::path const & file)
{
    using iterator = std::istreambuf_iterator<char>;
    std::ifstream fs{file, std::ios::binary};
    auto contents = std::vector<std::byte>{};
    std::transform(
        iterator{fs}, iterator{}, std::back_inserter(contents),
        [](auto c) { return static_cast<std::byte>(c); });

    return wttf::typeface{std::move(contents)};
}

icu::UnicodeString load_text_file(std::filesystem::path const & file)
{
    using iterator = std::istreambuf_iterator<char>;
    std::ifstream fs{file, std::ios::binary};
    auto contents = std::string{};
    std::copy(iterator{fs}, iterator{}, std::back_inserter(contents));
    return icu::UnicodeString::fromUTF8(contents);
}

struct layout_glyph
{
    UChar32 codepoint{0};
    std::size_t glyph_index{0};
    float horizontal_pos{0.0f};
};

struct layout_line
{
    std::size_t start_index{0};
    std::size_t end_index{0};
    float left_edge{0.0f};
    float right_edge{0.0f};
    std::vector<layout_glyph> glyphs;
};

struct text_layout
{
    float left_edge{std::numeric_limits<float>::max()};
    float right_edge{std::numeric_limits<float>::min()};
    std::vector<layout_line> lines;

    [[nodiscard]] constexpr float width() const
    {
        return right_edge - left_edge;
    }
};

layout_line create_line(
    icu::UnicodeString const & str, std::size_t start, std::size_t end,
    wttf::typeface const & font, float scale)
{
    auto res = layout_line{
        start, end,
        std::numeric_limits<float>::max(), std::numeric_limits<float>::min(),
        {}};
    auto const trimmed = str.tempSubString(start, end-start-1).trim();

    auto prev_glyph = std::size_t{0};
    auto h_pos = 0.0f;
    for(auto i = std::size_t{0}; i != trimmed.length(); ++i)
    {
        auto const codepoint = trimmed.char32At(i);
        auto const index = font.glyph_index(codepoint);
        auto const metrics = font.metrics(index).scaled(scale);
        res.left_edge = std::min(res.left_edge, h_pos+metrics.x_min);
        res.right_edge = std::max(res.right_edge, h_pos+metrics.x_max);
        h_pos += font.kerning(prev_glyph, index) * scale;
        res.glyphs.push_back(layout_glyph{codepoint, index, h_pos});
        prev_glyph = index;
        h_pos += metrics.advance;
    }
    
    if(res.glyphs.empty())
    {
        res.left_edge = res.right_edge = 0.0f;
    }

    return res;
}

text_layout create_text_layout(
    icu::UnicodeString const & str, wttf::typeface const & font, float scale)
{
    auto res = std::vector<layout_line>{};
    auto left_edge = std::numeric_limits<float>::max();
    auto right_edge = std::numeric_limits<float>::min();
    auto line_start = std::size_t{0};

    for(auto curr_pos = std::size_t{0}; curr_pos != str.length(); ++curr_pos)
    {
        auto const c = str.char32At(curr_pos);
        if(c == '\n')
        {
            res.push_back(
                create_line(str, line_start, curr_pos, font, scale));
            line_start = curr_pos+1;
            left_edge = std::min(left_edge, res.back().left_edge);
            right_edge = std::max(right_edge, res.back().right_edge);
        }
    }

    // Add last line
    if(line_start != str.length())
    {
        res.push_back(
            create_line(str, line_start, str.length(), font, scale));
        left_edge = std::min(left_edge, res.back().left_edge);
        right_edge = std::max(right_edge, res.back().right_edge);
    }

    return {left_edge, right_edge, std::move(res)};
}

wttf::shape create_shape(
    text_layout const & layout, wttf::typeface const & font, float scale)
{
    auto const metrics = font.metrics().scaled(scale);
    auto const shape_width = std::ceil(layout.width());
    auto const shape_height = std::ceil(
        (metrics.height() * layout.lines.size()) +
        (metrics.line_gap * (layout.lines.size()-1)));
    auto const scale_matrix = wttf::matrix_2x2{scale, 0.0f, 0.0f, scale};

    auto res = wttf::shape{0.0f, 0.0f, shape_width, shape_height, 1};
    auto v_pos =
        -metrics.descent + (layout.lines.size() * metrics.linespace());

    for(auto const & line: layout.lines)
    {
        v_pos -= metrics.linespace();
        auto const line_width = line.right_edge - line.left_edge;
        auto const indent = (shape_width - line_width) / 2.0f;
        for(auto const & g: line.glyphs)
        {
            auto const transform =
                wttf::transform{scale_matrix, indent+g.horizontal_pos, v_pos};
            auto const s = font.glyph_shape(g.glyph_index);
            res.add_shape(s, transform);
        }
    }

    return res;
}

} /* namespace */

int main(int argc, char const * argv[])
{
    if(argc != 5)
    {
        std::cerr << "err\n";
        return 1;
    }

    auto const typeface = load_font(argv[1]);
    auto const font_size = std::stof(argv[2]);
    auto const text = load_text_file(argv[3]);
    auto const scale = font_size / typeface.metrics().height();
    auto const layout = create_text_layout(text, typeface, scale);
    auto const shape = create_shape(layout, typeface, scale);

    auto const image_width =
        static_cast<std::size_t>(std::ceil(shape.width()));
    auto const image_height =
        static_cast<std::size_t>(std::ceil(shape.height()));

    auto image_data = std::vector<std::uint8_t>{};
    image_data.resize(image_width*image_height);

    auto const rasterizer = wttf::rasterizer{
        image_data.data(), image_width, image_height,
        static_cast<std::ptrdiff_t>(image_width)};

    rasterizer.rasterize(shape, -shape.min_x(), -shape.min_y());
    save_png(argv[4], image_data.data(), image_width, image_height);

    fmt::print(
        "shape bounding box: ({}, {}), ({}, {})\n",
        shape.min_x(), shape.min_y(),
        shape.max_x(), shape.max_y());

    fmt::print("Image size: {}, {}\n", image_width, image_height);

    return 0;
}
