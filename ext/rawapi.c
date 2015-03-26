#include "extlz4.h"
#include <lz4.h>
#include <lz4hc.h>

#if __GNUC__ || __clang__
#define AUX_LIKELY(x)	__builtin_expect(!!(x), 1)
#define AUX_UNLIKELY(x)	__builtin_expect(!!(x), 0)
#else
#define AUX_LIKELY(x)	(x)
#define AUX_UNLIKELY(x)	(x)
#endif

static inline const char *
aux_lz4_expandsize(const char *p, const char *end, size_t *size)
{
    while (AUX_LIKELY(p < end)) {
        int s = (uint8_t)*p ++;
        *size += s;
        if (AUX_LIKELY(s != 255)) { return p; }
    }

    rb_raise(eError, "encounted invalid end of sequence");
}

static inline const char *
aux_lz4_scanseq(const char *p, const char *end, size_t *size)
{
    while (AUX_LIKELY(p < end)) {
        uint8_t token = (uint8_t)*p ++;
        size_t s = token >> 4;
        if (AUX_LIKELY(s == 15)) {
            p = aux_lz4_expandsize(p, end, &s);
        }
        *size += s;
        p += s;

        s = token & 0x0f;
        if (AUX_UNLIKELY(s == 0 && p == end)) {
            return p;
        }

        if (AUX_UNLIKELY(p + 2 >= end)) {
            break;
        }
        size_t offset = (uint8_t)*p ++;
        offset |= ((size_t)((uint8_t)*p ++)) << 8;
#if 0
        if (AUX_UNLIKELY(offset == 0)) {
            rb_raise(eError, "offset is zero");
        }
#endif
        if (AUX_LIKELY(s == 15)) {
            p = aux_lz4_expandsize(p, end, &s);
        }
        s += 4;
        *size += s;
    }

    rb_raise(eError, "encounted invalid end of sequence");
}

/*
 * lz4 シーケンスから伸張後のバイト数を得る
 *
 * str が文字列であることを保証するのは呼び出し元の責任
 */
static size_t
aux_lz4_scansize(VALUE str)
{
    const char *p = RSTRING_PTR(str);
    const char *end = p + RSTRING_LEN(str);

    size_t total = 0;
    aux_lz4_scanseq(p, end, &total);

    return total;
}

static inline VALUE
aux_shouldbe_string(VALUE obj)
{
    rb_check_type(obj, RUBY_T_STRING);
    return obj;
}

/*
 * Check the object and security
 *
 * - $SAFE < 3: Pass always
 * - $SAFE >= 3: Pass if all arguments, otherwise prevention
 */
static inline void
check_security(VALUE processor, VALUE src, VALUE dest)
{
    if (rb_safe_level() < 3 ||
        ((NIL_P(processor) || OBJ_TAINTED(processor)) &&
         OBJ_TAINTED(src) && OBJ_TAINTED(dest))) {

        return;
    }

    rb_insecure_operation();
}

static inline size_t
aux_lz4_compressbound(VALUE src)
{
    return LZ4_compressBound(RSTRING_LEN(src));
}

/*
 * call-seq:
 *  compressbound(src) -> size
 *
 * Calcuration maximum size of encoded data in worst case.
 */
static VALUE
rawenc_s_compressbound(VALUE mod, VALUE src)
{
    return SIZET2NUM(aux_lz4_compressbound(src));
}

/*
 * call-seq:
 *  scansize(lz4_rawencoded_data) -> integer
 *
 * Scan raw lz4 data, and get decoded byte size.
 *
 * このメソッドは、raw_decode メソッドに max_dest_size なしで利用する場合の検証目的で利用できるようにしてあります。
 *
 * その他の有用な使い方があるのかは不明です。
 */
static VALUE
rawdec_s_scansize(VALUE mod, VALUE str)
{
    Check_Type(str, RUBY_T_STRING);
    return SIZET2NUM(aux_lz4_scansize(str));
}

/*
 * calculate destination size from source data
 */
typedef size_t aux_calc_destsize_f(VALUE src);

