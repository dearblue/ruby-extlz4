#include <ruby.h>
#include <errno.h>
#include "externals/lz4/lz4.h"
#include "externals/lz4/lz4hc.h"

#if __GNUC__ || __clang__
#define EXT_LIKELY(x)	__builtin_expect(!!(x), 1)
#define EXT_UNLIKELY(x)	__builtin_expect(!!(x), 0)
#else
#define EXT_LIKELY(x)	(x)
#define EXT_UNLIKELY(x)	(x)
#endif

#define NOT_REACHABLE_HERE                              \
    {                                                   \
        rb_bug("not reached execution in ``%s:%d:%s''", \
               __FILE__, __LINE__, __func__);           \
    }                                                   \

#define LOG()                                  \
    {                                          \
        fprintf(stderr, "%s:%d:%s\n",          \
                __FILE__, __LINE__, __func__); \
    }                                          \

#define LOGF(FORMAT, ...)                                   \
    {                                                       \
        fprintf(stderr, "%s:%d:%s: " FORMAT "\n",           \
                __FILE__, __LINE__, __func__, __VA_ARGS__); \
    }                                                       \

static VALUE eLZ4Error;

/*
 * This function is emulate arguments of LZ4_compressHC2_limitedOutput for
 * LZ4_compress_limitedOutput.
 */
static int
ext_LZ4_compress_limitedOutput(const char *src, char *dest, int insize, int outsize, int level)
{
    return LZ4_compress_limitedOutput(src, dest, insize, outsize);
}

/*
 * This function is emulate arguments of LZ4_compressHC2_limitedOutput_continue for
 * LZ4_compress_limitedOutput_continue.
 */
static int
ext_LZ4_compress_limitedOutput_continue(void *lz4, const char *src, char *dest, int insize, int outsize, int level)
{
    return LZ4_compress_limitedOutput_continue(lz4, src, dest, insize, outsize);
}

static inline void
ext_referr(VALUE obj)
{
    rb_raise(eLZ4Error,
             "invalid reference - #<%s:%p>",
             rb_obj_classname(obj), (void *)obj);
}

static inline VALUE
ext_shouldbe_string(VALUE obj)
{
    Check_Type(obj, RUBY_T_STRING);
    return obj;
}

/*
 * Check the object and security
 *
 * - $SAFE < 4: Pass always
 * - $SAFE >= 4: Pass if all arguments, otherwise prevention
 */
static inline void
ext_check_security(VALUE processor, VALUE src, VALUE dest)
{
    if (rb_safe_level() < 4 ||
        ((NIL_P(processor) || OBJ_TAINTED(processor)) &&
         OBJ_TAINTED(src) && OBJ_TAINTED(dest))) {

        return;
    }

    rb_insecure_operation();
}

static inline void
ext_str_expand(VALUE str, size_t size)
{
    if (rb_str_capacity(str) < size) {
        rb_str_modify_expand(str, size - RSTRING_LEN(str));
        if (rb_str_capacity(str) < size) {
            errno = ENOMEM;
            rb_sys_fail("output buffer is not reallocated");
        }
    } else {
        rb_str_modify(str);
    }
}

static inline void
ext_check_uninitialized(VALUE obj, void *ptr)
{
    if (ptr) {
        rb_raise(rb_eTypeError,
                 "already initialized - #<%s:%p>",
                 rb_obj_classname(obj), (void *)obj);
    }
}


/*
 * lz4 raw encode / decode functions
 */

/*
 * ext_LZ4_compress_limitedOutput, LZ4_compressHC2_limitedOutput
 */
typedef int ext_lz4_encoder_f(const char *, char *, int, int, int);

/*
 * calculate destination size from source data
 */
typedef size_t ext_lz4_calc_destsize_f(VALUE src);

static inline void
ext_lz4_raw_process_scanargs(int argc, VALUE argv[], VALUE *src, VALUE *dest, size_t *maxsize, int *level, ext_lz4_calc_destsize_f *calcsize)
{
    const VALUE *argend = argv + argc;
    VALUE tmp;

    if (level) {
        int w = -1;
        if (argc > 1) {
            tmp = argv[0];
            if (NIL_P(tmp)) {
                argv ++;
            } else if (rb_obj_is_kind_of(tmp, rb_cNumeric)) {
                argv ++;
                w = NUM2INT(tmp);
                if (w < 0) {
                    rb_raise(rb_eArgError, "wrong negative number for level");
                }
            }
        }
        *level = w;
    }

    if (argv < argend) {
        *src = ext_shouldbe_string(argv[0]);
        switch (argend - argv) {
        case 1:
            *maxsize = calcsize(*src);
            *dest = rb_str_buf_new(*maxsize);
            return;
        case 2:
            tmp = argv[1];
            if (RB_TYPE_P(tmp, RUBY_T_STRING)) {
                *maxsize = calcsize(*src);
                *dest = ext_shouldbe_string(tmp);
            } else {
                *maxsize = NUM2SIZET(tmp);
                *dest = rb_str_buf_new(*maxsize);
            }
            return;
        case 3:
            *maxsize = NUM2SIZET(argv[1]);
            *dest = ext_shouldbe_string(argv[2]);
            return;
        }
    }

    rb_error_arity(argc, 1, (level ? 4 : 3));
}

