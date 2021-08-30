#ifndef TTF_TYPEFACE_HPP
#define TTF_TYPEFACE_HPP

#include <ttf/shape.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace ttf
{

struct font_data;

class typeface;

class font_collection
{
    public:
    font_collection();
    font_collection(font_collection const & other);
    font_collection(font_collection && other) = delete;

    font_collection(std::vector<std::byte> && data);

    font_collection & operator=(font_collection const & other) = default;
    font_collection & operator=(font_collection && other) = delete;

    [[nodiscard]] std::size_t num_fonts() const;

    private:
    friend class typeface;
    std::shared_ptr<font_data const> m_data;
};

class typeface
{
    public:
    typeface();
    typeface(typeface const & other);
    typeface(typeface && other) = delete;

    typeface(std::vector<std::byte> && data);
    typeface(font_collection const & collection, std::size_t index);

    ~typeface();

    typeface & operator=(typeface const & other);
    typeface & operator=(typeface && other) = delete;

    [[nodiscard]] std::size_t glyph_index(int codepoint) const;
    [[nodiscard]] shape glyph_shape(std::uint16_t index) const;

    private:
    typeface(std::shared_ptr<font_data const> const & data, std::size_t offset);

    using glyph_index_fn_t = std::uint16_t (typeface::*)(int) const;
    using glyph_offset_fn_t = std::uint32_t (typeface::*)(std::uint16_t) const;

    template <typename T> T get(std::size_t offset) const;

    [[nodiscard]] std::uint32_t find_table(char const * tag) const;

    std::uint16_t format0_glyph_index(int codepoint) const;
    std::uint16_t format4_glyph_index(int codepoint) const;
    std::uint16_t format6_glyph_index(int codepoint) const;

    std::uint32_t format0_glyph_offset(std::uint16_t glyph_index) const;
    std::uint32_t format1_glyph_offset(std::uint16_t glyph_index) const;
    [[nodiscard]] std::uint32_t glyph_offset(std::uint16_t glyph_index) const;

    [[nodiscard]] shape
    simple_glyph_shape(std::uint32_t const glyph_offset) const;

    [[nodiscard]] shape
    composite_glyph_shape(std::uint32_t const glyph_offset) const;

    std::shared_ptr<font_data const> m_data;
    glyph_index_fn_t m_glyph_index_fn{nullptr};
    glyph_offset_fn_t m_glyph_offset_fn{nullptr};
    std::size_t m_data_offset{0};
    std::uint32_t m_cmap_index{0};
    std::uint32_t m_loca{0};
    std::uint32_t m_glyf{0};
    std::uint16_t m_num_glyphs{0};
};

} /* namespace ttf */

#endif /* TTF_TYPEFACE_HPP */
