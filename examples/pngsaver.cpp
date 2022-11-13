#include "pngsaver.hpp"

#include <png.h>

#include <cstdio>
#include <iostream>
#include <vector>

namespace
{

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

} /* namespace */

void save_png(
    std::filesystem::path const & file, std::uint8_t const * data,
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

    for(auto y = 0ULL; y < height; ++y)
    {
        png_write_row(png_context, data + ((height-y-1) * width));
    }

    png_write_end(png_context, nullptr);

    png_destroy_write_struct(&png_context, &png_info_ptr);
}
