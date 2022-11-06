#ifndef WTTF_GLYPH_METRICS_HPP
#define WTTF_GLYPH_METRICS_HPP

#include "export.hpp"

namespace wttf
{

struct WTTF_EXPORT font_metrics
{
    float ascent;
    float descent;
    float line_gap;

    [[nodiscard]] constexpr font_metrics scaled(float s) const noexcept
    {
        return {ascent * s, descent * s, line_gap * s};
    }

    [[nodiscard]] constexpr float height() const noexcept
    {
        return ascent - descent;
    }

    [[nodiscard]] constexpr float linespace() const noexcept
    {
        return height() + line_gap;
    }
};

struct WTTF_EXPORT glyph_metrics
{
    float left_side_bearing;
    float advance;
    float x_min;
    float y_min;
    float x_max;
    float y_max;

    [[nodiscard]] constexpr glyph_metrics scaled(float s) const noexcept
    {
        return {
            left_side_bearing * s,
            advance * s,
            x_min * s,
            y_min * s,
            x_max * s,
            y_max * s
        };
    }

    [[nodiscard]] constexpr float bb_width() const noexcept
    {
        return x_max - x_min;
    }

    [[nodiscard]] constexpr float bb_height() const noexcept
    {
        return y_max - y_min;
    }
};

} /* namespace wttf */

#endif /* WTTF_GLYPH_METRICS_HPP */