static inline void
rawprocess_args(int argc, VALUE argv[], VALUE *src, VALUE *dest, size_t *maxsize, int *level, aux_calc_destsize_f *calcsize)
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
        *src = aux_shouldbe_string(argv[0]);
        switch (argend - argv) {
        case 1:
            *maxsize = calcsize(*src);
            *dest = rb_str_buf_new(*maxsize);
            return;
        case 2:
            tmp = argv[1];
            if (RB_TYPE_P(tmp, RUBY_T_STRING)) {
                *maxsize = calcsize(*src);
                *dest = aux_shouldbe_string(tmp);
                aux_str_reserve(*dest, *maxsize);
            } else {
                *maxsize = NUM2SIZET(tmp);
                *dest = rb_str_buf_new(*maxsize);
            }
            return;
        case 3:
            *maxsize = NUM2SIZET(argv[1]);
            *dest = aux_shouldbe_string(argv[2]);
            aux_str_reserve(*dest, *maxsize);
            return;
        }
    }

    rb_error_arity(argc, 1, (level ? 4 : 3));
}

/***********/

typedef int aux_lz4_encoder_f(const char *src, char *dest, int srcsize, int maxsize, int level);

static int
aux_LZ4_compress_limitedOutput(const char *src, char *dest, int srcsize, int maxsize, int level)
{
    return LZ4_compress_limitedOutput(src, dest, srcsize, maxsize);
}

/*
 * call-seq:
 *  encode(src) -> compressed string data
 *  encode(src, max_dest_size) -> compressed string data
 *  encode(src, dest) -> dest with compressed string data
 *  encode(src, max_dest_size, dest) -> dest with compressed string data
 *  encode(level, src) -> compressed string data
 *  encode(level, src, max_dest_size) -> compressed string data
 *  encode(level, src, dest) -> dest with compressed string data
 *  encode(level, src, max_dest_size, dest) -> dest with compressed string data
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
rawenc_s_encode(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    int level;
    rawprocess_args(argc, argv, &src, &dest, &maxsize, &level, aux_lz4_compressbound);
    check_security(Qnil, src, dest);

    aux_lz4_encoder_f *encoder;
    if (level < 0) {
        encoder = aux_LZ4_compress_limitedOutput;
    } else {
        encoder = LZ4_compressHC2_limitedOutput;
    }

    size_t srcsize = RSTRING_LEN(src);
    if (srcsize > LZ4_MAX_INPUT_SIZE) {
        rb_raise(eError,
                 "source size is too big for lz4 encode (given %zu, but max %zu bytes)",
                 srcsize, (size_t)LZ4_MAX_INPUT_SIZE);
    }
    aux_str_reserve(dest, maxsize);
    rb_str_set_len(dest, 0);
    rb_obj_infect(dest, src);

    int size = encoder(RSTRING_PTR(src), RSTRING_PTR(dest), srcsize, maxsize, level);
    if (size <= 0) {
        rb_raise(eError,
                 "failed LZ4 compress - maxsize is too small, or out of memory");
    }

    rb_str_set_len(dest, size);

    return dest;
}

/*
 * call-seq:
 *  decode(src) -> decoded string data
 *  decode(src, max_dest_size) -> decoded string data
 *  decode(src, dest) -> dest with decoded string data
 *  decode(src, max_dest_size, dest) -> dest with decoded string data
 *
 * Decode raw LZ4 data.
 *
 * 出力先は、max_dest_size が与えられていない場合、必要に応じて自動的に拡張されます。
 * この場合、いったん圧縮された LZ4 データを走査するため、事前に僅かな CPU 時間を必要とします。
 */
static VALUE
rawdec_s_decode(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    rawprocess_args(argc, argv, &src, &dest, &maxsize, NULL, aux_lz4_scansize);
    check_security(Qnil, src, dest);

    aux_str_reserve(dest, maxsize);
    rb_str_set_len(dest, 0);
    rb_obj_infect(dest, src);

    int size = LZ4_decompress_safe(RSTRING_PTR(src), RSTRING_PTR(dest), RSTRING_LEN(src), maxsize);
    if (size < 0) {
        rb_raise(eError,
                 "failed LZ4_decompress_safe - max_dest_size is too small, or data is corrupted");
    }

    rb_str_set_len(dest, size);

    return dest;
}

