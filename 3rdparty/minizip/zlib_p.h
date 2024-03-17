#if defined(_WIN32) || defined(__wasm__)
#  include <QtZlib/zlib.h>
#else
#  include <zlib.h>
#endif
