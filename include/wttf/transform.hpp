#ifndef WTTF_TRANSFORM_HPP
#define WTTF_TRANSFORM_HPP

#include "export.hpp"

#include <array>
#include <cmath>
#include <utility>

namespace wttf
{

using matrix_2x2 = std::array<float, 2*2>;

struct WTTF_EXPORT transform
{
    matrix_2x2 m{1.0f, 0.0f, 0.0f, 1.0f};
    float tx{0};
    float ty{0};

    std::pair<float, float>
    apply(float const x, float const y) const
    {
        /*
        auto const sx = std::sqrtf(m[0]*m[0] + m[1]*m[1]);
        auto const sy = std::sqrtf(m[2]*m[2] + m[3]*m[3]);
        */
        auto sx = 1.0f;
        auto sy = 1.0f;

        auto ox = sx * (m[0]*x + m[2]*y + tx);
        auto oy = sy * (m[1]*x + m[3]*y + ty);

        return {ox, oy};
    }
};

} /* namespace wttf */

#endif /* WTTF_TRANSFORM_HPP */
