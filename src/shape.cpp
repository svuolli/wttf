#include <ttf/assert.hpp>
#include <ttf/shape.hpp>

namespace ttf
{

shape::shape(
    float min_x, float min_y, float max_x, float max_y, std::size_t contours):
    m_contours{},
    m_min_x{min_x}, m_min_y{min_y},
    m_max_x{max_x}, m_max_y{max_y}
{
    if(contours)
    {
        m_contours.reserve(contours);
    }
}

shape::shape(shape const & other, ttf::transform const & t)
{
    add_shape(other, t);
}

void shape::add_contour(std::size_t s)
{
    m_contours.emplace_back();

    if(s)
    {
        m_contours.back().reserve(s);
    }
}

void shape::add_vertex(float x, float y, bool on_curve)
{
    TTF_ASSERT(on_curve || !empty());
    m_flat &= on_curve;
    m_contours.back().push_back({x, y, on_curve});
}

void shape::add_shape(shape const & s, ttf::transform const & t)
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

void shape::transform(ttf::transform const & t)
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

shape shape::flatten(float const flatness) const
{
    if(m_flat) return *this;

    auto result = shape{
        m_min_x, m_min_y, m_max_x, m_max_y, m_contours.size()};

    for(auto & cont: m_contours)
    {
        result.add_contour(cont.size());
        auto prev_on_curve = true;
        auto cx = 0.0f;
        auto cy = 0.0f;
        auto ex = 0.0f;
        auto ey = 0.0f;

        for(auto & v: cont)
        {
            if(v.on_curve)
            {
                if(prev_on_curve)
                {
                    result.add_vertex(v.x, v.y, true);
                }
                else
                {
                    result.add_tesselated_curve(
                        flatness, ex, ey, cx, cy, v.x, v.y);
                }

                ex = v.x;
                ey = v.y;
            }
            else
            {
                if(!prev_on_curve)
                {
                    auto const nx = (v.x+cx)/2.0f;
                    auto const ny = (v.y+cy)/2.0f;
                    result.add_tesselated_curve(
                        flatness, ex, ey, cx, cy, nx, ny);
                    ex = nx;
                    ey = ny;
                }

                cx = v.x;
                cy = v.y;
            }
            prev_on_curve = v.on_curve;
        }

        if(!prev_on_curve)
        {
            auto & v = cont[0];
            result.add_tesselated_curve(
                flatness, ex, ey, cx, cy, v.x, v.y, false);
        }
    }

    return result;
}

void shape::add_tesselated_curve(
    float flatness,
    float x0, float y0,
    float x1, float y1,
    float x2, float y2,
    bool add_end_point)
{
    // Middle of curve
    auto const mx = (x0 + 2.0f*x1 + x2) / 4.0f;
    auto const my = (y0 + 2.0f*y1 + y2) / 4.0f;

    // Vector from middle of curve to direct line
    auto const dx = (x0+x2)/2.0f - mx;
    auto const dy = (y0+y2)/2.0f - my;

    if(dx*dx+dy*dy > flatness)
    {
        add_tesselated_curve(flatness, x0, y0, (x0+x1)/2.0f, (y0+y1)/2.0f, mx, my);
        add_tesselated_curve(flatness, mx, my, (x1+x2)/2.0f, (y1+y2)/2.0f, x2, y2, add_end_point);
    }
    else if(add_end_point)
    {
        add_vertex(x2, y2, true);
    }
}

} /* namespace */