static inline size_t
ext_lz4_compressbound(VALUE src)
{
    return LZ4_compressBound(RSTRING_LEN(src));
}

/*
 * call-seq:
 *  raw_compressbound(src) -> size
 *
 * Calcuration maximum size of encoded data in worst case.
 */
static VALUE
ext_lz4_s_raw_compressbound(VALUE lz4, VALUE src)
{
    return SIZET2NUM(ext_lz4_compressbound(src));
}

static inline void
ext_lz4_raw_encode_scanargs(int argc, VALUE argv[], VALUE *src, VALUE *dest, size_t *maxsize, int *level)
{
    ext_lz4_raw_process_scanargs(argc, argv, src, dest, maxsize, level, ext_lz4_compressbound);
}

/*
 * call-seq:
 *  raw_encode(src) -> compressed string data
 *  raw_encode(src, max_dest_size) -> compressed string data
 *  raw_encode(src, dest) -> dest with compressed string data
 *  raw_encode(src, max_dest_size, dest) -> dest with compressed string data
 *  raw_encode(level, src) -> compressed string data
 *  raw_encode(level, src, max_dest_size) -> compressed string data
 *  raw_encode(level, src, dest) -> dest with compressed string data
 *  raw_encode(level, src, max_dest_size, dest) -> dest with compressed string data
 *
 * Encode to raw LZ4 data.
 *
 * level を指定した場合、より圧縮処理に時間を掛けて圧縮効率を高めることが出来ます。
 *
 * 実装の都合上、圧縮関数は LZ4_compress_limitedOutput / LZ4_compressHC2_limitedOutput が使われます。
 *
 * [RETURN]
 *      圧縮されたデータが文字列として返ります。dest を指定した場合は、圧縮データを格納した dest を返します。
 *
 *      圧縮データには自身の終わりやデータ長が含まれていないため、伸張する際には余計なデータが付随していると正常に伸張できません。
 *
 * [src]
 *      圧縮対象となる文字列オブジェクトを指定します。
 *
 * [max_dest_size]
 *      出力バッファの最大バイト数を指定します。圧縮時にこれよりも多くのバッファ長が必要になった場合は例外が発生します。
 *
 *      省略時は src 長から最悪値が計算されます。dest が最初に確保できれば圧縮処理中に例外が発生することがありません。
 *
 * [dest]
 *      出力先とする文字列オブジェクトを指定します。
 *
 *      max_dest_size が同時に指定されない場合、出力バッファの最大バイト長は src 長から最悪値が求められて調整されます。
 *
 * [level]
 *      圧縮レベルとして 0 から 16 までの整数で指定すると、高効率圧縮処理が行われます。
 *
 *      0 を指定した場合、LZ4 の規定値による高効率圧縮処理が行われます。
 *
 *      nil を与えるか省略した場合、通常の圧縮処理が行われます。
 */
static VALUE
ext_lz4_s_raw_encode(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    int level;
    ext_lz4_raw_encode_scanargs(argc, argv, &src, &dest, &maxsize, &level);
    ext_check_security(Qnil, src, dest);

    ext_lz4_encoder_f *encoder;
    if (level < 0) {
        encoder = ext_LZ4_compress_limitedOutput;
    } else {
        encoder = LZ4_compressHC2_limitedOutput;
    }

    size_t srcsize = RSTRING_LEN(src);
    if (srcsize > LZ4_MAX_INPUT_SIZE) {
        rb_raise(eLZ4Error,
                 "source size is too big for lz4 encode (max %u bytes)",
                 LZ4_MAX_INPUT_SIZE);
    }
    ext_str_expand(dest, maxsize);
    rb_str_set_len(dest, 0);
    OBJ_INFECT(dest, src);

    int size = encoder(RSTRING_PTR(src), RSTRING_PTR(dest), srcsize, maxsize, level);
    if (size <= 0) {
        rb_raise(eLZ4Error,
                 "failed LZ4 compress - maxsize is too small, or out of memory");
    }

    rb_str_set_len(dest, size);

    return dest;
}

static inline const char *
ext_lz4_expandsize(const char *p, const char *end, size_t *size)
{
    while (EXT_LIKELY(p < end)) {
        int s = (uint8_t)*p ++;
        *size += s;
        if (EXT_LIKELY(s != 255)) { return p; }
    }

    rb_raise(eLZ4Error, "encounted invalid end of sequence");
}

