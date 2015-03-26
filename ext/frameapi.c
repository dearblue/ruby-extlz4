#include "extlz4.h"
#include <lz4frame.h>
#include <lz4frame_static.h>
#include "hashargs.h"

static ID id_op_lshift;
static ID id_read;

enum {
    FLAG_LEGACY = 1 << 0,
    FLAG_BLOCK_LINK = 1 << 1,
    FLAG_BLOCK_SUM = 1 << 2,
    FLAG_STREAM_SIZE = 1 << 3,
    FLAG_STREAM_SUM = 1 << 4,

    WORK_BUFFER_SIZE = 256 * 1024, /* 256 KiB */
    AUX_LZ4FRAME_HEADER_MAX = 19,

    AUX_LZ4F_BLOCK_SIZE_MAX = 4 * 1024 * 1024, /* 4 MiB : maximum block size of LZ4 Frame */
    AUX_LZ4F_FINISH_SIZE = 16, /* from lz4frame.c */
};

/*** auxiliary and common functions ***/

static inline void
aux_lz4f_check_error(size_t err)
{
    if (LZ4F_isError(err)) {
        rb_raise(eError,
                 "%s (0x%04x)",
                 LZ4F_getErrorName(err), -(int)err);
    }
}

/*** class LZ4::Encoder ***/

struct encoder
{
    VALUE outport;
    VALUE workbuf;
    VALUE predict;
    LZ4F_preferences_t prefs;
    LZ4F_compressionContext_t encoder;
};

static void
encoder_mark(void *pp)
{
    if (pp) {
        struct encoder *p = pp;
        rb_gc_mark(p->outport);
        rb_gc_mark(p->workbuf);
        rb_gc_mark(p->predict);
    }
}

static void
encoder_free(void *pp)
{
    if (pp) {
        struct encoder *p = pp;
        if (p->encoder) {
            LZ4F_freeCompressionContext(p->encoder);
        }
    }
}

static const rb_data_type_t encoder_type = {
    .wrap_struct_name = "extlz4.LZ4.Encoder",
    .function.dmark = encoder_mark,
    .function.dfree = encoder_free,
    /* .function.dsize = encoder_size, */
};

static VALUE
fenc_alloc(VALUE mod)
{
    struct encoder *p;
    VALUE obj = TypedData_Make_Struct(mod, struct encoder, &encoder_type, p);
    p->outport = Qnil;
    p->workbuf = Qnil;
    p->predict = Qnil;
    return obj;
}

static inline void
fenc_init_args(int argc, VALUE argv[], VALUE *outport, LZ4F_preferences_t *prefs)
{
    VALUE level, opts;
    rb_scan_args(argc, argv, "11:", outport, &level, &opts);

    if (!NIL_P(opts)) {
        VALUE blocklink, streamsum;
        RBX_SCANHASH(opts, Qnil,
                RBX_SCANHASH_ARGS("level", &level, INT2FIX(1)),
                RBX_SCANHASH_ARGS("blocklink", &blocklink, Qfalse),
                RBX_SCANHASH_ARGS("streamsum", &streamsum, Qtrue));
        memset(prefs, 0, sizeof(*prefs));
        // prefs->autoFlush = ????;
        //prefs->frameInfo.blockSizeID = ; /* max64KB, max256KB, max1MB, max4MB ; 0 == default */
        prefs->frameInfo.blockMode = RTEST(blocklink) ? blockLinked : blockIndependent;
        prefs->frameInfo.contentChecksumFlag = RTEST(streamsum) ? contentChecksumEnabled : noContentChecksum;
    } else {
        memset(prefs, 0, sizeof(*prefs));
    }
    prefs->compressionLevel = NUM2INT(level);
}

/*
 * call-seq:
 *  initialize(outport, level = 1, blocklinked: false, streamsum: true)
 */
