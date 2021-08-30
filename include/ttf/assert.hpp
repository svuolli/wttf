#ifndef TTF_ASSERT_HPP
#define TTF_ASSERT_HPP

#ifndef TTF_ASSERT
#ifndef NDEBUG
#include <cassert>
#define TTF_ASSERT(x) assert(x)
#else
#define TTF_ASSERT(x)
#endif
#endif

#endif
