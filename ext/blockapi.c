#include "extlz4.h"
#include <lz4.h>
#include <lz4hc.h>

#define RDOCFAKE(code)

RDOCFAKE(extlz4_mLZ4 = rb_define_module("LZ4"));

#if __GNUC__ || __clang__ || EXTLZ4_FORCE_EXPECT
#define AUX_LIKELY(x)   __builtin_expect(!!(x), 1)
#define AUX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define AUX_LIKELY(x)   (x)
#define AUX_UNLIKELY(x) (x)
#endif


static void *
aux_LZ4_compress_fast_continue_nogvl(va_list *vp)
{
    LZ4_stream_t *context = va_arg(*vp, LZ4_stream_t *);
    const char *src = va_arg(*vp, const char *);
    char *dest = va_arg(*vp, char *);
    int srcsize = va_arg(*vp, int);
    int destsize = va_arg(*vp, int);
    int acceleration = va_arg(*vp, int);

    // NOTE: キャストについては aux_LZ4_decompress_safe_continue_nogvl() を参照されたし
    return (void *)(intptr_t)LZ4_compress_fast_continue(context, src, dest, srcsize, destsize, acceleration);
}

static int
aux_LZ4_compress_fast_continue(void *context, const char *src, char *dest, int srcsize, int destsize, int acceleration)
{
    return (int)aux_thread_call_without_gvl(
            aux_LZ4_compress_fast_continue_nogvl, NULL,
            context, src, dest, srcsize, destsize, acceleration);
}

static void *
aux_LZ4_compressHC_continue_nogvl(va_list *vp)
{
    LZ4_streamHC_t *context = va_arg(*vp, LZ4_streamHC_t *);
    const char *src = va_arg(*vp, const char *);
    char *dest = va_arg(*vp, char *);
    int srcsize = va_arg(*vp, int);
    int destsize = va_arg(*vp, int);

    // NOTE: キャストについては aux_LZ4_decompress_safe_continue_nogvl() を参照されたし
    return (void *)(intptr_t)LZ4_compress_HC_continue(context, src, dest, srcsize, destsize);
}

static int
aux_LZ4_compressHC_continue(void *context, const char *src, char *dest, int srcsize, int destsize, int acceleration__ignored__)
{
    (void)acceleration__ignored__;
    return (int)aux_thread_call_without_gvl(
            aux_LZ4_compressHC_continue_nogvl, NULL,
            context, src, dest, srcsize, destsize);
}

static void *
aux_LZ4_decompress_safe_continue_nogvl(va_list *vp)
{
    LZ4_streamDecode_t *context = va_arg(*vp, LZ4_streamDecode_t *);
    const char *src = va_arg(*vp, const char *);
    char *dest = va_arg(*vp, char *);
    int srcsize = va_arg(*vp, int);
    int maxsize = va_arg(*vp, int);

    // NOTE: キャストを (int) -> (intptr_t) -> (void *) としている理由は、
    // NOTE: (int) -> (void *) とするとコンパイラが警告してくるのを避けるため
    return (void *)(intptr_t)LZ4_decompress_safe_continue(context, src, dest, srcsize, maxsize);
}

static int
aux_LZ4_decompress_safe_continue(LZ4_streamDecode_t *context, const char *src, char *dest, int srcsize, int maxsize)
{
    return (int)aux_thread_call_without_gvl(
            aux_LZ4_decompress_safe_continue_nogvl, NULL,
            context, src, dest, srcsize, maxsize);
}

static inline size_t
aux_lz4_expandsize(const char **p, const char *end, size_t size)
{
    while (AUX_LIKELY(*p < end)) {
        int s = (uint8_t)*(*p) ++;
        size += s;
        if (AUX_LIKELY(s != 255)) {
            return size;
        }
    }

    rb_raise(extlz4_eError, "encounted invalid end of sequence");
}

