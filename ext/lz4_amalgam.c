#if RBEXT_VISIBILITY
#   define visibility(v) visibility("hidden")
#endif

#include "../contrib/lz4/lib/lz4.c"
#include "../contrib/lz4/lib/lz4hc.c"
#include "../contrib/lz4/lib/lz4frame.c"
#include "../contrib/lz4/lib/xxhash.c"