/***********/

typedef void rawencoder_reset_f(void *context, int level);
typedef void *rawencoder_create_f(int level);
typedef int rawencoder_free_f(void *context);
typedef int rawencoder_loaddict_f(void *context, const char *dict, int dictsize);
typedef int rawencoder_savedict_f(void *context, char *dict, int dictsize);
typedef int rawencoder_update_f(void *context, const char *src, char *dest, int srcsize, int destsize);
typedef int rawencoder_update_unlinked_f(void *context, const char *src, char *dest, int srcsize, int destsize);

struct rawencoder_traits
{
    rawencoder_reset_f *reset;
    rawencoder_create_f *create;
    rawencoder_free_f *free;
    rawencoder_loaddict_f *loaddict;
    rawencoder_savedict_f *savedict;
    rawencoder_update_f *update;
    rawencoder_update_unlinked_f *update_unlinked;
};

static void
aux_LZ4_resetStream(LZ4_stream_t *context, int level__ignored__)
{
    (void)level__ignored__;
    LZ4_resetStream(context);
}

static const struct rawencoder_traits rawencoder_traits_std = {
    .reset = (rawencoder_reset_f *)aux_LZ4_resetStream,
    .create = (rawencoder_create_f *)LZ4_createStream,
    .free = (rawencoder_free_f *)LZ4_freeStream,
    .loaddict = (rawencoder_loaddict_f *)LZ4_loadDict,
    .savedict = (rawencoder_savedict_f *)LZ4_saveDict,
    .update = (rawencoder_update_f *)LZ4_compress_limitedOutput_continue,
    .update_unlinked = (rawencoder_update_f *)LZ4_compress_limitedOutput_withState,
};

static const struct rawencoder_traits rawencoder_traits_hc = {
    .reset = (rawencoder_reset_f *)LZ4_resetStreamHC,
    .create = (rawencoder_create_f *)LZ4_createStreamHC,
    .free = (rawencoder_free_f *)LZ4_freeStreamHC,
    .loaddict = (rawencoder_loaddict_f *)LZ4_loadDictHC,
    .savedict = (rawencoder_savedict_f *)LZ4_saveDictHC,
    .update = (rawencoder_update_f *)LZ4_compressHC_limitedOutput_continue,
    .update_unlinked = (rawencoder_update_f *)LZ4_compressHC_limitedOutput_withStateHC,
};

struct rawencoder
{
    void *context;
    const struct rawencoder_traits *traits;
    VALUE predict;
    int prefixsize;
    char prefix[1 + (1 << 16)]; /* 64 KiB; LZ4_loadDict, LZ4_saveDict */

////////
//    VALUE predict;      /* preset-dictionary (used when next reset) */
//    VALUE buffer;       /* entity of input buffer */
//    char *inoff;        /* current offset of buffer */
//    const char *intail; /* end offset of buffer */
//    void *lz4;          /* lz4 stream context */
//    size_t blocksize;   /* stream block size (maximum size) */
//    struct rawencoder_traits *traits;
//    VALUE ishc;         /* false: not hc / true: hc */
////////
};

static void
rawenc_mark(void *pp)
{
    if (pp) {
        struct rawencoder *p = pp;
        rb_gc_mark(p->predict);
    }
}

static void
rawenc_free(void *pp)
{
    if (pp) {
        struct rawencoder *p = pp;
        if (p->context && p->traits) {
            p->traits->free(p->context);
        }
        p->context = NULL;
        p->traits = NULL;
    }
}

static const rb_data_type_t rawencoder_type = {
    .wrap_struct_name = "extlz4.LZ4.RawEncoder",
    .function.dmark = rawenc_mark,
    .function.dfree = rawenc_free,
    /* .function.dsize = rawenc_size, */
};