static inline const char *
ext_lz4_scanseq(const char *p, const char *end, size_t *size)
{
    while (EXT_LIKELY(p < end)) {
        uint8_t token = (uint8_t)*p ++;
        size_t s = token >> 4;
        if (EXT_LIKELY(s == 15)) {
            p = ext_lz4_expandsize(p, end, &s);
        }
        *size += s;
        p += s;

        s = token & 0x0f;
        if (EXT_UNLIKELY(s == 0 && p == end)) {
            return p;
        }

        if (EXT_UNLIKELY(p + 2 >= end)) {
            break;
        }
        size_t offset = (uint8_t)*p ++;
        offset |= ((size_t)((uint8_t)*p ++)) << 8;

        if (EXT_UNLIKELY(offset == 0)) {
            rb_raise(eLZ4Error, "offset is zero");
        }

        if (EXT_LIKELY(s == 15)) {
            p = ext_lz4_expandsize(p, end, &s);
        }
        s += 4;
        *size += s;
    }

    rb_raise(eLZ4Error, "encounted invalid end of sequence");
}

/*
 * lz4 シーケンスから伸張後のバイト数を得る
 *
 * str が文字列であることを保証するのは呼び出し元の責任
 */
static size_t
ext_lz4_scansize(VALUE str)
{
    const char *p = RSTRING_PTR(str);
    const char *end = p + RSTRING_LEN(str);

    size_t total = 0;
    ext_lz4_scanseq(p, end, &total);

    return total;
}

static inline void
ext_lz4_raw_decode_scanargs(int argc, VALUE argv[], VALUE *src, VALUE *dest, size_t *maxsize)
{
    ext_lz4_raw_process_scanargs(argc, argv, src, dest, maxsize, NULL, ext_lz4_scansize);
}

/*
 * call-seq:
 *  raw_decode(src) -> decoded string data
 *  raw_decode(src, max_dest_size) -> decoded string data
 *  raw_decode(src, dest) -> dest with decoded string data
 *  raw_decode(src, max_dest_size, dest) -> dest with decoded string data
 *
 * Decode raw LZ4 data.
 *
 * 出力先は、max_dest_size が与えられていない場合、必要に応じて自動的に拡張されます。
 * この場合、いったん圧縮された LZ4 データを走査するため、事前に僅かな CPU 時間を必要とします。
 */
static VALUE
ext_lz4_s_raw_decode(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    ext_lz4_raw_decode_scanargs(argc, argv, &src, &dest, &maxsize);
    ext_check_security(Qnil, src, dest);

    ext_str_expand(dest, maxsize);
    rb_str_set_len(dest, 0);
    OBJ_INFECT(dest, src);

    int size = LZ4_decompress_safe(RSTRING_PTR(src), RSTRING_PTR(dest), RSTRING_LEN(src), maxsize);
    if (size < 0) {
        rb_raise(eLZ4Error,
                 "failed LZ4_decompress_safe - max_dest_size is too small, or data is corrupted");
    }

    //rb_str_resize(dest, size);
    rb_str_set_len(dest, size);

    return dest;
}

/*
 * call-seq:
 *  raw_scansize(lz4_rawencoded_data) -> integer
 *
 * Scan raw lz4 data, and get decoded byte size.
 *
 * このメソッドは、raw_decode メソッドに max_dest_size なしで利用する場合の検証目的で利用できるようにしてあります。
 *
 * その他の有用な使い方があるのかは不明です。
 */
static VALUE
ext_lz4_s_raw_scansize(VALUE lz4, VALUE str)
{
    Check_Type(str, RUBY_T_STRING);
    return SIZET2NUM(ext_lz4_scansize(str));
}

/*
 * lz4 raw stream encoder - lz4rse
 */

enum {
    EXT_LZ4_RAWSTREAM_PREFIX_SIZE = 64 * 1024,     /* 64 KiB */
    EXT_LZ4_RAWSTREAM_BUFFER_MINSIZE = 192 * 1024, /* 192 KiB */
};

struct ext_lz4rse_traits_t {
    void *(*create)(char *);
    //int (*free)(void *);
    int (*update)(void *, const char *, char *, int, int, int);
    char *(*slide)(void *);
    int (*reset)(void *, const char *);
};

static void *
ext_lz4rse_create_common(char *inbuf, int size, int (*lz4reset)(void *, const char *))
{
    void *lz4 = xmalloc(size);
    int status = lz4reset(lz4, inbuf);
    if (status != 0) {
        rb_raise(eLZ4Error,
                 "failed initialization for lz4 context");
    }
    return lz4;
}

static void *
ext_lz4rse_create(char *inbuf)
{
    return ext_lz4rse_create_common(inbuf,
                                    LZ4_sizeofStreamState(),
                                    LZ4_resetStreamState);
}

