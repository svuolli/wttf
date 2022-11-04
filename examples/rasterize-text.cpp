#include <wttf/transform.hpp>
#include <wttf/shape.hpp>
#include <wttf/typeface.hpp>
#include <wttf/rasterizer.hpp>

#include <fmt/format.h>
#include <fmt/color.h>

#include <png.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef WTTF_ASSERT
#ifndef NDEBUG
#include <cassert>
#define WTTF_ASSERT(x) assert(x)
#else
#define WTTF_ASSERT(x)
#endif
#endif

namespace
{

void usage(char const * program_name)
{
    std::cerr <<
        fmt::format("Usage: {} ", program_name) <<
        fmt::format(fmt::emphasis::underline, "font-file") <<
        " " <<
        fmt::format(fmt::emphasis::underline, "font-size") <<
        " " <<
        fmt::format(fmt::emphasis::underline, "png-file") <<
        " " <<
        fmt::format(fmt::emphasis::underline, "message") <<
        "\n";
}

wttf::typeface load_font(std::filesystem::path const & file)
{
    using file_stream = std::basic_ifstream<std::byte>;
    using file_buff_iterator = std::istreambuf_iterator<std::byte>;

    std::ifstream fs{file, std::ios::binary};
    auto contents = std::vector<std::byte>{};
    std::transform(
        std::istreambuf_iterator<char>{fs}, std::istreambuf_iterator<char>{},
        std::back_insert_iterator{contents},
        [](auto c) { return static_cast<std::byte>(c); });

    return wttf::typeface{std::move(contents)};
}

wttf::shape draw_text(
    wttf::typeface const & typeface,
    std::string const & str,
    float const scale = 1.0f)
{
    if(str.empty())
        return {};

    auto result = wttf::shape{};
    auto t = wttf::transform{};
    t.m[0] = t.m[3] = scale;
    auto prev_glyph = std::uint16_t{0};

    for(auto ch: str)
    {
        auto const g_index = typeface.glyph_index(ch);
        auto const shape = typeface.glyph_shape(g_index);
        auto const metrics = typeface.metrics(g_index);
        auto const kern = typeface.kerning(prev_glyph, g_index);

        t.tx += kern * scale;
        result.add_shape(shape, t);
        t.tx += metrics.advance * scale;

        prev_glyph = g_index;
    }

    return result.flatten(0.35f);
}

float float_from_string(std::string const & str)
{
    return std::atof(str.data());
}

struct file_closer
{
    void operator()(FILE * fp) const
    {
        if(fp)
        {
            std::fclose(fp);
        };
    };
};

using unique_file_ptr = std::unique_ptr<FILE, file_closer>;

unique_file_ptr make_unique_file_ptr(char const * name, char const * mode)
{
    return unique_file_ptr{std::fopen(name, mode)};
}

void save_png(
    std::filesystem::path const & file,
    std::uint8_t * const data,
    std::size_t const width, std::size_t const height)
{
    png_structp png_context =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if(!png_context)
    {
        std::cerr << "png_create_write_struct failed\n";
        return;
    }

    if(setjmp(png_jmpbuf(png_context)))
    {
        std::cerr << "PNG I/O failed\n";
        return;
    }

    png_infop png_info_ptr = png_create_info_struct(png_context);
    
    if(!png_info_ptr)
    {
        std::cerr << "png_create_info_struct failed\n";
        return;
    }

    auto fp = make_unique_file_ptr(file.string().c_str(), "wb");

    if(!fp)
    {
        std::cerr << "fopen failed\n";
    }

    png_init_io(png_context, fp.get());

    png_set_IHDR(
        png_context,
        png_info_ptr,
        width, height,
        8, // Bit-depth
        PNG_COLOR_TYPE_GRAY,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE);
    png_write_info(png_context, png_info_ptr);

    auto row_pointers = std::vector<png_bytep>{height};
    for(auto y = 0ULL; y < height; ++y)
    {
        row_pointers[y] = data + ((height-y-1) * width);
    }

    png_write_image(png_context, row_pointers.data());
    png_write_end(png_context, nullptr);
}

} /* namespace */

int main(int argc, char const * argv[])
{
    if(argc < 5)
    {
        std::cerr << fmt::format("{}: too few arguments\n", argv[0]);
        usage(argv[0]);
        return -1;
    }

    auto const fnt = load_font(argv[1]);
    auto const px_height = float_from_string(argv[2]);

    auto const & metrics = fnt.metrics();
    auto const scale = px_height / (metrics.ascent - metrics.descent);

    auto const shape = draw_text(fnt, argv[4], scale);

    auto const extents = [](auto a, auto b)
    {
        auto const oa = static_cast<int>(std::floor(a));
        auto const ob = static_cast<int>(std::ceil(b));
        return static_cast<std::size_t>(ob-oa);
    };

    auto const w = extents(shape.min_x(), shape.max_x());
    auto const h = extents(shape.min_y(), shape.max_y());

    std::vector<std::uint8_t> img;
    img.resize(w*h);
    wttf::rasterizer r{img.data(), w, h, static_cast<std::ptrdiff_t>(w)};
    r.rasterize(shape, -shape.min_x(), -std::floor(shape.min_y()));

    save_png(argv[3], img.data(), w, h);
}
