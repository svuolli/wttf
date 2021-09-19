#ifndef WTTF_ASSERT_HPP
#define WTTF_ASSERT_HPP

#ifndef WTTF_ASSERT
#ifndef NDEBUG
#include <cassert>
#define WTTF_ASSERT(x) assert(x)
#else
#define WTTF_ASSERT(x)
#endif
#endif

#endif /* WTTF_ASSERT_HPP */