static void *
ext_lz4rse_create_hc(char *inbuf)
{
    return ext_lz4rse_create_common(inbuf,
                                    LZ4_sizeofStreamStateHC(),
                                    LZ4_resetStreamStateHC);
}

static struct ext_lz4rse_traits_t ext_lz4rse_encode = {
    .create = ext_lz4rse_create,
    //.free = LZ4_free,
    .update = ext_LZ4_compress_limitedOutput_continue,
    .slide = LZ4_slideInputBuffer,
    .reset = LZ4_resetStreamState,
};

static struct ext_lz4rse_traits_t ext_lz4rse_encode_hc = {
    .create = ext_lz4rse_create_hc,
    //.free = LZ4_freeHC,
    .update = LZ4_compressHC2_limitedOutput_continue,
    .slide = LZ4_slideInputBufferHC,
    .reset = LZ4_resetStreamStateHC,
};

struct ext_lz4rse
{
    VALUE predict;      /* preset-dictionary (used when next reset) */
    VALUE buffer;       /* entity of input buffer */
    char *inoff;        /* current offset of buffer */
    const char *intail; /* end offset of buffer */
    void *lz4;          /* lz4 stream context */
    size_t blocksize;   /* stream block size (maximum size) */
    struct ext_lz4rse_traits_t *traits;
    VALUE ishc;         /* false: not hc / true: hc */
};

static void
ext_lz4rse_mark(struct ext_lz4rse *lz4p)
{
    if (lz4p) {
        rb_gc_mark(lz4p->predict);
        rb_gc_mark(lz4p->buffer);
    }
}

static void
ext_lz4rse_free(struct ext_lz4rse *lz4p)
{
    if (lz4p) {
        if (lz4p->lz4) {
            free(lz4p->lz4);
        }
        free(lz4p);
    }
}

static inline struct ext_lz4rse *
ext_lz4rse_refp(VALUE lz4)
{
    struct ext_lz4rse *lz4p;
    Data_Get_Struct(lz4, struct ext_lz4rse, lz4p);
    return lz4p;
}

static inline struct ext_lz4rse *
ext_lz4rse_ref(VALUE lz4)
{
    struct ext_lz4rse *lz4p = ext_lz4rse_refp(lz4);
    if (!lz4p) { ext_referr(lz4); }
    return lz4p;
}

static VALUE
ext_lz4rse_alloc(VALUE lz4)
{
    return Data_Wrap_Struct(lz4, ext_lz4rse_mark, ext_lz4rse_free, NULL);
}

static void
ext_lz4rse_set_predict(struct ext_lz4rse *lz4p)
{
    VALUE predict = lz4p->predict;
    if (NIL_P(predict)) { return; }
    size_t srcsize = RSTRING_LEN(predict);
    size_t maxsize = LZ4_compressBound(EXT_LZ4_RAWSTREAM_PREFIX_SIZE);
    VALUE temp = rb_str_buf_new(maxsize);
    char *predictp = RSTRING_PTR(predict);
    memcpy(lz4p->inoff, predictp, srcsize);
    int size = lz4p->traits->update(lz4p->lz4, lz4p->inoff, RSTRING_PTR(temp), srcsize, maxsize, 1);
    if (size <= 0) {
        rb_raise(eLZ4Error,
                 "failed set preset dictionary");
    }
    lz4p->inoff += srcsize;
}

static void
ext_lz4rse_create_encoder(struct ext_lz4rse *lz4p)
{
    if (lz4p->ishc) {
        lz4p->traits = &ext_lz4rse_encode_hc;
    } else {
        lz4p->traits = &ext_lz4rse_encode;
    }
    VALUE buf = lz4p->buffer;
    lz4p->inoff = RSTRING_PTR(buf);
    lz4p->intail = lz4p->inoff + rb_str_capacity(buf);
    lz4p->blocksize = rb_str_capacity(buf) - EXT_LZ4_RAWSTREAM_PREFIX_SIZE;
    lz4p->lz4 = lz4p->traits->create(lz4p->inoff);
    ext_lz4rse_set_predict(lz4p);
}

static size_t
ext_lz4rse_correct_blocksize(size_t blocksize)
{
    if ((ssize_t)blocksize < 1) {
        rb_raise(rb_eArgError, "blocksize is too small (or too big)");
    }
    blocksize += EXT_LZ4_RAWSTREAM_PREFIX_SIZE;
    if (blocksize > LZ4_MAX_INPUT_SIZE) {
        rb_raise(rb_eArgError,
                 "blocksize is too big (max %d bytes)",
                 LZ4_MAX_INPUT_SIZE);
    }
    if (blocksize < EXT_LZ4_RAWSTREAM_BUFFER_MINSIZE) {
        blocksize = EXT_LZ4_RAWSTREAM_BUFFER_MINSIZE;
    }

    return blocksize;
}