static VALUE
fenc_init(int argc, VALUE argv[], VALUE enc)
{
    struct encoder *p = getref(enc, &encoder_type);
    VALUE outport;
    fenc_init_args(argc, argv, &outport, &p->prefs);

    LZ4F_errorCode_t status;
    status = LZ4F_createCompressionContext(&p->encoder, LZ4F_VERSION);
    aux_lz4f_check_error(status);
    p->workbuf = rb_str_buf_new(AUX_LZ4F_BLOCK_SIZE_MAX);
    size_t s = LZ4F_compressBegin(p->encoder, RSTRING_PTR(p->workbuf), rb_str_capacity(p->workbuf), &p->prefs);
    aux_lz4f_check_error(s);
    rb_str_set_len(p->workbuf, s);
    rb_funcall2(outport, id_op_lshift, 1, &p->workbuf);
    p->outport = outport;
    return enc;
}

static inline void
fenc_update(struct encoder *p, VALUE src, LZ4F_compressOptions_t *opts)
{
    rb_check_type(src, RUBY_T_STRING);
    const char *srcp = RSTRING_PTR(src);
    const char *srctail = srcp + RSTRING_LEN(src);
    while (srcp < srctail) {
        size_t srcsize = srctail - srcp;
        if (srcsize > AUX_LZ4F_BLOCK_SIZE_MAX) { srcsize = AUX_LZ4F_BLOCK_SIZE_MAX; }
        size_t destsize = LZ4F_compressBound(srcsize, &p->prefs);
        aux_str_reserve(p->workbuf, destsize);
        char *destp = RSTRING_PTR(p->workbuf);
        size_t size = LZ4F_compressUpdate(p->encoder, destp, destsize, srcp, srcsize, opts);
        aux_lz4f_check_error(size);
        rb_str_set_len(p->workbuf, size);
        rb_funcall2(p->outport, id_op_lshift, 1, &p->workbuf);
        srcp += srcsize;
    }
}

/*
 * call-seq:
 *  write(src) -> self
 */
static VALUE
fenc_write(int argc, VALUE argv[], VALUE enc)
{
    struct encoder *p = getref(enc, &encoder_type);
    VALUE src;
    rb_scan_args(argc, argv, "1", &src);
    fenc_update(p, src, NULL);
    return enc;
}

static VALUE
fenc_push(VALUE enc, VALUE src)
{
    struct encoder *p = getref(enc, &encoder_type);
    fenc_update(p, src, NULL);
    return enc;
}

static VALUE
fenc_flush(int argc, VALUE argv[], VALUE enc)
{
    struct encoder *p = getref(enc, &encoder_type);
    size_t destsize = AUX_LZ4F_BLOCK_SIZE_MAX + AUX_LZ4F_FINISH_SIZE;
    aux_str_reserve(p->workbuf, destsize);
    //rb_str_locktmp(p->workbuf);
    char *destp = RSTRING_PTR(p->workbuf);
    size_t size = LZ4F_flush(p->encoder, destp, destsize, NULL);
    //rb_str_unlocktmp(p->workbuf);
    aux_lz4f_check_error(size);
    rb_str_set_len(p->workbuf, size);
    rb_funcall2(p->outport, id_op_lshift, 1, &p->workbuf);

    return enc;
}

static VALUE
fenc_close(VALUE enc)
{
    struct encoder *p = getref(enc, &encoder_type);
    size_t destsize = AUX_LZ4F_BLOCK_SIZE_MAX + AUX_LZ4F_FINISH_SIZE;
    aux_str_reserve(p->workbuf, destsize);
    //rb_str_locktmp(p->workbuf);
    char *destp = RSTRING_PTR(p->workbuf);
    size_t size = LZ4F_compressEnd(p->encoder, destp, destsize, NULL);
    //rb_str_unlocktmp(p->workbuf);
    aux_lz4f_check_error(size);
    rb_str_set_len(p->workbuf, size);
    rb_funcall2(p->outport, id_op_lshift, 1, &p->workbuf);

    return enc;
}

/*** class LZ4::Decoder ***/

struct decoder
{
    VALUE inport;
    VALUE readbuf;   /* read buffer from inport */
    VALUE blockbuf; /* decoded lz4 frame block buffer */
    VALUE predict;   /* preset dictionary (OBSOLUTE) */ /* FIXME: DELETE ME */
    size_t readsize; /* readblocksize in initialize */
    size_t status; /* status code of LZ4F_decompress */
    LZ4F_frameInfo_t info;
    LZ4F_decompressionContext_t decoder;
};

