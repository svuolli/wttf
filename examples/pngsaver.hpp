#ifndef WTTF_EXAMPLES_PNGSAVER_HPP
#define WTTF_EXAMPLES_PNGSAVER_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>

void save_png(
    std::filesystem::path const & file, std::uint8_t const * data,
    std::size_t const width, std::size_t const height);

#endif /* WTTF_EXAMPLES_PNGSAVER_HPP */