static inline size_t
aux_lz4_scanseq(const char *p, const char *end, size_t *linksize)
{
    size_t size = 0;
    while (AUX_LIKELY(p < end)) {
        uint8_t token = (uint8_t)*p ++;
        size_t s = token >> 4;
        if (AUX_LIKELY(s == 15)) {
            s = aux_lz4_expandsize(&p, end, s);
        }
        size += s;
        p += s;

        if (AUX_UNLIKELY(p + 2 >= end)) {
            if (p == end) {
#if 0
                s = token & 0x0f;
                if (s != 0) {
                    // TODO: raise? or do nothing?
                }
#endif
                return size;
            }
            break;
        }
        size_t offset = (uint8_t)*p ++;
        offset |= ((size_t)((uint8_t)*p ++)) << 8;
        if (linksize) {
            ssize_t n = offset - size;
            if (AUX_UNLIKELY(n > 0 && n > (ssize_t)*linksize)) {
                *linksize = n;
            }
        }
#if 0
        if (AUX_UNLIKELY(offset == 0)) {
            rb_raise(extlz4_eError, "offset is zero");
        }
#endif
        s = token & 0x0f;
        if (AUX_LIKELY(s == 15)) {
            s = aux_lz4_expandsize(&p, end, s);
        }
        size += s + 4;
    }

    rb_raise(extlz4_eError, "encounted invalid end of sequence");
}

/*
 * lz4 シーケンスから伸張後のバイト数を得る
 *
 * str が文字列であることを保証するのは呼び出し元の責任
 */
static size_t
aux_lz4_scansize(VALUE str)
{
    const char *p;
    size_t size;
    RSTRING_GETMEM(str, p, size);

    return aux_lz4_scanseq(p, p + size, NULL);
}

/*
 * offset トークンがバッファの負の数を表しているか確認する。
 *
 * 戻り値はその最大距離を返す (負の数として見るならば最小値だが、絶対値に変換する)。
 *
 * 名称の link は LZ4 frame からとった。
 */
static size_t
aux_lz4_linksize(VALUE str)
{
    const char *p;
    size_t size;
    RSTRING_GETMEM(str, p, size);

    size_t linksize = 0;
    aux_lz4_scanseq(p, p + size, &linksize);

    return linksize;
}

static inline VALUE
aux_shouldbe_string(VALUE obj)
{
    rb_check_type(obj, RUBY_T_STRING);
    return obj;
}

static inline size_t
aux_lz4_compressbound(VALUE src)
{
    return LZ4_compressBound(RSTRING_LEN(src));
}

enum {
    MAX_PREDICT_SIZE = 65536,
};

static inline VALUE
make_predict(VALUE predict)
{
    if (NIL_P(predict)) {
        return Qnil;
    }

    rb_check_type(predict, RUBY_T_STRING);
    size_t size = RSTRING_LEN(predict);
    if (size == 0) {
        return Qnil;
    }
    if (size > MAX_PREDICT_SIZE) {
        predict = rb_str_subseq(predict, size - MAX_PREDICT_SIZE, MAX_PREDICT_SIZE);
    } else {
        predict = rb_str_dup(predict);
    }
    return rb_str_freeze(predict);
}


/*
 * calculate destination size from source data
 */
typedef size_t aux_calc_destsize_f(VALUE src);

static inline void
blockprocess_args(int argc, VALUE argv[], VALUE *src, VALUE *dest, size_t *maxsize, int *level, aux_calc_destsize_f *calcsize)
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

/*
 * Document-class: LZ4::BlockEncoder
 *
 * このクラスは LZ4 Block API を扱うためのものです。
 */

typedef void blockencoder_reset_f(void *context, int level);
typedef void *blockencoder_create_f(void);
typedef int blockencoder_free_f(void *context);
typedef int blockencoder_loaddict_f(void *context, const char *dict, int dictsize);
typedef int blockencoder_savedict_f(void *context, char *dict, int dictsize);
typedef int blockencoder_update_f(void *context, const char *src, char *dest, int srcsize, int destsize, int acceleration);
typedef int blockencoder_update_unlinked_f(void *context, const char *src, char *dest, int srcsize, int destsize);

struct blockencoder_traits
{
    blockencoder_reset_f *reset;
    blockencoder_create_f *create;
    blockencoder_free_f *free;
    blockencoder_loaddict_f *loaddict;
    blockencoder_savedict_f *savedict;
    blockencoder_update_f *update;
    /* blockencoder_update_unlinked_f *update_unlinked; */
};