static void
decoder_mark(void *pp)
{
    if (pp) {
        struct decoder *p = pp;
        rb_gc_mark(p->inport);
        rb_gc_mark(p->readbuf);
        rb_gc_mark(p->blockbuf);
        rb_gc_mark(p->predict);
    }
}

static void
decoder_free(void *pp)
{
    if (pp) {
        struct decoder *p = pp;
        if (p->decoder) {
            LZ4F_freeDecompressionContext(p->decoder);
        }
    }
}

static const rb_data_type_t decoder_type = {
    .wrap_struct_name = "extlz4.LZ4.Decoder",
    .function.dmark = decoder_mark,
    .function.dfree = decoder_free,
    /* .function.dsize = decoder_size, */
};

static VALUE
fdec_alloc(VALUE mod)
{
    struct decoder *p;
    VALUE obj = TypedData_Make_Struct(mod, struct decoder, &decoder_type, p);
    p->inport = Qnil;
    p->readbuf = Qnil;
    p->blockbuf = Qnil;
    p->predict = Qnil;
    p->status = ~(size_t)0;
    return obj;
}

static inline VALUE
aux_read(VALUE obj, size_t size, VALUE buf)
{
    if (NIL_P(buf)) { buf = rb_str_buf_new(size); }
    if (NIL_P(AUX_FUNCALL(obj, id_read, SIZET2NUM(size), buf))) {
//fprintf(stderr, "%s:%d:%s: buffer.size=nil\n", __FILE__, __LINE__, __func__);
        return Qnil;
    } else {
//fprintf(stderr, "%s:%d:%s: buffer.size=%d\n", __FILE__, __LINE__, __func__, (int)RSTRING_LEN(buf));
        if (RSTRING_LEN(buf) > size) {
            rb_raise(rb_eRuntimeError, "read buffer is too big (%d, but expected to %d)", (int)RSTRING_LEN(buf), (int)size);
        }
        return buf;
    }
}

/*
 * call-seq:
 *  initialize(inport) -> self
 *
 * [inport]
 *  An I/O (liked) object for data read from LZ4 Frame.
 *
 *  This object need +.read+ method.
 */
static VALUE
fdec_init(int argc, VALUE argv[], VALUE dec)
{
    struct decoder *p = getref(dec, &decoder_type);
    VALUE inport, readblocksize;
    rb_scan_args(argc, argv, "1", &inport);
    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&p->decoder, LZ4F_VERSION);
    aux_lz4f_check_error(err);
    p->inport = inport;
    //p->readsize = NIL_P(readblocksize) ? WORK_BUFFER_SIZE : NUM2INT(readblocksize);
    p->readsize = 0;
    p->blockbuf = rb_str_buf_new(AUX_LZ4F_BLOCK_SIZE_MAX);
    p->readbuf = rb_str_buf_new(0);
    char *readp;
    size_t readsize;
    size_t zero = 0;
    size_t s = 4; /* magic number size of lz4 frame */
    int i;
    for (i = 0; i < 2; i ++) {
        /*
         * first step: check magic number
         * second step: check frame information
         */
        aux_read(inport, s, p->readbuf);
        aux_str_getmem(p->readbuf, &readp, &readsize);
        if (!readp || readsize < s) {
            rb_raise(eError, "read error (or already EOF)");
        }
        s = LZ4F_decompress(p->decoder, NULL, &zero, readp, &readsize, NULL);
        aux_lz4f_check_error(s);
    }
    p->status = s;
    s = LZ4F_getFrameInfo(p->decoder, &p->info, NULL, &zero);
    aux_lz4f_check_error(s);

    return dec;
}

static inline void
fdec_read_args(int argc, VALUE argv[], size_t *size, VALUE *buf)
{
    switch (argc) {
    case 0:
        *size = ~(size_t)0;
        *buf = rb_str_buf_new(0);
        break;
    case 1:
        *size = NUM2SIZET(argv[0]);
        *buf = rb_str_buf_new(*size);
        break;
    case 2:
        *size = NUM2SIZET(argv[0]);
        *buf = argv[1];
        rb_check_type(*buf, RUBY_T_STRING);
        rb_str_modify(*buf);
        rb_str_set_len(*buf, 0);
        break;
    default:
        rb_error_arity(argc, 0, 2);
    }
}