static VALUE
ext_make_predict(VALUE predict)
{
    if (NIL_P(predict)) {
        return Qnil;
    }

    Check_Type(predict, RUBY_T_STRING);
    ssize_t size = RSTRING_LEN(predict);
    ssize_t off = size - EXT_LZ4_RAWSTREAM_PREFIX_SIZE;
    if (off < 0) { off = 0; }
    return rb_str_subseq(predict, off, size);
}

static inline void
ext_lz4rse_init_scanargs(int argc, VALUE argv[], size_t *blocksize, VALUE *ishc, VALUE *predict)
{
    switch (argc) {
    case 1:
        *blocksize = NUM2UINT(argv[0]);
        *ishc = Qfalse;
        *predict = Qnil;
        return;
    case 2:
        *blocksize = NUM2UINT(argv[0]);
        *ishc = argv[1];
        *predict = Qnil;
        return;
    case 3:
        *blocksize = NUM2UINT(argv[0]);
        *ishc = argv[1];
        *predict = argv[2];
        return;
    }

    rb_error_arity(argc, 1, 3);
}

/*
 * call-seq:
 *  initialize(blocksize)
 *  initialize(blocksize, is_high_compress)
 *  initialize(blocksize, is_high_compress, preset_dictionary)
 */
static VALUE
ext_lz4rse_init(int argc, VALUE argv[], VALUE lz4)
{
    ext_check_uninitialized(lz4, ext_lz4rse_refp(lz4));

    size_t blocksize;
    VALUE ishc, predict;
    ext_lz4rse_init_scanargs(argc, argv, &blocksize, &ishc, &predict);

    VALUE buffer = rb_str_buf_new(ext_lz4rse_correct_blocksize(blocksize));
    struct ext_lz4rse *lz4p = ALLOC(struct ext_lz4rse);
    DATA_PTR(lz4) = lz4p;

    lz4p->predict = ext_make_predict(predict);
    OBJ_INFECT(lz4, lz4p->predict);
    lz4p->buffer = buffer;
    lz4p->ishc = RTEST(ishc);
    ext_lz4rse_create_encoder(lz4p);

    return lz4;
}

/*
 * call-seq:
 *  update(src) -> encoded data
 *  update(src, max_dest_size) -> encoded data
 *  update(src, dest) -> dest with encoded data
 *  update(src, max_dest_size, dest) -> dest with encoded data
 *  update(level, src) -> encoded data
 *  update(level, src, max_dest_size) -> encoded data
 *  update(level, src, dest) -> dest with encoded data
 *  update(level, src, max_dest_size, dest) -> dest with encoded data
 *
 * 実際の圧縮処理を行います。詳しい引数と戻り値については、LZ4.raw_encode と同じです。そちらをご参照ください。
 *
 * [level]
 *      高効率圧縮器での圧縮レベルを指定します。省略時は 0 と等価です。
 *      通常圧縮器では無視されます (意味がありません)。
 */
static VALUE
ext_lz4rse_update(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    int level;
    ext_lz4_raw_encode_scanargs(argc, argv, &src, &dest, &maxsize, &level);
    ext_check_security(lz4, src, dest);
    if (level < 0) { level = 0; }

    struct ext_lz4rse *lz4p = ext_lz4rse_ref(lz4);
    size_t srcsize = RSTRING_LEN(src);

    ext_str_expand(dest, maxsize);
    rb_str_set_len(dest, 0);
    OBJ_INFECT(lz4, src);
    OBJ_INFECT(dest, lz4);

    VALUE buf = ext_shouldbe_string(lz4p->buffer);
    if (rb_str_capacity(buf) < srcsize + EXT_LZ4_RAWSTREAM_PREFIX_SIZE) {
        rb_raise(eLZ4Error,
                 "src is more than block size");
    }

    if (lz4p->intail - lz4p->inoff < (ssize_t)srcsize) {
        lz4p->inoff = lz4p->traits->slide(lz4p->lz4);
    }
    memcpy(lz4p->inoff, RSTRING_PTR(src), srcsize);
    int size = lz4p->traits->update(lz4p->lz4, lz4p->inoff, RSTRING_PTR(dest), srcsize, maxsize, level);
    if (size <= 0) {
        rb_raise(eLZ4Error,
                 "failed LZ4 encoding - max_dest_size is too small, or out of memory");
    }
    lz4p->inoff += srcsize;
    rb_str_set_len(dest, size);

    return dest;
}

static void
ext_lz4rse_reset_state(struct ext_lz4rse *lz4p)
{
    VALUE buf = lz4p->buffer;
    lz4p->inoff = RSTRING_PTR(buf);
    lz4p->intail = lz4p->inoff + rb_str_capacity(buf);
    lz4p->blocksize = rb_str_capacity(buf) - EXT_LZ4_RAWSTREAM_PREFIX_SIZE;
    memset(lz4p->inoff, 0, EXT_LZ4_RAWSTREAM_BUFFER_MINSIZE);
    int status = lz4p->traits->reset(lz4p->lz4, lz4p->inoff);
    if (status != 0) {
        rb_raise(eLZ4Error,
                 "failed reset raw stream encoder");
    }
    ext_lz4rse_set_predict(lz4p);
}

