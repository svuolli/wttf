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
my long term goal is to make the API stable, so that new features won't brake
programs using the old features. Until then, the version number will stay at
0.1.x.

The parser is not doing much validity check, not even bounds check. So do not
use with data you don't trust.

-- Sami Vuolli
