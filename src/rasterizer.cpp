#include <wttf/rasterizer.hpp>
#include <wttf/assert.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>
#include <utility>

// #define WTTF_NO_ANTIALIASING 1

namespace wttf
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

    void rasterize(shape const & s, float x_offset, float y_offset) const;

    private:
    struct line_segment
    {
        float x1;
        float y1;
        float x2;
        float y2;
        int winding;
    };

    std::vector <line_segment> create_lines(
        shape const & s, float x, float y) const;
    void rasterize_scanlines(
        std::size_t const start_x, std::size_t const end_x,
        std::size_t const start_y, std::size_t const end_y,
        std::vector<line_segment> const & lines) const;

#ifndef WTTF_NO_ANTIALIASING
    struct edge_info
    {
        float x1;
        float x2;
        float winding_height;

        float coverage(float const x) const
        {
            if(x > x2)
            {
                return winding_height;
            }
            else if(x+1.0f < x1)
            {
                return 0.0f;
            }
            else
            {
                auto const tdx = x2-x1;
                if(tdx < std::numeric_limits<float>::epsilon())
                {
                    return winding_height * ((x+1.0f) - x2);
                }

                auto const ix1 = std::clamp(x, x1, x2);
                auto const ix2 = std::clamp(x+1.0f, x1, x2);
                auto const dx1 = ix2-ix1;
                auto const dx2 = (x+1.0f)-ix2;
                auto const h1 = winding_height * (ix1 - x1)/tdx;
                auto const h2 = winding_height * (ix2 - x1)/tdx;
                auto const avg_h = (h1+h2) / 2.0f;
                return (avg_h*dx1) + (winding_height*dx2);
            }
        }
    };

    edge_info clip(float const y1, line_segment seg) const;
#endif

    std::uint8_t * m_image{nullptr};
    std::size_t m_width{0};
    std::size_t m_height{0};
    std::ptrdiff_t m_stride{0};
}; /* class rasterizer::implementation */

void rasterizer::implementation::rasterize(
    shape const & s, float x_offset, float y_offset) const
{
    auto const start_x = std::max(0.0f, std::floor(s.min_x() + x_offset));
    auto const start_y = std::max(0.0f, std::floor(s.min_y() + y_offset));
    auto const end_x = std::min(
        static_cast<float>(m_width), std::ceil(s.max_x() + x_offset));
    auto const end_y = std::min(
        static_cast<float>(m_height), std::ceil(s.max_y() + y_offset));

    // Early exit, if shape is out of bounds
    if(start_x >= end_x || start_y >= end_y)
        return;

    auto const & lines = create_lines(s, x_offset, y_offset);
    rasterize_scanlines(
        static_cast<std::size_t>(start_x), static_cast<std::size_t>(end_x),
        static_cast<std::size_t>(start_y), static_cast<std::size_t>(end_y),
        lines);
}

std::vector<rasterizer::implementation::line_segment>
rasterizer::implementation::create_lines(
    shape const & s,
    float x_offset, float y_offset) const
{
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

            lines.push_back({
                v1.x+x_offset, v1.y+y_offset,
                v2.x+x_offset, v2.y+y_offset,
                -1});
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

    return lines;
}

#ifdef WTTF_NO_ANTIALIASING
void rasterizer::implementation::rasterize_scanlines(
        std::size_t start_x, std::size_t end_x,
        std::size_t start_y, std::size_t end_y,
        std::vector<line_segment> const & lines) const
{
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

            m_image[cy * m_stride + cx] =
                (winding != 0) ? 0xFF : 0;
        }
    }
}

#else /* WTTF_NO_ANTIALIASING */

void rasterizer::implementation::rasterize_scanlines(
        std::size_t const start_x, std::size_t const end_x,
        std::size_t const start_y, std::size_t const end_y,
        std::vector<line_segment> const & lines) const
{
    std::vector<edge_info> scanline_buffer;
    auto line_it = std::cbegin(lines);
    for(auto cy = start_y; cy < end_y; ++cy)
    {
        auto const fcy = static_cast<float>(cy);

        scanline_buffer.clear();
        auto const pred = [cy=static_cast<float>(cy)](auto const & l)
        {
            return l.y2 > cy;
        };
        line_it = std::find_if(line_it, std::cend(lines), pred);

        for(auto i = line_it; i != std::cend(lines); ++i)
        {
            if(i->y1 >= (fcy+1.0f))
                continue;
            scanline_buffer.push_back(clip(fcy, *i));
        }

        auto const compare_edge = [](auto const & a, auto const & b)
        {
            return a.x2 < b.x2;
        };


        std::sort(
            std::begin(scanline_buffer), std::end(scanline_buffer),
            compare_edge);

        auto coverage1 = 0.0f;
        auto sbuf_it = std::cbegin(scanline_buffer);

        for(auto cx = start_x; cx < end_x;)
        {
            auto const fcx = static_cast<float>(cx);

            while(
                sbuf_it != std::cend(scanline_buffer) &&
                sbuf_it->x2 < fcx)
            {
                coverage1 += sbuf_it->coverage(fcx);
                ++sbuf_it;
            }

            auto coverage2 = 0.0f;
            auto next_x1 = static_cast<float>(end_x);
            for(auto it = sbuf_it; it != std::cend(scanline_buffer); ++it)
            {
                if((fcx+1.0f) >=it->x1)
                {
                    coverage2 += it->coverage(fcx);
                    next_x1 = fcx+1.0f;
                }
                else
                {
                    next_x1 = std::min(next_x1, it->x1);
                }
            }

            next_x1 = std::floor(next_x1);
            WTTF_ASSERT(next_x1 > fcx);

            auto const next_cx = static_cast<std::size_t>(next_x1);
            auto const out_count = next_cx - cx;

            auto const coverage = coverage1 + coverage2;
            auto const w = std::clamp(std::abs(coverage), 0.0f, 1.0f);
            auto const out = std::min(255, static_cast<int>(w * 255.0f));

            auto const start_of_row =
                static_cast<std::ptrdiff_t>(cy) * m_stride;
            auto const offset = static_cast<std::size_t>(start_of_row) + cx;
            std::fill_n(&m_image[offset], out_count, out);

            cx = next_cx;
        }
    }
}

rasterizer::implementation::edge_info
rasterizer::implementation::clip(float const y1, line_segment seg) const
{
    auto const y2 = y1+1.0f;
    WTTF_ASSERT(seg.y2 > y1 && seg.y1 < y2);

    auto const dy = seg.y2 - seg.y1;
    auto const fdx = (seg.x2 - seg.x1)/dy;

    if(seg.y1 < y1)
    {
        seg.x1 += fdx * (y1 - seg.y1);
        seg.y1 = y1;
    }

    if(seg.y2 > y2)
    {
        seg.x2 += fdx * (y2 - seg.y2);
        seg.y2 = y2;
    }

    auto const x1 = std::min(seg.x1, seg.x2);
    auto const x2 = std::max(seg.x1, seg.x2);
    auto const h = (seg.y2 - seg.y1) * static_cast<float>(seg.winding);

    return {x1, x2, h};
}
#endif /* WTTF_NO_ANTIALIASING */

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

void rasterizer::rasterize(
    shape const & s, float x_offset, float y_offset) const
{
    if(!m_impl)
        return;

    if(!s.flat())
    {
        rasterize(s.flatten(0.45f), x_offset, y_offset);
        return;
    }

    m_impl->rasterize(s, x_offset, y_offset);
}

} /* namespace wttf */
