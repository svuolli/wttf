#ifndef TTF_GLYPH_METRICS_HPP
#define TTF_GLYPH_METRICS_HPP

#include "export.hpp"

namespace ttf
{

struct WTTF_EXPORT font_metrics
{
    float ascent;
    float descent;
    float line_gap;

    constexpr font_metrics scaled(float s)
    {
        return {ascent * s, descent * s, line_gap * s};
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

    constexpr glyph_metrics scaled(float s)
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
};

} /* namespace ttf */

#endif /* TTF_GLYPH_METRICS_HPP */