static VALUE
rawenc_alloc(VALUE klass)
{
    struct rawencoder *p;
    VALUE v = TypedData_Make_Struct(klass, struct rawencoder, &rawencoder_type, p);
    p->predict = Qnil;
    return v;
}

static inline struct rawencoder *
getencoderp(VALUE enc)
{
    return getrefp(enc, &rawencoder_type);
}

static inline struct rawencoder *
getencoder(VALUE enc)
{
    return getref(enc, &rawencoder_type);
}

static inline void
rawenc_setup(int argc, VALUE argv[], int *level, struct rawencoder *p)
{
    if (p->context) {
        void *cx = p->context;
        p->context = NULL;
        p->traits->free(cx);
    }

    VALUE level1;
    rb_scan_args(argc, argv, "02", &level1, &p->predict);

    if (NIL_P(level1)) {
        *level = -1;
        p->traits = &rawencoder_traits_std;
    } else {
        *level = NUM2UINT(level1);
        p->traits = &rawencoder_traits_hc;
    }

    if (argc < 2) {
        p->predict = Qundef;
    } else {
        if (!NIL_P(p->predict)) {
            rb_check_type(p->predict, RUBY_T_STRING);
        }
    }

    p->context = p->traits->create(*level);
    if (!p->context) {
        rb_gc();
        p->context = p->traits->create(*level);
        if (!p->context) {
            errno = ENOMEM;
            rb_sys_fail("failed context allocation by LZ4_createStream()");
        }
    }
}

/*
 * call-seq:
 *  initialize(level = nil, predict = nil)
 *
 * [RETURN]
 *      self
 *
 * [level]
 *      When given +nil+, encode normal compression.
 *
 *      When given +0+ .. +15+, encode high compression.
 *
 * [predict]
 *      Pre load dictionary.
 */
static VALUE
rawenc_init(int argc, VALUE argv[], VALUE enc)
{
    struct rawencoder *p = getencoder(enc);
    if (p->context) {
        rb_raise(eError,
                "already initialized - #<%s:%p>",
                rb_obj_classname(enc), (void *)enc);
    }

    int level;
    rawenc_setup(argc, argv, &level, p);
    p->prefixsize = p->traits->savedict(p->context, p->prefix, sizeof(p->prefix));
    if (p->predict == Qundef) {
        p->predict = Qnil;
    } else if (!NIL_P(p->predict)) {
        p->traits->loaddict(p->context, RSTRING_PTR(p->predict), RSTRING_LEN(p->predict));
    }

    return enc;
}

/*
 * update(src [, max_dest_size] [, dest]) -> dest
 */
static VALUE
rawenc_update(int argc, VALUE argv[], VALUE enc)
{
    struct rawencoder *p = getencoder(enc);
    VALUE src, dest;
    size_t maxsize;
    rawprocess_args(argc, argv, &src, &dest, &maxsize, NULL, aux_lz4_compressbound);
    check_security(enc, src, dest);
    rb_obj_infect(enc, src);
    rb_obj_infect(dest, enc);
    char *srcp;
    size_t srcsize;
    RSTRING_GETMEM(src, srcp, srcsize);
    int s = p->traits->update(p->context, srcp, RSTRING_PTR(dest), srcsize, rb_str_capacity(dest));
    if (s <= 0) {
        rb_raise(eError,
                "destsize too small (given destsize is %zu)",
                rb_str_capacity(dest));
    }
    p->prefixsize = p->traits->savedict(p->context, p->prefix, sizeof(p->prefix));
//fprintf(stderr, "%s:%d:%s: rawencoder.prefixsize=%d\n", __FILE__, __LINE__, __func__, p->prefixsize);
    rb_str_set_len(dest, s);
    return dest;
}

/*
 * call-seq:
 *  reset(level = nil) -> self
 *  reset(level, predict) -> self
 *
 * Reset raw stream encoder.
 */
