#ifndef WTTF_TRANSFORM_HPP
#define WTTF_TRANSFORM_HPP

#include "export.hpp"

#include <array>
#include <cmath>
#include <utility>

namespace wttf
{

struct WTTF_EXPORT point
{
    float x;
    float y;
};

struct WTTF_EXPORT transform
{
    std::array<float, 6> matrix;

    static transform from_scale_translate(point scale, point translate)
    {
        return {scale.x, 0.0f, 0.0f, scale.y, translate.x, translate.y};
    }

    static transform from_scale_translate(float scale, point translate)
    {
        return {scale, 0.0f, 0.0f, scale, translate.x, translate.y};
    }

    point apply(float const x, float const y) const
    {
        auto ox = matrix[0]*x + matrix[2]*y + matrix[4];
        auto oy = matrix[1]*x + matrix[3]*y + matrix[5];

        return {ox, oy};
    }
}; /* struct transform */

} /* namespace wttf */

#endif /* WTTF_TRANSFORM_HPP */
