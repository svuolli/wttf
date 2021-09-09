#ifndef TTF_RASTERIZER_HPP
#define TTF_RASTERIZER_HPP

#include "shape.hpp"

#include <cstdint>
#include <memory>

namespace ttf
{

class rasterizer
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
    // void rasterize(shape const & s, int x, int y) const;

    private:
    class implementation;

    std::unique_ptr<implementation> m_impl;
};

} /* namespace ttf */

#endif /* TTF_RASTERIZER_HPP */
