#include <ttf/rasterizer.hpp>

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <utility>

namespace ttf
{

/* Class: rasterizer::implementation */
class rasterizer::implementation
{
    public:
    implementation() = default;
    implementation(implementation const &) = default;
    implementation(implementation &&) = default;

    implementation(
        std::uint8_t * image,
        std::size_t width,
        std::size_t height,
        std::ptrdiff_t stride):
        m_image{image},
        m_width{width},
        m_height{height},
        m_stride{stride}
    {}

    void rasterize(shape const & s, int x, int y) const;

    private:
    struct line_segment
    {
        float x1;
        float y1;
        float x2;
        float y2;
        int winding;
    };

    std::uint8_t * m_image{nullptr};
    std::size_t m_width{0};
    std::size_t m_height{0};
    std::ptrdiff_t m_stride{0};
};

void rasterizer::implementation::rasterize(
    shape const & s, int x, int y) const
{
    auto const start_x = std::max(-x, static_cast<int>(std::floorf(s.min_x())));
    auto const start_y = std::max(-y, static_cast<int>(std::floorf(s.min_y())));
    auto const end_x = std::min(
        static_cast<int>(m_width) - x,
        static_cast<int>(std::ceilf(s.max_x())));
    auto const end_y = std::min(
        static_cast<int>(m_height) - y,
        static_cast<int>(std::ceilf(s.max_y())));

    // Early exit, if shape is out of bounds
    if(start_x >= end_x || start_y >= end_y)
        return;

    auto num_lines = std::size_t{0};
    for(auto const & c: s)
    {
        num_lines += c.size();
    }

    std::vector<line_segment> lines;
    lines.reserve(num_lines);

    for(auto const & contour: s)
    {
        for(auto i = 0u; i != contour.size(); ++i)
        {
            auto const & v1 = contour[i];
            auto const & v2 = contour[(i+1) % contour.size()];

            // Ignore horizontal lines
            if(v1.y == v2.y) 
            {
                continue;
            }

            lines.push_back({v1.x, v1.y, v2.x, v2.y, -1});
            auto & l = lines.back();
            if(l.y1 > l.y2)
            {
                std::swap(l.x1, l.x2);
                std::swap(l.y1, l.y2);
                l.winding = 1;
            }
        }
    }

    // Sort lines by their top position
    auto const compare_line = [](auto const & a, auto const & b)
    {
        return a.y2 < b.y2;
    };

    std::sort(std::begin(lines), std::end(lines), compare_line);

    auto line_it = std::cbegin(lines);
    std::vector<std::pair<float, int>> scanline_buffer;
    for(auto cy = start_y; cy < end_y; ++cy)
    {
        scanline_buffer.clear();
        auto const pred = [cy=static_cast<float>(cy)](auto const & l)
        {
            return l.y2 >= cy;
        };
        line_it = std::find_if(line_it, std::cend(lines), pred);

        for(auto i = line_it; i != std::cend(lines); ++i)
        {
            if(i->y1 >= static_cast<float>(cy))
                continue;

            auto const d = (cy - i->y1) / (i->y2 - i->y1);
            auto const x = i->x1 + d*(i->x2 - i->x1);
            scanline_buffer.emplace_back(x, i->winding);
        }

        auto const compare_sline = [](auto const & a, auto const & b)
        {
            return a.first < b.first;
        };
        std::sort(
            std::begin(scanline_buffer), std::end(scanline_buffer),
            compare_sline);

        auto const out_y = cy + y;
        auto winding = 0;
        auto sbuf_it = std::cbegin(scanline_buffer);
        for(auto cx = start_x; cx < end_x; ++cx)
        {
            while(
                sbuf_it != std::cend(scanline_buffer) &&
                sbuf_it->first <= static_cast<float>(cx))
            {
                winding += sbuf_it->second;
                ++sbuf_it;
            }

            auto const out_x = cx + x;
            m_image[out_y * m_stride + out_x] =
                (winding != 0) ? 0xFF : 0;
        }
    }
}

/* Class: rasterizer */
rasterizer::rasterizer() = default;
rasterizer::rasterizer(rasterizer &&) = default;

rasterizer::rasterizer(
    std::uint8_t * image,
    std::size_t width,
    std::size_t height,
    std::ptrdiff_t stride):
    m_impl{std::make_unique<implementation>(image, width, height, stride)}
{}

rasterizer::~rasterizer() = default;

rasterizer & rasterizer::operator=(rasterizer &&) = default;

void rasterizer::rasterize(shape const & s, int x, int y) const
{
    if(!m_impl)
        return;

    if(!s.flat())
    {
        rasterize(s.flatten(0.45f), x, y);
        return;
    }

    m_impl->rasterize(s, x, y);
}

} /* namespace ttf */