static void
aux_LZ4_resetStream(LZ4_stream_t *context, int level__ignored__)
{
    (void)level__ignored__;
    LZ4_resetStream(context);
}

static const struct blockencoder_traits blockencoder_traits_std = {
    .reset = (blockencoder_reset_f *)aux_LZ4_resetStream,
    .create = (blockencoder_create_f *)LZ4_createStream,
    .free = (blockencoder_free_f *)LZ4_freeStream,
    .loaddict = (blockencoder_loaddict_f *)LZ4_loadDict,
    .savedict = (blockencoder_savedict_f *)LZ4_saveDict,
    .update = (blockencoder_update_f *)aux_LZ4_compress_fast_continue,
    /* .update_unlinked = (blockencoder_update_unlinked_f *)LZ4_compress_limitedOutput_withState, */
};

static const struct blockencoder_traits blockencoder_traits_hc = {
    .reset = (blockencoder_reset_f *)LZ4_resetStreamHC,
    .create = (blockencoder_create_f *)LZ4_createStreamHC,
    .free = (blockencoder_free_f *)LZ4_freeStreamHC,
    .loaddict = (blockencoder_loaddict_f *)LZ4_loadDictHC,
    .savedict = (blockencoder_savedict_f *)LZ4_saveDictHC,
    .update = (blockencoder_update_f *)aux_LZ4_compressHC_continue,
    /* .update_unlinked = (blockencoder_update_unlinked_f *)LZ4_compressHC_limitedOutput_withStateHC, */
};

struct blockencoder
{
    void *context;
    const struct blockencoder_traits *traits;
    VALUE predict;
    int level;
    int prefixsize;
    char prefix[1 << 16]; /* 64 KiB; LZ4_loadDict, LZ4_saveDict */
};

static void
blkenc_mark(void *pp)
{
    struct blockencoder *p = pp;
    rb_gc_mark(p->predict);
}

static void
blkenc_free(void *pp)
{
    struct blockencoder *p = pp;
    if (p->context && p->traits) {
        p->traits->free(p->context);
    }
    memset(p, 0, sizeof(*p));
    xfree(p);
}

