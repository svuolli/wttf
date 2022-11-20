#ifndef WTTF_TYPEFACE_P_HPP
#define WTTF_TYPEFACE_P_HPP

#include <wttf/typeface.hpp>
#include "font_data.hpp"

namespace wttf
{

class typeface::implementation
{
    public:
    implementation() = delete;
    implementation(const implementation &) = delete;
    implementation(implementation &&) = delete;

    implementation(
        std::shared_ptr<font_data const> const & data, std::size_t offset);

    [[nodiscard]] std::size_t glyph_index(unsigned int codepoint) const;
    [[nodiscard]] shape glyph_shape(std::uint16_t index) const;
    [[nodiscard]] glyph_metrics metrics(std::uint16_t index) const;
    [[nodiscard]] font_metrics const & metrics() const;
    [[nodiscard]] float kerning(std::uint16_t glyph1, std::uint16_t glyph2) const;

    private:
    using glyph_index_fn_t =
        std::uint16_t (implementation::*)(unsigned int) const;
    using glyph_offset_fn_t =
        std::uint32_t (implementation::*)(std::uint16_t) const;
    using kerning_table = std::map<std::uint16_t, float>;

    template <typename T> [[nodiscard]] T get(std::size_t offset) const
    {
        return m_data->get<T>(offset);
    }

    [[nodiscard]] std::uint32_t find_table(char const * tag) const;

    [[nodiscard]] std::uint16_t
    format0_glyph_index(unsigned int codepoint) const;
    [[nodiscard]] std::uint16_t
    format4_glyph_index(unsigned int codepoint) const;
    [[nodiscard]] std::uint16_t
    format6_glyph_index(unsigned int codepoint) const;

    [[nodiscard]] std::uint32_t 
    format0_glyph_offset(std::uint16_t glyph_index) const;
    [[nodiscard]] std::uint32_t
    format1_glyph_offset(std::uint16_t glyph_index) const;

    [[nodiscard]] std::uint32_t glyph_offset(std::uint16_t glyph_index) const;

    [[nodiscard]] shape simple_glyph_shape(
        std::uint32_t const glyph_offset) const;
    [[nodiscard]] shape composite_glyph_shape(
        std::uint32_t const glyph_offset) const;

    std::shared_ptr<font_data const> m_data{nullptr};
    std::map<std::uint16_t, kerning_table> m_kerning_tables{};
    glyph_index_fn_t m_glyph_index_fn{nullptr};
    glyph_offset_fn_t m_glyph_offset_fn{nullptr};
    std::size_t m_data_offset{0u};
    std::uint32_t m_cmap_index{0};
    std::uint32_t m_loca{0};
    std::uint32_t m_glyf{0};
    std::uint32_t m_hmtx{0};
    std::uint32_t m_kern{0};
    std::uint16_t m_num_glyphs{0};
    font_metrics m_metrics{0.0f, 0.0f, 0.0f};
    std::uint16_t m_number_of_h_metrics{0};
}; /* class typeface::implementation */

} /* namespace wttf */

#endif /* WTTF_TYPEFACE_P_HPP */