static void
fdec_read_fetch(char **blockp, size_t *blocksize, struct decoder *p)
{
    RSTRING_GETMEM(p->blockbuf, *blockp, *blocksize);
    if (*blocksize > 0) {
        return;
    }

    while (*blocksize <= 0 && p->status != 0) {
        aux_read(p->inport, p->status, p->readbuf);
        char *readp;
        size_t readsize;
        aux_str_getmem(p->readbuf, &readp, &readsize);
        if (!readp) {
            rb_raise(eError,
                    "read error - encounted invalid EOF - #<%s:%p>",
                    rb_obj_classname(p->inport), (void *)p->inport);
        }

        *blocksize = rb_str_capacity(p->blockbuf);
        p->status = LZ4F_decompress(p->decoder, *blockp, blocksize, readp, &readsize, NULL);
        aux_lz4f_check_error(p->status);
    }
}

/*
 * call-seq:
 *  read -> string
 *  read(size) -> string
 *  read(size, buffer) -> buffer
 */
static VALUE
fdec_read(int argc, VALUE argv[], VALUE dec)
{
    struct decoder *p = getref(dec, &decoder_type);
    size_t size;
    VALUE dest;
    fdec_read_args(argc, argv, &size, &dest);
    if (size == 0) {
        return dest;
    }

    if (p->status == 0) {
        return Qnil;
    }

    rb_str_modify(p->blockbuf);

    do {
        char *blockp;
        size_t blocksize;
        fdec_read_fetch(&blockp, &blocksize, p);

        if (size < blocksize) {
            rb_str_buf_cat(dest, blockp, size);
            rb_str_set_len(p->blockbuf, blocksize);
            aux_str_drop_bytes(p->blockbuf, size);
            size = 0;
        } else {
            rb_str_buf_cat(dest, blockp, blocksize);
            rb_str_set_len(p->blockbuf, 0);
            size -= blocksize;
        }
    } while (size > 0 && p->status != 0);

    return dest;
}

static VALUE
fdec_close(VALUE dec)
{
    struct decoder *p = getref(dec, &decoder_type);
    p->status = 0;
    // TODO: destroy decoder
    return dec;
}

static VALUE
fdec_eof(VALUE dec)
{
    struct decoder *p = getref(dec, &decoder_type);
    if (p->status == 0) {
        return Qtrue;
    } else {
        return Qfalse;
    }
}

/*** setup for LZ4::Encoder and LZ4::Decoder ***/

void
extlz4_init_frameapi(void)
{
    id_op_lshift = rb_intern("<<");
    id_read = rb_intern("read");

    VALUE cEncoder = rb_define_class_under(mLZ4, "Encoder", rb_cObject);
    rb_define_alloc_func(cEncoder, fenc_alloc);
    rb_define_method(cEncoder, "initialize", RUBY_METHOD_FUNC(fenc_init), -1);
    rb_define_method(cEncoder, "write", RUBY_METHOD_FUNC(fenc_write), -1);
    rb_define_method(cEncoder, "<<", RUBY_METHOD_FUNC(fenc_push), 1);
    rb_define_method(cEncoder, "flush", RUBY_METHOD_FUNC(fenc_flush), -1);
    rb_define_method(cEncoder, "close", RUBY_METHOD_FUNC(fenc_close), 0);

    VALUE cDecoder = rb_define_class_under(mLZ4, "Decoder", rb_cObject);
    rb_define_alloc_func(cDecoder, fdec_alloc);
    rb_define_method(cDecoder, "initialize", RUBY_METHOD_FUNC(fdec_init), -1);
    rb_define_method(cDecoder, "read", RUBY_METHOD_FUNC(fdec_read), -1);
    rb_define_method(cDecoder, "close", RUBY_METHOD_FUNC(fdec_close), 0);
    rb_define_method(cDecoder, "eof", RUBY_METHOD_FUNC(fdec_eof), 0);
    rb_define_alias(cDecoder, "eof?", "eof");
}