static const rb_data_type_t blockencoder_type = {
    .wrap_struct_name = "extlz4.LZ4.BlockEncoder",
    .function.dmark = blkenc_mark,
    .function.dfree = blkenc_free,
    /* .function.dsize = blkenc_size, */
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE
blkenc_alloc(VALUE klass)
{
    struct blockencoder *p;
    VALUE v = TypedData_Make_Struct(klass, struct blockencoder, &blockencoder_type, p);
    p->predict = Qnil;
    return v;
}

static inline struct blockencoder *
getencoderp(VALUE enc)
{
    return getrefp(enc, &blockencoder_type);
}

static inline struct blockencoder *
getencoder(VALUE enc)
{
    return getref(enc, &blockencoder_type);
}

static inline struct blockencoder *
encoder_context(VALUE enc)
{
    struct blockencoder *p = getencoder(enc);
    if (!p->context) {
        rb_raise(extlz4_eError,
                "not initialized yet - #<%s:%p>",
                rb_obj_classname(enc), (void *)enc);
    }

    return p;
}

static inline void
blkenc_setup(int argc, VALUE argv[], struct blockencoder *p, VALUE predict)
{
    rb_check_arity(argc, 0, 2);

    if (p->context) {
        void *cx = p->context;
        p->context = NULL;
        p->traits->free(cx);
    }

    if (argc == 0 || NIL_P(argv[0])) {
        p->level = 1;
        p->traits = &blockencoder_traits_std;
    } else {
        p->level = NUM2UINT(argv[0]);
        if (p->level < 0) {
            p->traits = &blockencoder_traits_std;
            p->level = -p->level;
        } else {
            p->traits = &blockencoder_traits_hc;
        }
    }

    if (argc < 2) {
        p->predict = predict;
    } else {
        p->predict = make_predict(argv[1]);
    }

    p->context = p->traits->create();
    if (!p->context) {
        rb_gc();
        p->context = p->traits->create();
        if (!p->context) {
            errno = ENOMEM;
            rb_sys_fail("failed context allocation by LZ4_createStream()");
        }
    }

    p->traits->reset(p->context, p->level);

    if (NIL_P(p->predict)) {
        p->traits->loaddict(p->context, NULL, 0);
    } else {
        /*
         * NOTE: すぐ下で LZ4_saveDict() を実行するため、
         * NOTE: p->predict のバッファ領域が保持されることはない。
         */
        p->traits->loaddict(p->context, RSTRING_PTR(p->predict), RSTRING_LEN(p->predict));
    }

    p->prefixsize = p->traits->savedict(p->context, p->prefix, sizeof(p->prefix));
}

/*
 * call-seq:
 *  initialize(level = nil, predict = nil)
 *
 * [INFECTION]
 *  +self+ <- +predict+
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
 *      Preset dictionary.
 */
static VALUE
blkenc_init(int argc, VALUE argv[], VALUE enc)
{
    struct blockencoder *p = getencoder(enc);
    if (p->context) {
        rb_raise(extlz4_eError,
                "already initialized - #<%s:%p>",
                rb_obj_classname(enc), (void *)enc);
    }

    blkenc_setup(argc, argv, p, Qnil);
    rb_obj_infect(enc, p->predict);

    return enc;
}

/*
 * call-seq:
 *  update(src, dest = "") -> dest
 *  update(src, max_dest_size, dest = "") -> dest
 *
 * [INFECTION]
 *      +dest+ <- +self+ <- +src+
 */
static VALUE
blkenc_update(int argc, VALUE argv[], VALUE enc)
{
    struct blockencoder *p = encoder_context(enc);
    VALUE src, dest;
    size_t maxsize;
    blockprocess_args(argc, argv, &src, &dest, &maxsize, NULL, aux_lz4_compressbound);
    rb_obj_infect(enc, src);
    rb_obj_infect(dest, enc);
    char *srcp;
    size_t srcsize;
    RSTRING_GETMEM(src, srcp, srcsize);
    int s = p->traits->update(p->context, srcp, RSTRING_PTR(dest), srcsize, maxsize, p->level);
    if (s <= 0) {
        rb_raise(extlz4_eError,
                "destsize too small (given destsize is %zu)",
                rb_str_capacity(dest));
    }
    p->prefixsize = p->traits->savedict(p->context, p->prefix, sizeof(p->prefix));
    rb_str_set_len(dest, s);
    return dest;
}

/*
 * call-seq:
 *  reset(level = nil) -> self
 *  reset(level, predict) -> self
 *
 * [INFECTION]
 *  +self+ < +predict+
 *
 * Reset block stream encoder.
 */
static VALUE
blkenc_reset(int argc, VALUE argv[], VALUE enc)
{
    struct blockencoder *p = encoder_context(enc);
    blkenc_setup(argc, argv, p, p->predict);
    rb_obj_infect(enc, p->predict);

    return enc;
}

static VALUE
blkenc_release(VALUE enc)
{
    struct blockencoder *p = getencoder(enc);
    if (p->traits && p->context) {
        p->traits->free(p->context);
    }
    p->context = NULL;
    p->traits = NULL;
    memset(p->prefix, 0, sizeof(p->prefix));
    p->prefixsize = 0;
    return Qnil;
}

static VALUE
blkenc_predict(VALUE enc)
{
    return getencoder(enc)->predict;
}

/*
 * call-seq:
 *  savedict -> dict or nil
 *  savedict(buf) -> buf or nil
 */
static VALUE
blkenc_savedict(int argc, VALUE argv[], VALUE enc)
{
    struct blockencoder *p = encoder_context(enc);
    VALUE dict;

    if (argc == 0) {
        dict = rb_str_buf_new(p->prefixsize);
    } else if (argc == 1) {
        dict = argv[0];
        aux_str_reserve(dict, p->prefixsize);
    } else {
        rb_error_arity(argc, 0, 1);
    }

    memcpy(RSTRING_PTR(dict), p->prefix, p->prefixsize);
    if (p->prefixsize > 0) {
        rb_str_set_len(dict, p->prefixsize);
        rb_obj_infect(dict, enc);
        return dict;
    } else {
        return Qnil;
    }
}

static VALUE
blkenc_inspect(VALUE enc)
{
    struct blockencoder *p = getencoderp(enc);
    if (p && p->context) {
        if (p->traits == &blockencoder_traits_std) {
            return rb_sprintf("#<%s:%p (fast compression %d)%s>",
                    rb_obj_classname(enc), (void *)enc, p->level,
                    (NIL_P(p->predict)) ? "" : " (with predict)");
        } else if (p->traits == &blockencoder_traits_hc) {
            return rb_sprintf("#<%s:%p (high compression %d)%s>",
                    rb_obj_classname(enc), (void *)enc, p->level,
                    (NIL_P(p->predict)) ? "" : " (with predict)");
        } else {
            return rb_sprintf("#<%s:%p **INVALID COMPRESSOR**>",
                    rb_obj_classname(enc), (void *)enc);
        }
    } else {
        return rb_sprintf("#<%s:%p **NOT INITIALIZED**>",
                rb_obj_classname(enc), (void *)enc);
    }
}

/*
 * call-seq:
 *  compressbound(src) -> size
 *
 * Calcuration maximum size of encoded data in worst case.
 */
static VALUE
blkenc_s_compressbound(VALUE mod, VALUE src)
{
    return SIZET2NUM(LZ4_compressBound(NUM2UINT(src)));
}

typedef int aux_lz4_encoder_f(const char *src, char *dest, int srcsize, int maxsize, int level);

/*
 * call-seq:
 *  encode(src, dest = "") -> dest with compressed string data
 *  encode(src, max_dest_size, dest = "") -> dest with compressed string data
 *  encode(level, src, dest = "") -> dest with compressed string data
 *  encode(level, src, max_dest_size, dest = "") -> dest with compressed string data
 *
 * Encode to block LZ4 data.
 *
 * level を指定した場合、より圧縮処理に時間を掛けて圧縮効率を高めることが出来ます。
 *
 * 実装の都合上、圧縮関数は LZ4_compress_fast / LZ4_compress_HC が使われます。
 *
 * [INFECTION]
 *      +dest+ <- +src+
 *
 * [RETURN]
 *      圧縮されたデータが文字列として返ります。dest を指定した場合は、圧縮データを格納した dest を返します。
 *
 *      圧縮データには自身の終わりやデータ長が含まれていないため、伸張する際には余計なデータが付随していると正常に伸張できません。
 *
 * [src]
 *      圧縮対象となる文字列オブジェクトを指定します。
 *
 * [max_dest_size (optional)]
 *      出力バッファの最大バイト数を指定します。圧縮時にこれよりも多くのバッファ長が必要になった場合は例外が発生します。
 *
 *      省略時は src 長から最悪値が計算されます。dest が最初に確保できれば圧縮処理中に例外が発生することがありません。
 *
 * [dest (optional)]
 *      出力先とする文字列オブジェクトを指定します。
 *
 *      max_dest_size が同時に指定されない場合、出力バッファの最大バイト長は src 長から最悪値が求められて調整されます。
 *
 * [level (optional)]
 *      圧縮レベルとしての数値または nil を指定します。
 *
 *      0 を指定した場合、LZ4 の規定値による高効率圧縮処理が行われます。
 *
 *      0 を超えた数値を指定した場合、LZ4 の高効率圧縮処理が行われます。
 *
 *      nil を与えるか省略した場合、通常の圧縮処理が行われます。
 *
 *      0 に満たない数値を指定した場合、高速圧縮処理が行われます。
 *      内部でこの値は絶対値に変換されて LZ4_compress_fast() の acceleration 引数として渡されます。
 */
static VALUE
blkenc_s_encode(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    int level;
    blockprocess_args(argc, argv, &src, &dest, &maxsize, &level, aux_lz4_compressbound);

    aux_lz4_encoder_f *encoder;
    if (level < 0) {
        encoder = LZ4_compress_fast;
        level = -level;
    } else {
        encoder = LZ4_compress_HC;
    }

    size_t srcsize = RSTRING_LEN(src);
    if (srcsize > LZ4_MAX_INPUT_SIZE) {
        rb_raise(extlz4_eError,
                 "source size is too big for lz4 encode (given %zu, but max %zu bytes)",
                 srcsize, (size_t)LZ4_MAX_INPUT_SIZE);
    }
    aux_str_reserve(dest, maxsize);
    rb_str_set_len(dest, 0);
    rb_obj_infect(dest, src);

    int size = encoder(RSTRING_PTR(src), RSTRING_PTR(dest), srcsize, maxsize, level);
    if (size <= 0) {
        rb_raise(extlz4_eError,
                 "failed LZ4 compress - maxsize is too small, or out of memory");
    }

    rb_str_set_len(dest, size);

    return dest;
}

static void
init_blockencoder(void)
{
    VALUE cBlockEncoder = rb_define_class_under(extlz4_mLZ4, "BlockEncoder", rb_cObject);
    rb_define_alloc_func(cBlockEncoder, blkenc_alloc);
    rb_define_method(cBlockEncoder, "initialize", RUBY_METHOD_FUNC(blkenc_init), -1);
    rb_define_method(cBlockEncoder, "reset", RUBY_METHOD_FUNC(blkenc_reset), -1);
    rb_define_method(cBlockEncoder, "update", RUBY_METHOD_FUNC(blkenc_update), -1);
    rb_define_method(cBlockEncoder, "release", RUBY_METHOD_FUNC(blkenc_release), 0);
    rb_define_method(cBlockEncoder, "predict", RUBY_METHOD_FUNC(blkenc_predict), 0);
    rb_define_method(cBlockEncoder, "savedict", RUBY_METHOD_FUNC(blkenc_savedict), -1);
    rb_define_method(cBlockEncoder, "inspect", RUBY_METHOD_FUNC(blkenc_inspect), 0);
    rb_define_alias(cBlockEncoder, "encode", "update");
    rb_define_alias(cBlockEncoder, "compress", "update");
    rb_define_alias(cBlockEncoder, "free", "release");

    rb_define_singleton_method(cBlockEncoder, "compressbound", blkenc_s_compressbound, 1);
    rb_define_singleton_method(cBlockEncoder, "encode", blkenc_s_encode, -1);
    rb_define_alias(rb_singleton_class(cBlockEncoder), "compress", "encode");

    rb_define_const(extlz4_mLZ4, "LZ4HC_CLEVEL_MIN", INT2FIX(LZ4HC_CLEVEL_MIN));
    rb_define_const(extlz4_mLZ4, "LZ4HC_CLEVEL_DEFAULT", INT2FIX(LZ4HC_CLEVEL_DEFAULT));
    rb_define_const(extlz4_mLZ4, "LZ4HC_CLEVEL_OPT_MIN", INT2FIX(LZ4HC_CLEVEL_OPT_MIN));
    rb_define_const(extlz4_mLZ4, "LZ4HC_CLEVEL_MAX", INT2FIX(LZ4HC_CLEVEL_MAX));

    rb_define_const(extlz4_mLZ4, "HC_CLEVEL_MIN", INT2FIX(LZ4HC_CLEVEL_MIN));
    rb_define_const(extlz4_mLZ4, "HC_CLEVEL_DEFAULT", INT2FIX(LZ4HC_CLEVEL_DEFAULT));
    rb_define_const(extlz4_mLZ4, "HC_CLEVEL_OPT_MIN", INT2FIX(LZ4HC_CLEVEL_OPT_MIN));
    rb_define_const(extlz4_mLZ4, "HC_CLEVEL_MAX", INT2FIX(LZ4HC_CLEVEL_MAX));
}

/*
 * class LZ4::BlockDecoder
 */

struct blockdecoder
{
    void *context;
    VALUE predict;
    size_t dictsize;
    char dictbuf[64 * 1024];
};

static void
blkdec_mark(void *pp)
{
    struct blockdecoder *p = pp;
    rb_gc_mark(p->predict);
}

static void
blkdec_free(void *pp)
{
    struct blockdecoder *p = pp;
    if (p->context) {
        LZ4_freeStreamDecode(p->context);
    }
    memset(p, 0, sizeof(*p));
    xfree(p);
}

static const rb_data_type_t blockdecoder_type = {
    .wrap_struct_name = "extlz4.LZ4.BlockDecoder",
    .function.dmark = blkdec_mark,
    .function.dfree = blkdec_free,
    /* .function.dsize = blkdec_size, */
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE
blkdec_alloc(VALUE klass)
{
    struct blockdecoder *p;
    VALUE v = TypedData_Make_Struct(klass, struct blockdecoder, &blockdecoder_type, p);
    p->predict = Qnil;
    return v;
}

static inline struct blockdecoder *
getdecoderp(VALUE dec)
{
    return getrefp(dec, &blockdecoder_type);
}

static inline struct blockdecoder *
getdecoder(VALUE dec)
{
    return getref(dec, &blockdecoder_type);
}

static inline void
blkdec_setup(int argc, VALUE argv[], VALUE predict, struct blockdecoder *p)
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
        const char *pdp;
        RSTRING_GETMEM(predict, pdp, p->dictsize);
        if (p->dictsize > sizeof(p->dictbuf)) {
            pdp += p->dictsize - sizeof(p->dictbuf);
            p->dictsize = sizeof(p->dictbuf);
        }

        memcpy(p->dictbuf, pdp, p->dictsize);
    } else {
        p->dictsize = 0;
    }
}