static VALUE
rawenc_reset(int argc, VALUE argv[], VALUE enc)
{
    struct rawencoder *p = getencoder(enc);
    if (!p->context) {
        rb_raise(eError,
                "not initialized yet - #<%s:%p>",
                rb_obj_classname(enc), (void *)enc);
    }

    VALUE predict = p->predict;
    int level;
    rawenc_setup(argc, argv, &level, p);
    if (p->predict == Qundef) {
        p->predict = predict;
    }

    if (!NIL_P(p->predict)) {
        p->traits->loaddict(p->context, RSTRING_PTR(p->predict), RSTRING_LEN(p->predict));
    }

    return enc;
}

static VALUE
rawenc_release(VALUE enc)
{
    struct rawencoder *p = getencoder(enc);
    if (p->traits && p->context) {
        p->traits->free(p->context);
    }
    p->context = NULL;
    p->traits = NULL;
    memset(p->prefix, 0, sizeof(p->prefix));
    p->prefixsize = 0;
    return Qnil;
}

/*
 * class LZ4::RawDecoder
 */

struct rawdecoder
{
    void *context;
    VALUE predict;
    //int prefixsize;
    //char prefix[1 + (1 << 16)]; /* 64 KiB; LZ4_loadDict, LZ4_saveDict */
};

static void
rawdec_mark(void *pp)
{
    if (pp) {
        struct rawdecoder *p = pp;
        rb_gc_mark(p->predict);
    }
}

static void
rawdec_free(void *pp)
{
    if (pp) {
        struct rawdecoder *p = pp;
        if (p->context) {
            LZ4_freeStreamDecode(p->context);
        }
        p->context = NULL;
    }
}

static const rb_data_type_t rawdecoder_type = {
    .wrap_struct_name = "extlz4.LZ4.RawDecoder",
    .function.dmark = rawdec_mark,
    .function.dfree = rawdec_free,
    /* .function.dsize = rawdec_size, */
};

static VALUE
rawdec_alloc(VALUE klass)
{
    struct rawdecoder *p;
    VALUE v = TypedData_Make_Struct(klass, struct rawdecoder, &rawdecoder_type, p);
    p->predict = Qnil;
    return v;
}

static inline struct rawdecoder *
getdecoderp(VALUE dec)
{
    return getrefp(dec, &rawdecoder_type);
}

static inline struct rawdecoder *
getdecoder(VALUE dec)
{
    return getref(dec, &rawdecoder_type);
}

static inline void
rawdec_setup(int argc, VALUE argv[], VALUE predict, struct rawdecoder *p)
{
    VALUE predict1;
    rb_scan_args(argc, argv, "01", &predict1);
    if (argc == 0) {
        p->predict = predict;
    } else {
        if (NIL_P(predict1)) {
            p->predict = predict;
        } else {
            rb_check_type(predict1, RUBY_T_STRING);
            p->predict = predict = rb_str_dup(predict1);
        }
    }

    if (!p->context) {
        p->context = LZ4_createStreamDecode();
        if (!p->context) {
            rb_gc();
            p->context = LZ4_createStreamDecode();
            if (!p->context) {
                errno = ENOMEM;
                rb_sys_fail("failed LZ4_createStreamDecode()");
            }
        }
    }

    if (!NIL_P(predict)) {
        if (LZ4_setStreamDecode(p->context, RSTRING_PTR(predict), RSTRING_LEN(predict)) == 0) {
            rb_raise(eError,
                    "failed set preset dictionary - LZ4_setStreamDecode()");
        }
    } else {
        LZ4_setStreamDecode(p->context, NULL, 0);
    }
}

/*
 * call-seq:
 *  initialize
 *  initialize(preset_dictionary)
 */
static VALUE
rawdec_init(int argc, VALUE argv[], VALUE dec)
{
    struct rawdecoder *p = getdecoder(dec);

    rawdec_setup(argc, argv, Qnil, p);
    rb_obj_infect(p->predict, dec);

    return dec;
}

/*
 * call-seq:
 *  reset
 *  reset(preset_dictionary)
 */
