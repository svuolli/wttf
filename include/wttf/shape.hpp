#ifndef WTTF_SHAPE_HPP
#define WTTF_SHAPE_HPP

#include "export.hpp"
#include "transform.hpp"

#include <vector>

namespace wttf
{

class WTTF_EXPORT shape
{
    public:
    struct vertex
    {
        float x;
        float y;
        bool on_curve;
    };

    using contour_t = std::vector<vertex>;

    shape() = default;
    shape(shape const &) = default;
    shape(shape &&) = default;
    shape(
        float min_x, float min_y,
        float max_x, float max_y,
        std::size_t contours = 0);
    shape(shape const & other, wttf::transform const & t);

    ~shape() = default;

    shape & operator=(shape const &) = default;
    shape & operator=(shape &&) = default;

    [[nodiscard]] std::size_t num_contours() const 
    {
        return m_contours.size();
    }

    [[nodiscard]] bool empty() const { return m_contours.empty(); }

    [[nodiscard]] contour_t const & contour(std::size_t i) const
    {
        return m_contours[i];
    }

    [[nodiscard]] float min_x() const { return m_min_x; }
    [[nodiscard]] float min_y() const { return m_min_y; }
    [[nodiscard]] float max_x() const { return m_max_x; }
    [[nodiscard]] float max_y() const { return m_max_y; }
    [[nodiscard]] float width() const { return m_max_x - m_min_x; }
    [[nodiscard]] float height() const { return m_max_y - m_min_y; }
    [[nodiscard]] bool flat() const { return m_flat; }

    void add_contour(std::size_t s = 0);
    void add_vertex(float x, float y, bool on_curve);
    void add_shape(shape const & s, transform const & t = {});
    void transform(wttf::transform const & t);

    [[nodiscard]] shape flatten(float const flatness) const;

    [[nodiscard]] auto begin() const { return m_contours.begin(); }
    [[nodiscard]] auto end() const { return m_contours.end(); }

    private:
    void add_tesselated_curve(
        float flatness,
        float x0, float y0,
        float x1, float y1,
        float x2, float y2,
        bool add_last_point = true);

    std::vector<contour_t> m_contours;
    float m_min_x;
    float m_min_y;
    float m_max_x;
    float m_max_y;
    bool m_flat{true};
};

} /* namespace wttf */

#endif /* WTTF_SHAPE_HPP */