/*
 * call-seq:
 *  initialize
 *  initialize(preset_dictionary)
 *
 * [INFECTION]
 *  +self+ < +preset_dictionary+
 */
static VALUE
blkdec_init(int argc, VALUE argv[], VALUE dec)
{
    struct blockdecoder *p = getdecoder(dec);

    blkdec_setup(argc, argv, Qnil, p);
    rb_obj_infect(dec, p->predict);

    return dec;
}

/*
 * call-seq:
 *  reset
 *  reset(preset_dictionary)
 *
 * [INFECTION]
 *  +self+ < +preset_dictionary+
 */
static VALUE
blkdec_reset(int argc, VALUE argv[], VALUE dec)
{
    struct blockdecoder *p = getdecoder(dec);

    blkdec_setup(argc, argv, p->predict, p);
    rb_obj_infect(dec, p->predict);

    return dec;
}

/*
 * call-seq:
 *  update(src, dest = "") -> dest for decoded string data
 *  update(src, max_dest_size, dest = "") -> dest for decoded string data
 *
 * Decode block lz4 data of stream block.
 *
 * Given arguments and return values are same as LZ4#block_decode.
 * See LZ4#block_decode for about its.
 *
 * 出力先は、max_dest_size が与えられていない場合、必要に応じて自動的に拡張されます。
 * この場合、いったん圧縮された LZ4 データを走査するため、事前に僅かな CPU 時間を必要とします。
 *
 * [INFECTION]
 *      +dest+ < +self+ < +src+
 */