static void
ext_lz4rse_reset_scanargs(int argc, VALUE argv[], size_t *blocksize, VALUE *ishc, VALUE *predict)
{
    switch (argc) {
    case 0:
        *blocksize = 0;
        return;
    case 1:
        *blocksize = ext_lz4rse_correct_blocksize(NUM2SIZET(argv[0]));
        *ishc = *predict = Qundef;
        return;
    case 2:
        *blocksize = ext_lz4rse_correct_blocksize(NUM2SIZET(argv[0]));
        *ishc = RTEST(argv[1]);
        *predict = Qundef;
        return;
    case 3:
        *blocksize = ext_lz4rse_correct_blocksize(NUM2SIZET(argv[0]));
        *ishc = RTEST(argv[1]);
        *predict = ext_make_predict(argv[2]);
        return;
    }

    rb_error_arity(argc, 0, 3);
}

/*
 * call-seq:
 *  reset -> self
 *  reset(blocksize) -> self
 *  reset(blocksize, is_high_compress) -> self
 *  reset(blocksize, is_high_compress, preset_dictionary) -> self
 *
 * Reset raw stream encoder.
 */
static VALUE
ext_lz4rse_reset(int argc, VALUE argv[], VALUE lz4)
{
    struct ext_lz4rse *lz4p = ext_lz4rse_ref(lz4);
    size_t blocksize;
    VALUE ishc, predict;

    ext_lz4rse_reset_scanargs(argc, argv, &blocksize, &ishc, &predict);

    if (blocksize == 0) {
        ext_lz4rse_reset_state(lz4p);
    } else if (ishc == Qundef) {
        ext_str_expand(lz4p->buffer, blocksize);
        ext_lz4rse_reset_state(lz4p);
        return lz4;
    } else {
        if (predict != Qundef) {
            lz4p->predict = ext_make_predict(predict);
            OBJ_INFECT(lz4, lz4p->predict);
        }

        ext_str_expand(lz4p->buffer, blocksize);

        if (RTEST(ishc) == lz4p->ishc) {
            ext_lz4rse_reset_state(lz4p);
        } else {
            lz4p->ishc = RTEST(ishc);
            free(lz4p->lz4);
            lz4p->lz4 = NULL;
            ext_lz4rse_create_encoder(lz4p);
        }
    }

    return lz4;
}

/*
 * call-seq:
 *  release -> nil
 * 
 * Release allocated input buffer and internal lz4 context.
 */
static VALUE
ext_lz4rse_release(VALUE lz4)
{
    struct ext_lz4rse *lz4p = ext_lz4rse_refp(lz4);
    if (!lz4p) { return Qnil; }
    if (lz4p->lz4) { free(lz4p->lz4); }
    if (!NIL_P(lz4p->buffer)) {
        rb_str_resize(lz4p->buffer, 0);
        lz4p->intail = lz4p->inoff = NULL;
    }
    free(lz4p);
    DATA_PTR(lz4) = NULL;

    return Qnil;
}

/*
 * lz4 raw stream decoder - lz4rsd
 */

struct ext_lz4rsd
{
    VALUE predict;
    VALUE prefix;
};

static void
ext_lz4rsd_mark(struct ext_lz4rsd *lz4p)
{
    if (lz4p) {
        rb_gc_mark(lz4p->predict);
        rb_gc_mark(lz4p->prefix);
    }
}

static void
ext_lz4rsd_free(struct ext_lz4rsd *lz4p)
{
    if (lz4p) {
        free(lz4p);
    }
}

static struct ext_lz4rsd *
ext_lz4rsd_refp(VALUE lz4)
{
    struct ext_lz4rsd *lz4p;
    Data_Get_Struct(lz4, struct ext_lz4rsd, lz4p);
    return lz4p;
}

static struct ext_lz4rsd *
ext_lz4rsd_ref(VALUE lz4)
{
    struct ext_lz4rsd *lz4p = ext_lz4rsd_refp(lz4);
    if (!lz4p) { ext_referr(lz4); }
    return lz4p;
}

static VALUE
ext_lz4rsd_alloc(VALUE lz4)
{
    return Data_Wrap_Struct(lz4, ext_lz4rsd_mark, ext_lz4rsd_free, NULL);
}

static inline void
ext_lz4rsd_init_scanargs(int argc, VALUE argv[], VALUE *predict)
{
    switch (argc) {
    case 0:
        *predict = Qnil;
        return;
    case 1:
        *predict = ext_make_predict(argv[0]);
        return;
    }

    rb_error_arity(argc, 0, 1);
}

/*
 * call-seq:
 *  initialize
 *  initialize(preset_dictionary)
 */
