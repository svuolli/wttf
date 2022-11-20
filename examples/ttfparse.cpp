#include <wttf/transform.hpp>
#include <wttf/shape.hpp>
#include <wttf/typeface.hpp>
#include <wttf/rasterizer.hpp>

#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

#include <unicode/unistr.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef WTTF_ASSERT
#ifndef NDEBUG
#include <cassert>
#define WTTF_ASSERT(x) assert(x)
#else
#define WTTF_ASSERT(x)
#endif
#endif

/*
 * References:
 *  https://docs.microsoft.com/en-us/typography/opentype/spec/otff
 *  https://developer.apple.com/fonts/TrueType-Reference-Manual/RM01/Chap1.html
 *  https://github.com/nothings/stb/blob/master/stb_truetype.h
 */

namespace 
{

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

void draw_contour(wttf::shape::contour_t const & c)
{
    // fmt::print("ctx.beginPath();\n");
    fmt::print("\nctx.moveTo({}, {});\n", c[0].x, c[0].y);

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

wttf::shape draw_text(wttf::typeface const & typeface, std::string const & str)
{
    auto const start = std::chrono::high_resolution_clock::now();
    if(str.empty())
        return {};

    auto result = wttf::shape{};
    auto constexpr scale = 1.0f/25.0f;
    auto h_pos = 0.0f;
    auto prev_glyph = std::uint16_t{0};
    auto total_kern = 0.0f;

    auto const unicode_string = icu::UnicodeString::fromUTF8(str);

    for(auto i = 0u; i != unicode_string.length(); ++i)
    {
        auto const ch = unicode_string.char32At(i);
        auto const g_index = typeface.glyph_index(ch);
        auto const shape = typeface.glyph_shape(g_index);
        auto const metrics = typeface.metrics(g_index);
        auto const kern = typeface.kerning(prev_glyph, g_index);
        total_kern += kern;

        h_pos += kern * scale;
        result.add_shape(
            shape,
            wttf::transform::from_scale_translate(scale, {h_pos, 0.0f}));
        h_pos += metrics.advance * scale;

        prev_glyph = g_index;
    }

    auto const end = std::chrono::high_resolution_clock::now();
    auto const duration = end - start;
    fmt::print(FMT_STRING("T[draw_text] = {}\n"), duration);
    fmt::print("total kern: {}\n", total_kern);

    return result;
}

void usage(char const * program_name)
{
    std::cerr <<
        fmt::format("Usage: {} ", program_name) <<
        fmt::format(fmt::emphasis::underline, "font-file") <<
        " " <<
        fmt::format(fmt::emphasis::underline, "result-file") <<
        "\n";
}

}

int main(int argc, char const * argv[])
{
    if(argc < 3)
    {
        std::cerr << fmt::format("{}: too few arguments\n", argv[0]);
        usage(argv[0]);
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

    auto fnt = wttf::typeface{std::move(contents)};
    // auto const s_b = fnt.glyph_shape(fnt.glyph_index('@' /* 196 = Ä */));
    auto const s_b = draw_text(fnt, "Yes We Kern!");
    
#if 1
    auto const s = s_b.flatten(0.35f);
#else
    auto const scale = .01774819744869661674f;
    wttf::transform t;
    t.m[0] = scale;
    t.m[3] = scale;

    auto const s_a = wttf::shape{s_b, t};
    auto const s = s_a.flatten(0.35f);
#endif

    /*
    fmt::print(html_head, (s.width()+1)/2, (s.height()+2)/2, -s.min_x(), -s.max_y());

    for(auto i=0; i < s.num_contours(); ++i)
    {
        draw_contour(s.contour(i));
    }

    fmt::print(html_foot);
    */

    auto const extents = [](auto a, auto b)
    {
        auto const oa = static_cast<int>(std::floor(a));
        auto const ob = static_cast<int>(std::ceil(b));
        return static_cast<std::size_t>(ob-oa);
    };

    auto const w = extents(s.min_x(), s.max_x());
    auto const h = extents(s.min_y(), s.max_y());

    std::vector<std::uint8_t> img;
    img.resize(w*h);
    wttf::rasterizer r{img.data(), w, h, static_cast<std::ptrdiff_t>(w)};

    auto const t_before = std::chrono::high_resolution_clock::now();
    auto const repeats = 1000u;
    for(auto i = 0u; i < repeats; ++i)
    {
        r.rasterize(s, -s.min_x(), -std::floor(s.min_y()));
    }
    auto const t_after = std::chrono::high_resolution_clock::now();

    std::ofstream out{argv[2], std::ios::binary};
    std::transform(
        std::cbegin(img), std::cend(img),
        std::ostream_iterator<char>{out},
        [](auto c) { return static_cast<char>(c); });

    fmt::print("Image {}x{}\n", w, h);
    fmt::print("T_rasterize: {}\n", (t_after - t_before)/repeats);

    return 0;
}