static VALUE
blkdec_update(int argc, VALUE argv[], VALUE dec)
{
    struct blockdecoder *p = getdecoder(dec);
    if (!p->context) { rb_raise(extlz4_eError, "need reset (context not initialized)"); }
    VALUE src, dest;
    size_t maxsize;
    blockprocess_args(argc, argv, &src, &dest, &maxsize, NULL, aux_lz4_scansize);
    rb_obj_infect(dec, src);
    rb_obj_infect(dest, dec);
    const char *srcp;
    size_t srcsize;
    RSTRING_GETMEM(src, srcp, srcsize);
    LZ4_setStreamDecode(p->context, p->dictbuf, p->dictsize);
    int s = aux_LZ4_decompress_safe_continue(p->context, srcp, RSTRING_PTR(dest), srcsize, maxsize);
    if (s < 0) {
        rb_raise(extlz4_eError,
                "`max_dest_size' too small, or corrupt lz4'd data");
    }
    rb_str_set_len(dest, s);

    /* copy prefix */
    if (s < sizeof(p->dictbuf)) {
        ssize_t discard = (p->dictsize + s) - sizeof(p->dictbuf);
        if (discard > 0) {
            size_t remain = p->dictsize - discard;
            memmove(p->dictbuf, (const char *)(p->dictbuf + discard), remain);
            p->dictsize = remain;
        }

        memcpy(p->dictbuf + p->dictsize, RSTRING_PTR(dest), s);
        p->dictsize += s;
    } else {
        memcpy(p->dictbuf, RSTRING_END(dest) - sizeof(p->dictbuf), sizeof(p->dictbuf));
        p->dictsize = sizeof(p->dictbuf);
    }

    return dest;
}