static VALUE
ext_lz4rsd_init(int argc, VALUE argv[], VALUE lz4)
{
    ext_check_uninitialized(lz4, ext_lz4rsd_refp(lz4));

    VALUE predict;
    ext_lz4rsd_init_scanargs(argc, argv, &predict);

    VALUE prefix;
    if (NIL_P(predict)) {
        prefix = rb_str_buf_new(EXT_LZ4_RAWSTREAM_PREFIX_SIZE);
    } else {
        prefix = rb_str_dup(predict);
        OBJ_INFECT(lz4, prefix);
    }

    struct ext_lz4rsd *lz4p = ALLOC(struct ext_lz4rsd);
    DATA_PTR(lz4) = lz4p;
    lz4p->predict = predict;
    lz4p->prefix = prefix;

    return lz4;
}

/*
 * call-seq:
 *  update(src) -> decoded string data
 *  update(src, max_dest_size) -> decoded string data
 *  update(src, dest) -> dest for decoded string data
 *  update(src, max_dest_size, dest) -> dest for decoded string data
 *
 * Decode raw lz4 data of stream block.
 *
 * Given arguments and return values are same as LZ4#raw_decode.
 * See LZ4#raw_decode for about its.
 *
 * 出力先は、max_dest_size が与えられていない場合、必要に応じて自動的に拡張されます。
 * この場合、いったん圧縮された LZ4 データを走査するため、事前に僅かな CPU 時間を必要とします。
 *
 * dest は実装の都合上、メモリが常に 64 KiB 多く確保されます。
 * これは伸張時に必要とする prefix の分です。
 * メソッドから戻るときは prefix の部分が取り除かれて正しいバイト列長に修正されますが、確保されたメモリを縮小することは行っていません。
 */
static VALUE
ext_lz4rsd_update(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    ext_lz4_raw_decode_scanargs(argc, argv, &src, &dest, &maxsize);
    ext_check_security(lz4, src, dest);
    struct ext_lz4rsd *lz4p = ext_lz4rsd_ref(lz4);
    size_t allocsize = maxsize + EXT_LZ4_RAWSTREAM_PREFIX_SIZE;

    ext_str_expand(dest, allocsize);
    rb_str_set_len(dest, 0);
    OBJ_INFECT(lz4, src);
    OBJ_INFECT(dest, lz4);

    char *destp = RSTRING_PTR(dest);
    char *prefixp = RSTRING_PTR(lz4p->prefix);
    memcpy(destp, prefixp, EXT_LZ4_RAWSTREAM_PREFIX_SIZE);
    char *destpx = destp + EXT_LZ4_RAWSTREAM_PREFIX_SIZE;
    int size = LZ4_decompress_safe_withPrefix64k(RSTRING_PTR(src), destpx, RSTRING_LEN(src), maxsize);
    if (size < 0) {
        rb_raise(eLZ4Error,
                 "failed decode - maxsize is too small, or corrupt compressed data");
    }
    memcpy(prefixp, destp + size, EXT_LZ4_RAWSTREAM_PREFIX_SIZE);
    memmove(destp, destpx, size);
    rb_str_set_len(dest, size);

    return dest;
}

static void
ext_lz4rsd_reset_scanargs(int argc, VALUE argv[], VALUE *predict)
{
    switch (argc) {
    case 0:
        *predict = Qundef;
        return;
    case 1:
        *predict = ext_make_predict(argv[0]);
        return;
    }

    rb_error_arity(argc, 0, 1);
}

/*
 * call-seq:
 *  reset -> self
 *  reset(preset_dictionary) -> self
 */
static VALUE
ext_lz4rsd_reset(int argc, VALUE argv[], VALUE lz4)
{
    struct ext_lz4rsd *lz4p = ext_lz4rsd_ref(lz4);
    VALUE predict;
    ext_lz4rsd_reset_scanargs(argc, argv, &predict);

    if (NIL_P(predict) || (predict == Qundef && NIL_P(lz4p->predict))) {
        lz4p->predict = Qnil;
        memset(RSTRING_PTR(lz4p->prefix), 0, EXT_LZ4_RAWSTREAM_PREFIX_SIZE);
    } else if (predict == Qundef) {
        memcpy(RSTRING_PTR(lz4p->prefix), RSTRING_PTR(lz4p->predict), EXT_LZ4_RAWSTREAM_PREFIX_SIZE);
    } else {
        lz4p->predict = predict;
        memcpy(RSTRING_PTR(lz4p->prefix), RSTRING_PTR(predict), EXT_LZ4_RAWSTREAM_PREFIX_SIZE);
        OBJ_INFECT(lz4, predict);
    }

    return lz4;
}

/*
 * call-seq:
 *  release -> nil
 *
 * Release allocated internal heap memory.
 */