static VALUE
rawdec_reset(int argc, VALUE argv[], VALUE dec)
{
    struct rawdecoder *p = getdecoder(dec);

    rawdec_setup(argc, argv, p->predict, p);
    rb_obj_infect(p->predict, dec);

    return dec;
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
 * [INFECTION]
 *      +src+ -> +self+ -> +dest+
 */
static VALUE
rawdec_update(int argc, VALUE argv[], VALUE dec)
{
    struct rawdecoder *p = getdecoder(dec);
    if (!p->context) { rb_raise(eError, "need reset (context not initialized)"); }
    VALUE src, dest;
    size_t maxsize;
    rawprocess_args(argc, argv, &src, &dest, &maxsize, NULL, aux_lz4_scansize);
    check_security(dec, src, dest);
    rb_obj_infect(dec, src);
    rb_obj_infect(dest, dec);
    const char *srcp;
    size_t srcsize;
    RSTRING_GETMEM(src, srcp, srcsize);
    int s = LZ4_decompress_safe_continue(p->context, srcp, RSTRING_PTR(dest), srcsize, maxsize);
    if (s < 0) {
        rb_raise(eError,
                "`max_dest_size' too small, or corrupt lz4'd data");
    }
    rb_str_set_len(dest, s);
    return dest;
}

/*
 * call-seq:
 *  release -> nil
 *
 * Release allocated internal heap memory.
 */
static VALUE
rawdec_release(VALUE lz4)
{
    struct rawdecoder *p = getdecoderp(lz4);
    if (!p) { return Qnil; }
    if (p->context) {
        LZ4_freeStreamDecode(p->context);
        p->context = NULL;
    }
    // TODO: p->predict と p->prefix も rb_str_resize で 0 にするべきか?
    p->predict = Qnil;
    return Qnil;
}

/*
 * initializer rawapi.c
 */

void
extlz4_init_rawapi(void)
{
    VALUE cRawEncoder = rb_define_class_under(mLZ4, "RawEncoder", rb_cObject);
    rb_define_singleton_method(cRawEncoder, "compressbound", rawenc_s_compressbound, 1);
    rb_define_singleton_method(cRawEncoder, "encode", rawenc_s_encode, -1);
    rb_define_alias(rb_singleton_class(cRawEncoder), "compress", "encode");
    rb_define_alloc_func(cRawEncoder, rawenc_alloc);
    rb_define_method(cRawEncoder, "initialize", RUBY_METHOD_FUNC(rawenc_init), -1);
    rb_define_method(cRawEncoder, "reset", RUBY_METHOD_FUNC(rawenc_reset), -1);
    rb_define_method(cRawEncoder, "update", RUBY_METHOD_FUNC(rawenc_update), -1);
    rb_define_method(cRawEncoder, "release", RUBY_METHOD_FUNC(rawenc_release), 0);
    rb_define_alias(cRawEncoder, "encode", "update");
    rb_define_alias(cRawEncoder, "compress", "update");
    rb_define_alias(cRawEncoder, "free", "release");

    VALUE cRawDecoder = rb_define_class_under(mLZ4, "RawDecoder", rb_cObject);
    rb_define_singleton_method(cRawDecoder, "scansize", rawdec_s_scansize, 1);
    rb_define_singleton_method(cRawDecoder, "decode", rawdec_s_decode, -1);
    rb_define_alias(rb_singleton_class(cRawDecoder), "decompress", "decode");
    rb_define_alias(rb_singleton_class(cRawDecoder), "uncompress", "decode");
    rb_define_alloc_func(cRawDecoder, rawdec_alloc);
    rb_define_method(cRawDecoder, "initialize", RUBY_METHOD_FUNC(rawdec_init), -1);
    rb_define_method(cRawDecoder, "reset", RUBY_METHOD_FUNC(rawdec_reset), -1);
    rb_define_method(cRawDecoder, "update", RUBY_METHOD_FUNC(rawdec_update), -1);
    rb_define_method(cRawDecoder, "release", RUBY_METHOD_FUNC(rawdec_release), 0);
    rb_define_alias(cRawDecoder, "decode", "update");
    rb_define_alias(cRawDecoder, "decompress", "update");
    rb_define_alias(cRawDecoder, "uncompress", "update");
}