/*
 * call-seq:
 *  release -> nil
 *
 * Release allocated internal heap memory.
 */
static VALUE
blkdec_release(VALUE lz4)
{
    struct blockdecoder *p = getdecoderp(lz4);
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
 * call-seq:
 *  scansize(lz4_blockencoded_data) -> integer
 *
 * Scan block lz4 data, and get decoded byte size.
 *
 * このメソッドは、block_decode メソッドに max_dest_size なしで利用する場合の検証目的で利用できるようにしてあります。
 *
 * その他の有用な使い方があるのかは不明です。
 */
static VALUE
blkdec_s_scansize(VALUE mod, VALUE str)
{
    rb_check_type(str, RUBY_T_STRING);
    return SIZET2NUM(aux_lz4_scansize(str));
}

/*
 * call-seq:
 *  linksize(lz4_blockencoded_data) -> prefix size as integer
 *
 * Scan block lz4 data, and get prefix byte size.
 */
static VALUE
blkdec_s_linksize(VALUE mod, VALUE str)
{
    rb_check_type(str, RUBY_T_STRING);
    return SIZET2NUM(aux_lz4_linksize(str));
}

/*
 * call-seq:
 *  decode(src, dest = "") -> dest with decoded string data
 *  decode(src, max_dest_size, dest = "") -> dest with decoded string data
 *
 * Decode block LZ4 data.
 *
 * 出力先は、max_dest_size が与えられていない場合、必要に応じて自動的に拡張されます。
 * この場合、いったん圧縮された LZ4 データを走査するため、事前に僅かな CPU 時間を必要とします。
 *
 * [INFECTION]
 *      +dest+ < +src+
 */
static VALUE
blkdec_s_decode(int argc, VALUE argv[], VALUE lz4)
{
    VALUE src, dest;
    size_t maxsize;
    blockprocess_args(argc, argv, &src, &dest, &maxsize, NULL, aux_lz4_scansize);

    aux_str_reserve(dest, maxsize);
    rb_str_set_len(dest, 0);
    rb_obj_infect(dest, src);

    int size = LZ4_decompress_safe(RSTRING_PTR(src), RSTRING_PTR(dest), RSTRING_LEN(src), maxsize);
    if (size < 0) {
        rb_raise(extlz4_eError,
                 "failed LZ4_decompress_safe - max_dest_size is too small, or data is corrupted");
    }

    rb_str_set_len(dest, size);

    return dest;
}

static void
init_blockdecoder(void)
{
    VALUE cBlockDecoder = rb_define_class_under(extlz4_mLZ4, "BlockDecoder", rb_cObject);
    rb_define_alloc_func(cBlockDecoder, blkdec_alloc);
    rb_define_method(cBlockDecoder, "initialize", RUBY_METHOD_FUNC(blkdec_init), -1);
    rb_define_method(cBlockDecoder, "reset", RUBY_METHOD_FUNC(blkdec_reset), -1);
    rb_define_method(cBlockDecoder, "update", RUBY_METHOD_FUNC(blkdec_update), -1);
    rb_define_method(cBlockDecoder, "release", RUBY_METHOD_FUNC(blkdec_release), 0);
    rb_define_alias(cBlockDecoder, "decode", "update");
    rb_define_alias(cBlockDecoder, "decompress", "update");
    rb_define_alias(cBlockDecoder, "uncompress", "update");
    rb_define_alias(cBlockDecoder, "free", "release");

    rb_define_singleton_method(cBlockDecoder, "scansize", blkdec_s_scansize, 1);
    rb_define_singleton_method(cBlockDecoder, "linksize", blkdec_s_linksize, 1);
    rb_define_singleton_method(cBlockDecoder, "decode", blkdec_s_decode, -1);
    rb_define_alias(rb_singleton_class(cBlockDecoder), "decompress", "decode");
    rb_define_alias(rb_singleton_class(cBlockDecoder), "uncompress", "decode");
}

/*
 * initializer blockapi.c
 */

void
extlz4_init_blockapi(void)
{
    init_blockencoder();
    init_blockdecoder();
}
