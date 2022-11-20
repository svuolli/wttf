#ifndef WTTF_TYPEFACE_HPP
#define WTTF_TYPEFACE_HPP

#include "export.hpp"
#include "shape.hpp"
#include "metrics.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace wttf
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

    explicit typeface(std::vector<std::byte> && data);

#if WTTF_FONT_COLLECTION_IMPLEMENTED
    typeface(font_collection const & collection, std::size_t index);
#endif

    ~typeface();

    typeface & operator=(typeface const & other);
    typeface & operator=(typeface && other) = delete;

    explicit operator bool() const;

    [[nodiscard]] std::size_t glyph_index(unsigned int codepoint) const;
    [[nodiscard]] shape glyph_shape(std::uint16_t index) const;
    [[nodiscard]] glyph_metrics metrics(std::uint16_t index) const;
    [[nodiscard]] font_metrics const & metrics() const;
    [[nodiscard]] float kerning(
        std::uint16_t glyph1, std::uint16_t glyph2) const;

    private:
    class implementation;

    typeface(
        std::shared_ptr<font_data const> const & data, std::size_t offset);

    std::shared_ptr<implementation> m_impl;
}; /* class typeface */

} /* namespace wttf */

#endif /* WTTF_TYPEFACE_HPP */
