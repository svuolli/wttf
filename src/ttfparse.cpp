#include <ttf/transform.hpp>
#include <ttf/shape.hpp>
#include <ttf/typeface.hpp>
#include <ttf/rasterizer.hpp>

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

void draw_contour(ttf::shape::contour_t const & c)
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

}

int main(int argc, char const * argv[])
{
    if(argc < 3)
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

    auto fnt = ttf::typeface{std::move(contents)};
    auto const s_b = fnt.glyph_shape(fnt.glyph_index('@'));
    auto const s = s_b.flatten(0.5f);

    /*
    fmt::print(html_head, (s.width()+1)/2, (s.height()+2)/2, -s.min_x(), -s.max_y());

    for(auto i=0; i < s.num_contours(); ++i)
    {
        draw_contour(s.contour(i));
    }

    fmt::print(html_foot);
    */

    auto const w = static_cast<std::size_t>(s.width() + 1);
    auto const h = static_cast<std::size_t>(s.height() + 1);
    std::vector<std::uint8_t> img;
    img.resize(w*h);
    ttf::rasterizer r{img.data(), w, h, static_cast<std::ptrdiff_t>(w)};
    r.rasterize(s, -s.min_x(), -s.min_y());

    std::ofstream out{argv[2], std::ios::binary};
    std::transform(
        std::cbegin(img), std::cend(img),
        std::ostream_iterator<char>{out},
        [](auto c) { return static_cast<char>(c); });

    fmt::print("Image {}x{}\n", w, h);

    return 0;
}
