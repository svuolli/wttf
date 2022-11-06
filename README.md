# Font File Parser and Rasterizer Library for C++

wttf started as my holiday project, when I was wondering what kind of data
TrueType Font files are storing. The parser is far from complete, but it
extracts the metrics, outline and kerning info from the few font files I used
while developing.

There's also a pretty decent rasterizer in the library. It creates an 8-bit
grayscale image of the outline data. The output is anti-aliased and looks nice
to my eyes. The performance seems to be also quite good.

There's lots of missing features, some of which I am interested in adding
when I find time and motivation, and some of which I am indifferent about.

The API (and ABI) is not stable. I am currently not going to pay any attention
that existing programs using wttf keeps working when I change something. But
my long term goal is to make the API stable, so that new features won't break
programs using the old features. Until then, the version number will stay at
0.1.x.

The parser is not doing much validity check, not even bounds check. So do not
use with data you don't trust.

-- Sami Vuolli

## Building

Building and using wttf requires a C++ compiler, compliant with the C++17
standard. For building, you need also CMake version 3.20.0 or newer, and a
build tool that CMake supports.

The wttf library itself does not depend on any other libraries than the C++
standard library. The shipped examples depends on libfmt, libpng and libicu.

If you have CMake 3.24.0 or newer, you can build wttf using the shipped CMake
presets (CMakePresets.json):

```
cmake --preset=ReleaseWithExamples
cmake --build --preset=ReleaseWithExamples
sudo cmake --install build/ReleaseWithExamples/
```

If you want to build only the library, replace "RelaseWithExamples" with
"Release".

## Usage

### Basics

An example to rasterize one glyph:

```cpp
#include <wttf/typeface.hpp>
#include <wttf/rasterizer.hpp>

// ...

// wttf does not do file access, a std::vector<std::byte> with a contents
// of a font file is required by the typeface constructor.
std::vector<std::byte> font_file_contents = read_file_contents("fontfile.ttf");

// Construct a typeface object. This is the central type of wttf. With
// typeface object you can extract shape and layouting info of the glyphs of
// the font.
wttf::typeface typeface{std::move(font_file_contents)};

// You can't use unicode codepoints directly to access glyph data. Instead you
// need the index of a glyph. This index is font specific. Following line
// converts a unicode codepoint for letter A to glyph index.
auto const glyph_index = typeface.glyph_index('A');

// With glyph_index, you can access glyph_metrics...
wttf::glyph_metrics const glyph_metrics = typeface.metrics(glyph_index);

// ... and glyph shape
wttf::shape glyph_shape = typeface.glyph_shape(glyph_index);

// Shape returned by this function is unscaled. That means that the size of
// of the glyph is in what ever scale font designer used. To scale it to some
// pixel size you need to scale it.
constexpr static float pixel_size = 24.0f;
wttf::font_metrics const font_metrics = typeface.metrics();
glyph_shape.scale(pixel_size / font_metrics.height());

// To rasterize, you need to provide a contiguous buffer to store the
// rasterized image.
std::size_t const image_width =
    static_cast<std::size_t>(std::floor(shape.width()));
std::size_t const image_height =
    static_cast<std::size_t>(std::floor(shape.height()));
std::vector<std::uint8_t> image_data;
image_data.resize(image_width * image_height);

// Finally we're ready to raterize the glyph shape
wttf::rasterizer rasterizer{
    image_data.data(), // Pointer to lower-left most pixel
    image_width, image_height, // Image dimensions
    static_cast<std::ptrdiff_t>(image_height)}; // Stride
// Stride is distance of a pixel in memory to it's neighbor above. In bytes.

rasterizer.rasterize(glyph_shape, -glyph_shape.min_x(), -glyph_shape.min_y());

// Now image_data contains rasterized 8-bit grayscale image of the glyph.
// Pixel value 0x00 means that pixel is completely out side of the shape.
// Value 0xFF means that pixel is totally covered by the shape.
// Values between are partially covered (anti-aliasing).
```

### Text layouting

wttf provides enough support for user to implement basic horizontal text
layouting. At least for left-to-right text. Vertical layouts are not currently
supported at all. Also any advanced text shaping features, required for
scripts like Arabic, are not supported.

Example for layouting text and constructing a shape for it:

```cpp
wttf::shape text_layout(
    wttf::typeface const & typeface, float scale, std::string const & text)
{
    wttf::shape result{};
    wttf::transform t{
       wttf::matrix_2x2{scale, 0.0f, 0.0f, scale},
       0.0f, 0.0f};
    std::uint16_t prev_glyph = 0;

    for(auto const ch: text)
    {
        auto const glyph_index = typeface.glyph_index(ch);
        auto const shape = typeface.glyph_shape(glyph_index);
        auto const metrics = typeface.metrics(glyph_index);
        auto const kerning = typeface.kerning(prev_glyph, glyph_index);

        t.tx += kerning * scale;
        result.add_shape(shape, t); // Adds shape (transformed by t) to an existing shape.
        t.tx += metrics.advance * scale; // Move "cursor" to next position

        prev_glyph = glyph_index;
    }

    return result;
}
```

