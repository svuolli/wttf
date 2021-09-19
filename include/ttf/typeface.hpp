#ifndef WTTF_TYPEFACE_HPP
#define WTTF_TYPEFACE_HPP

#include "export.hpp"
#include "shape.hpp"
#include "metrics.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

namespace ttf
{

struct font_data;

class typeface;

#if WTTF_FONT_COLLECTION_IMPLEMENTED
class WTTF_EXPORT font_collection
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

    std::size_t offset(std::size_t font_index) const;
    std::shared_ptr<font_data const> m_data;
};
#endif

class WTTF_EXPORT typeface
{
    public:
    typeface();
    typeface(typeface const & other);
    typeface(typeface && other) = delete;

    typeface(std::vector<std::byte> && data);

#if WTTF_FONT_COLLECTION_IMPLEMENTED
    typeface(font_collection const & collection, std::size_t index);
#endif

    ~typeface();

    typeface & operator=(typeface const & other);
    typeface & operator=(typeface && other) = delete;

    [[nodiscard]] std::size_t glyph_index(unsigned int codepoint) const;
    [[nodiscard]] shape glyph_shape(std::uint16_t index) const;
    [[nodiscard]] glyph_metrics metrics(std::uint16_t index) const;
    [[nodiscard]] const font_metrics & metrics() const { return m_metrics; }
    [[nodiscard]] float kerning(std::uint16_t glyph1, std::uint16_t glyph2) const;

    private:
    typeface(std::shared_ptr<font_data const> const & data, std::size_t offset);

    using glyph_index_fn_t = std::uint16_t (typeface::*)(unsigned int) const;
    using glyph_offset_fn_t = std::uint32_t (typeface::*)(std::uint16_t) const;

    using kerning_table = std::map<std::uint16_t, float>;

    template <typename T> [[nodiscard]] T get(std::size_t offset) const;

    [[nodiscard]] std::uint32_t find_table(char const * tag) const;

    [[nodiscard]] std::uint16_t format0_glyph_index(unsigned int codepoint) const;
    [[nodiscard]] std::uint16_t format4_glyph_index(unsigned int codepoint) const;
    [[nodiscard]] std::uint16_t format6_glyph_index(unsigned int codepoint) const;

    [[nodiscard]] std::uint32_t
    format0_glyph_offset(std::uint16_t glyph_index) const;
    [[nodiscard]] std::uint32_t
    format1_glyph_offset(std::uint16_t glyph_index) const;
    [[nodiscard]] std::uint32_t glyph_offset(std::uint16_t glyph_index) const;

    [[nodiscard]] shape
    simple_glyph_shape(std::uint32_t const glyph_offset) const;

    [[nodiscard]] shape
    composite_glyph_shape(std::uint32_t const glyph_offset) const;

    std::shared_ptr<font_data const> m_data;
    std::map<std::uint16_t, kerning_table> m_kerning_tables{};
    glyph_index_fn_t m_glyph_index_fn{nullptr};
    glyph_offset_fn_t m_glyph_offset_fn{nullptr};
    std::size_t m_data_offset{0};
    std::uint32_t m_cmap_index{0};
    std::uint32_t m_loca{0};
    std::uint32_t m_glyf{0};
    std::uint32_t m_hmtx{0};
    std::uint32_t m_kern{0};
    std::uint16_t m_num_glyphs{0};
    font_metrics m_metrics{0.0f, 0.0f, 0.0f};
    std::uint16_t m_number_of_h_metrics{0};
};

} /* namespace ttf */

#endif /* WTTF_TYPEFACE_HPP */
