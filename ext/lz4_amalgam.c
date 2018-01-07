#if RBEXT_VISIBILITY
#   define visibility(v) visibility("hidden")
#endif

#include "../contrib/lz4/lib/lz4.c"

#define LZ4_isLittleEndian      amalg_LZ4_isLittleEndian
#define LZ4_read16              amalg_LZ4_read16
#define LZ4_read32              amalg_LZ4_read32
#define LZ4_read_ARCH           amalg_LZ4_read_ARCH
#define LZ4_write16             amalg_LZ4_write16
#define LZ4_write32             amalg_LZ4_write32
#define LZ4_readLE16            amalg_LZ4_readLE16
#define LZ4_writeLE16           amalg_LZ4_writeLE16
#define LZ4_copy8               amalg_LZ4_copy8
#define LZ4_wildCopy            amalg_LZ4_wildCopy
#define LZ4_minLength           amalg_LZ4_minLength
#define LZ4_NbCommonBytes       amalg_LZ4_NbCommonBytes
#define LZ4_count               amalg_LZ4_count
#define limitedOutput           amalg_limitedOutput
#define limitedOutput_directive amalg_limitedOutput_directive
#define unalign                 amalg_unalign
#include "../contrib/lz4/lib/lz4hc.c"

#undef ALLOCATOR
#undef KB
#undef MB
#undef GB
#include "../contrib/lz4/lib/lz4frame.c"

#include "../contrib/lz4/lib/xxhash.c"
