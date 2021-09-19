#ifndef WTTF_RASTERIZER_HPP
#define WTTF_RASTERIZER_HPP

#include "export.hpp"
#include "shape.hpp"

#include <cstdint>
#include <memory>

namespace wttf
{

class WTTF_EXPORT rasterizer
{
    public:
    rasterizer();
    rasterizer(rasterizer const &) = delete;
    rasterizer(rasterizer &&);

    rasterizer(
        std::uint8_t * image,
        std::size_t width,
        std::size_t height,
        std::ptrdiff_t stride);

    ~rasterizer();

    rasterizer & operator=(rasterizer const &) = delete;
    rasterizer & operator=(rasterizer &&);

    void rasterize(shape const & s, float x, float y) const;

    private:
    class implementation;

    std::unique_ptr<implementation> m_impl;
};

} /* namespace wttf */

#endif /* WTTF_RASTERIZER_HPP */
