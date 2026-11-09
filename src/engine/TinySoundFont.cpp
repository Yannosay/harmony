#if defined (_MSC_VER)
 #pragma warning (push, 0)
 #pragma warning (disable: 4244 4267 4456 4701 4706)
#elif defined (__clang__) || defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wshadow"
#endif

#define TSF_IMPLEMENTATION
#include "tsf/tsf.h"

#if defined (_MSC_VER)
 #pragma warning (pop)
#else
 #pragma GCC diagnostic pop
#endif