static VALUE
ext_lz4rsd_release(VALUE lz4)
{
    struct ext_lz4rsd *lz4p = ext_lz4rsd_refp(lz4);
    if (!lz4p) { return Qnil; }
    DATA_PTR(lz4) = NULL;
    // TODO: lz4p->predict と lz4p->prefix も rb_str_resize で 0 にするべきか?
    free(lz4p);
    return Qnil;
}

/*
 * version information
 */

static VALUE
ext_lz4_ver_major(VALUE ver)
{
    return INT2FIX(LZ4_VERSION_MAJOR);
}

static VALUE
ext_lz4_ver_minor(VALUE ver)
{
    return INT2FIX(LZ4_VERSION_MINOR);
}

static VALUE
ext_lz4_ver_release(VALUE ver)
{
    return INT2FIX(LZ4_VERSION_RELEASE);
}

static VALUE
ext_lz4_ver_to_s(VALUE ver)
{
    return rb_sprintf("%d.%d.%d", LZ4_VERSION_MAJOR, LZ4_VERSION_MINOR, LZ4_VERSION_RELEASE);
}


/*
 * initialize library
 */

void
Init_extlz4(void)
{
    VALUE mLZ4 = rb_define_module("LZ4");

    /*
     * Document-const: LZ4::VERSION
     *
     * This constant value means api version number of original lz4 library as array'd integers.
     *
     * And it's has any singleton methods, so they are `#major`, `#minor`, `#release` and `#to_s`.
     *
     * 実体が配列である理由は、比較を行いやすくすることを意図しているためです。
     */
    VALUE ver = rb_ary_new3(3,
                            INT2FIX(LZ4_VERSION_MAJOR),
                            INT2FIX(LZ4_VERSION_MINOR),
                            INT2FIX(LZ4_VERSION_RELEASE));
    rb_define_singleton_method(ver, "major", ext_lz4_ver_major, 0);
    rb_define_singleton_method(ver, "minor", ext_lz4_ver_minor, 0);
    rb_define_singleton_method(ver, "release", ext_lz4_ver_release, 0);
    rb_define_singleton_method(ver, "to_s", ext_lz4_ver_to_s, 0);
    OBJ_FREEZE(ver);
    rb_define_const(mLZ4, "VERSION", ver);

    rb_define_singleton_method(mLZ4, "raw_compressbound", ext_lz4_s_raw_compressbound, 1);
    rb_define_singleton_method(mLZ4, "raw_scansize", ext_lz4_s_raw_scansize, 1);

    rb_define_singleton_method(mLZ4, "raw_encode", ext_lz4_s_raw_encode, -1);
    rb_define_singleton_method(mLZ4, "raw_decode", ext_lz4_s_raw_decode, -1);
    rb_define_alias(rb_singleton_class(mLZ4), "raw_compress", "raw_encode");
    rb_define_alias(rb_singleton_class(mLZ4), "raw_decompress", "raw_decode");
    rb_define_alias(rb_singleton_class(mLZ4), "raw_uncompress", "raw_decode");

    VALUE cLZ4RSE = rb_define_class_under(mLZ4, "RawStreamEncoder", rb_cObject);
    rb_define_alloc_func(cLZ4RSE, ext_lz4rse_alloc);
    rb_define_method(cLZ4RSE, "initialize", RUBY_METHOD_FUNC(ext_lz4rse_init), -1);
    rb_define_method(cLZ4RSE, "update", RUBY_METHOD_FUNC(ext_lz4rse_update), -1);
    rb_define_method(cLZ4RSE, "reset", RUBY_METHOD_FUNC(ext_lz4rse_reset), -1);
    rb_define_method(cLZ4RSE, "release", RUBY_METHOD_FUNC(ext_lz4rse_release), 0);
    rb_define_alias(cLZ4RSE, "encode", "update");
    rb_define_alias(cLZ4RSE, "compress", "update");

    VALUE cLZ4RSD = rb_define_class_under(mLZ4, "RawStreamDecoder", rb_cObject);
    rb_define_alloc_func(cLZ4RSD, ext_lz4rsd_alloc);
    rb_define_method(cLZ4RSD, "initialize", RUBY_METHOD_FUNC(ext_lz4rsd_init), -1);
    rb_define_method(cLZ4RSD, "update", RUBY_METHOD_FUNC(ext_lz4rsd_update), -1);
    rb_define_method(cLZ4RSD, "reset", RUBY_METHOD_FUNC(ext_lz4rsd_reset), -1);
    rb_define_method(cLZ4RSD, "release", RUBY_METHOD_FUNC(ext_lz4rsd_release), 0);
    rb_define_alias(cLZ4RSD, "decode", "update");
    rb_define_alias(cLZ4RSD, "decompress", "update");
    rb_define_alias(cLZ4RSD, "uncompress", "update");

    eLZ4Error = rb_define_class_under(mLZ4, "Error", rb_eStandardError);
}
