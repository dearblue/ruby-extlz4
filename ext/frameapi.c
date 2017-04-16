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
        rb_raise(extlz4_eError,
                 "%s (0x%04x)",
                 LZ4F_getErrorName(err), -(int)err);
    }
}

static void *
aux_LZ4F_compressUpdate_nogvl(void *pp)
{
    va_list *p = pp;
    LZ4F_compressionContext_t encoder = va_arg(*p, LZ4F_compressionContext_t);
    char *dest = va_arg(*p, char *);
    size_t destsize = va_arg(*p, size_t);
    const char *src = va_arg(*p, const char *);
    size_t srcsize = va_arg(*p, size_t);
    LZ4F_compressOptions_t *opts = va_arg(*p, LZ4F_compressOptions_t *);

    return (void *)LZ4F_compressUpdate(encoder, dest, destsize, src, srcsize, opts);
}

static size_t
aux_LZ4F_compressUpdate(LZ4F_compressionContext_t encoder,
        char *dest, size_t destsize, const char *src, size_t srcsize,
        LZ4F_compressOptions_t *opts)
{
    return (size_t)aux_thread_call_without_gvl(aux_LZ4F_compressUpdate_nogvl, NULL,
            encoder, dest, destsize, src, srcsize, opts);
}

static int
aux_frame_level(const LZ4F_preferences_t *p)
{
    return p->compressionLevel;
}

static int
aux_frame_blocksize(const LZ4F_frameInfo_t *info)
{
    int bsid = info->blockSizeID;
    if (bsid == 0) {
        bsid = LZ4F_max4MB;
    }
    return 1 << (bsid * 2 + 8);
}

static int
aux_frame_blocklink(const LZ4F_frameInfo_t *info)
{
    return info->blockMode == LZ4F_blockLinked;
}

static int
aux_frame_checksum(const LZ4F_frameInfo_t *info)
{
    return info->contentChecksumFlag == LZ4F_contentChecksumEnabled;
}

/*** class LZ4::Encoder ***/

struct encoder
{
    VALUE outport;
    VALUE workbuf;
    LZ4F_preferences_t prefs;
    LZ4F_compressionContext_t encoder;
};

static void
encoder_mark(void *pp)
{
    struct encoder *p = pp;
    rb_gc_mark(p->outport);
    rb_gc_mark(p->workbuf);
}

static void
encoder_free(void *pp)
{
    struct encoder *p = pp;
    if (p->encoder) {
        LZ4F_freeCompressionContext(p->encoder);
    }
    memset(p, 0, sizeof(*p));
    xfree(p);
}

static const rb_data_type_t encoder_type = {
    .wrap_struct_name = "extlz4.LZ4.Encoder",
    .function.dmark = encoder_mark,
    .function.dfree = encoder_free,
    /* .function.dsize = encoder_size, */
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE
fenc_alloc(VALUE mod)
{
    struct encoder *p;
    VALUE obj = TypedData_Make_Struct(mod, struct encoder, &encoder_type, p);
    p->outport = Qnil;
    p->workbuf = Qnil;
    return obj;
}

static inline int
fenc_init_args_blocksize(size_t size)
{
    if (size == 0) {
        return LZ4F_default;
    } else if (size <= 64 * 1024) {
        return LZ4F_max64KB;
    } else if (size <= 256 * 1024) {
        return LZ4F_max256KB;
    } else if (size <= 1 * 1024 * 1024) {
        return LZ4F_max1MB;
    } else {
        return LZ4F_max4MB;
    }
}

static inline void
fenc_init_args(int argc, VALUE argv[], VALUE *outport, LZ4F_preferences_t *prefs)
{
    VALUE level, opts;
    rb_scan_args(argc, argv, "02:", outport, &level, &opts);

    memset(prefs, 0, sizeof(*prefs));

    if (NIL_P(*outport)) {
        *outport = rb_str_buf_new(0);
    }

    prefs->compressionLevel = NIL_P(level) ? 1 : NUM2INT(level);

    if (!NIL_P(opts)) {
        VALUE blocksize, blocklink, checksum;
        RBX_SCANHASH(opts, Qnil,
                RBX_SCANHASH_ARGS("blocksize", &blocksize, Qnil),
                RBX_SCANHASH_ARGS("blocklink", &blocklink, Qfalse),
                RBX_SCANHASH_ARGS("checksum", &checksum, Qtrue));
        // prefs->autoFlush = TODO;
        prefs->frameInfo.blockSizeID = NIL_P(blocksize) ? LZ4F_default : fenc_init_args_blocksize(NUM2INT(blocksize));
        prefs->frameInfo.blockMode = RTEST(blocklink) ? LZ4F_blockLinked : LZ4F_blockIndependent;
        prefs->frameInfo.contentChecksumFlag = RTEST(checksum) ? LZ4F_contentChecksumEnabled : LZ4F_noContentChecksum;
    } else {
        prefs->frameInfo.blockSizeID = LZ4F_default;
        prefs->frameInfo.blockMode = LZ4F_blockIndependent;
        prefs->frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    }
}

static struct encoder *
getencoderp(VALUE enc)
{
    return getrefp(enc, &encoder_type);
}

static struct encoder *
getencoder(VALUE enc)
{
    return getref(enc, &encoder_type);
}

/*
 * call-seq:
 *  initialize(outport = "".b, level = 1, blocksize: nil, blocklink: false, checksum: true)
 */
static VALUE
fenc_init(int argc, VALUE argv[], VALUE enc)
{
    struct encoder *p = getencoder(enc);
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
    rb_obj_infect(p->outport, enc);
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
        size_t size = aux_LZ4F_compressUpdate(p->encoder, destp, destsize, srcp, srcsize, opts);
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
    struct encoder *p = getencoder(enc);
    VALUE src;
    rb_scan_args(argc, argv, "1", &src);
    rb_obj_infect(enc, src);
    rb_obj_infect(enc, p->workbuf);
    rb_obj_infect(p->outport, enc);
    fenc_update(p, src, NULL);
    return enc;
}

static VALUE
fenc_push(VALUE enc, VALUE src)
{
    struct encoder *p = getencoder(enc);
    rb_obj_infect(enc, src);
    rb_obj_infect(enc, p->workbuf);
    rb_obj_infect(p->outport, enc);
    fenc_update(p, src, NULL);
    return enc;
}

static VALUE
fenc_flush(VALUE enc)
{
    struct encoder *p = getencoder(enc);
    size_t destsize = AUX_LZ4F_BLOCK_SIZE_MAX + AUX_LZ4F_FINISH_SIZE;
    aux_str_reserve(p->workbuf, destsize);
    char *destp = RSTRING_PTR(p->workbuf);
    size_t size = LZ4F_flush(p->encoder, destp, destsize, NULL);
    aux_lz4f_check_error(size);
    rb_str_set_len(p->workbuf, size);
    rb_funcall2(p->outport, id_op_lshift, 1, &p->workbuf);

    return enc;
}

static VALUE
fenc_close(VALUE enc)
{
    struct encoder *p = getencoder(enc);
    size_t destsize = AUX_LZ4F_BLOCK_SIZE_MAX + AUX_LZ4F_FINISH_SIZE;
    aux_str_reserve(p->workbuf, destsize);
    char *destp = RSTRING_PTR(p->workbuf);
    size_t size = LZ4F_compressEnd(p->encoder, destp, destsize, NULL);
    aux_lz4f_check_error(size);
    rb_str_set_len(p->workbuf, size);
    rb_funcall2(p->outport, id_op_lshift, 1, &p->workbuf);

    return enc;
}

static VALUE
fenc_getoutport(VALUE enc)
{
    return getencoder(enc)->outport;
}

static VALUE
fenc_setoutport(VALUE enc, VALUE outport)
{
    return getencoder(enc)->outport = outport;
}

static VALUE
fenc_prefs_level(VALUE enc)
{
    return INT2NUM(aux_frame_level(&getencoder(enc)->prefs));
}

static int
fenc_blocksize(struct encoder *p)
{
    return aux_frame_blocksize(&p->prefs.frameInfo);
}

static VALUE
fenc_prefs_blocksize(VALUE enc)
{
    return INT2NUM(fenc_blocksize(getencoder(enc)));
}

static VALUE
fenc_prefs_blocklink(VALUE enc)
{
    return aux_frame_blocklink(&getencoder(enc)->prefs.frameInfo) ? Qtrue : Qfalse;
}

static VALUE
fenc_prefs_checksum(VALUE enc)
{
    return aux_frame_checksum(&getencoder(enc)->prefs.frameInfo) ? Qtrue : Qfalse;
}

static VALUE
fenc_inspect(VALUE enc)
{
    struct encoder *p = getencoderp(enc);
    if (p) {
        return rb_sprintf("#<%s:%p outport=#<%s:%p>, level=%d, blocksize=%d, blocklink=%s, checksum=%s>",
                rb_obj_classname(enc), (void *)enc,
                rb_obj_classname(p->outport), (void *)p->outport,
                p->prefs.compressionLevel, fenc_blocksize(p),
                aux_frame_blocklink(&p->prefs.frameInfo) ? "true" : "false",
                aux_frame_checksum(&p->prefs.frameInfo) ? "true" : "false");
    } else {
        return rb_sprintf("#<%s:%p **INVALID REFERENCE**>",
                rb_obj_classname(enc), (void *)enc);
    }
}

/*** class LZ4::Decoder ***/

struct decoder
{
    VALUE inport;
    VALUE readbuf;   /* read buffer from inport */
    char *blockbuf; /* decoded lz4 frame block buffer */
    const char *blockend; /* end of blockbuf */
    char *blockhead;
    const char *blocktail;
    size_t status;   /* status code of LZ4F_decompress */
    LZ4F_frameInfo_t info;
    LZ4F_decompressionContext_t decoder;
};

static void
decoder_mark(void *pp)
{
    struct decoder *p = pp;
    rb_gc_mark(p->inport);
    rb_gc_mark(p->readbuf);
}

static void
decoder_free(void *pp)
{
    struct decoder *p = pp;
    if (p->decoder) {
        LZ4F_freeDecompressionContext(p->decoder);
    }
    if (p->blockbuf) {
        xfree(p->blockbuf);
    }
    memset(p, 0, sizeof(*p));
    xfree(p);
}

static const rb_data_type_t decoder_type = {
    .wrap_struct_name = "extlz4.LZ4.Decoder",
    .function.dmark = decoder_mark,
    .function.dfree = decoder_free,
    /* .function.dsize = decoder_size, */
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE
fdec_alloc(VALUE mod)
{
    struct decoder *p;
    VALUE obj = TypedData_Make_Struct(mod, struct decoder, &decoder_type, p);
    p->inport = Qnil;
    p->readbuf = Qnil;
    p->status = 0;
    return obj;
}

static struct decoder *
getdecoderp(VALUE dec)
{
    return getrefp(dec, &decoder_type);
}

static struct decoder *
getdecoder(VALUE dec)
{
    return getref(dec, &decoder_type);
}

static inline VALUE
aux_read(VALUE obj, size_t size, VALUE buf)
{
    if (NIL_P(buf)) { buf = rb_str_buf_new(size); }
    if (NIL_P(AUX_FUNCALL(obj, id_read, SIZET2NUM(size), buf))) {
        return Qnil;
    } else {
        if (RSTRING_LEN(buf) > size) {
            rb_raise(rb_eRuntimeError, "most read (%d, but expected to %d)", (int)RSTRING_LEN(buf), (int)size);
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
    struct decoder *p = getdecoder(dec);
    VALUE inport;
    //VALUE readblocksize;
    rb_scan_args(argc, argv, "1", &inport);
    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&p->decoder, LZ4F_VERSION);
    aux_lz4f_check_error(err);
    rb_obj_infect(dec, inport);
    p->inport = inport;
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
            rb_raise(extlz4_eError, "read error (or already EOF)");
        }
        s = LZ4F_decompress(p->decoder, NULL, &zero, readp, &readsize, NULL);
        aux_lz4f_check_error(s);
    }
    p->status = s;
    s = LZ4F_getFrameInfo(p->decoder, &p->info, NULL, &zero);
    aux_lz4f_check_error(s);

    size_t size = 1 << (p->info.blockSizeID * 2 + 8);
    p->blockbuf = ALLOC_N(char, size);
    p->blockend = p->blockbuf + size;

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

static size_t
fdec_read_fetch(VALUE dec, struct decoder *p)
{
    size_t blocksize = p->blocktail - p->blockhead;
    while (blocksize <= 0 && p->status != 0) {
        VALUE v = aux_read(p->inport, p->status, p->readbuf);
        char *readp;
        size_t readsize;
        aux_str_getmem(v, &readp, &readsize);
        if (!readp) {
            rb_raise(extlz4_eError,
                    "read error - encounted invalid EOF - #<%s:%p>",
                    rb_obj_classname(p->inport), (void *)p->inport);
        }

        rb_obj_infect(dec, v);
        blocksize = p->blockend - p->blockbuf;
        p->status = LZ4F_decompress(p->decoder, p->blockbuf, &blocksize, readp, &readsize, NULL);
        aux_lz4f_check_error(p->status);
        p->blockhead = p->blockbuf;
        p->blocktail = p->blockhead + blocksize;
    }

    return blocksize;
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
    struct decoder *p = getdecoder(dec);
    size_t size;
    VALUE dest;
    fdec_read_args(argc, argv, &size, &dest);
    if (size == 0) {
        rb_obj_infect(dest, dec);
        return dest;
    }

    if (p->status == 0) {
        return Qnil;
    }

    do {
        size_t blocksize = fdec_read_fetch(dec, p);
        rb_obj_infect(dest, dec);

        if (size < blocksize) {
            rb_str_buf_cat(dest, p->blockhead, size);
            p->blockhead += size;
            break;
        } else {
            rb_str_buf_cat(dest, p->blockhead, blocksize);
            p->blocktail = p->blockhead = NULL;
            size -= blocksize;
        }
    } while (size > 0 && p->status != 0);

    return dest;
}

/*
 * call-seq:
 *  getc -> String | nil
 *
 * Read one byte character.
 */
static VALUE
fdec_getc(VALUE dec)
{
    struct decoder *p = getdecoder(dec);

    for (;;) {
        if (p->status == 0) {
            return Qnil;
        }
        size_t blocksize = fdec_read_fetch(dec, p);
        if (blocksize != 0) {
            char ch = (uint8_t)*p->blockhead;
            p->blockhead ++;
            return rb_str_new(&ch, 1);
        }
    }
}

/*
 * call-seq:
 *  getbyte -> Integer | nil
 *
 * Read one byte code integer.
 */
static VALUE
fdec_getbyte(VALUE dec)
{
    struct decoder *p = getdecoder(dec);

    for (;;) {
        if (p->status == 0) {
            return Qnil;
        }
        size_t blocksize = fdec_read_fetch(dec, p);
        if (blocksize != 0) {
            int ch = (uint8_t)*p->blockhead;
            p->blockhead ++;
            return INT2FIX(ch);
        }
    }
}

static VALUE
fdec_close(VALUE dec)
{
    struct decoder *p = getdecoder(dec);
    p->status = 0;
    // TODO: destroy decoder
    return dec;
}

static VALUE
fdec_eof(VALUE dec)
{
    struct decoder *p = getdecoder(dec);
    if (p->status == 0) {
        return Qtrue;
    } else {
        return Qfalse;
    }
}

static VALUE
fdec_inport(VALUE dec)
{
    return getdecoder(dec)->inport;
}

static int
fdec_blocksize(struct decoder *p)
{
    return aux_frame_blocksize(&p->info);
}

static VALUE
fdec_prefs_blocksize(VALUE dec)
{
    return INT2NUM(fdec_blocksize(getdecoder(dec)));
}

static VALUE
fdec_prefs_blocklink(VALUE dec)
{
    return aux_frame_blocklink(&getdecoder(dec)->info) ? Qtrue : Qfalse;
}

static VALUE
fdec_prefs_checksum(VALUE dec)
{
    return aux_frame_checksum(&getdecoder(dec)->info) ? Qtrue : Qfalse;
}

static VALUE
fdec_inspect(VALUE dec)
{
    struct decoder *p = getdecoderp(dec);
    if (p) {
        return rb_sprintf("#<%s:%p inport=#<%s:%p>, blocksize=%d, blocklink=%s, checksum=%s>",
                rb_obj_classname(dec), (void *)dec,
                rb_obj_classname(p->inport), (void *)p->inport,
                fdec_blocksize(p),
                aux_frame_blocklink(&p->info) ? "true" : "false",
                aux_frame_checksum(&p->info) ? "true" : "false");
    } else {
        return rb_sprintf("#<%s:%p **INVALID REFERENCE**>",
                rb_obj_classname(dec), (void *)dec);
    }
}

/*** setup for LZ4::Encoder and LZ4::Decoder ***/

void
extlz4_init_frameapi(void)
{
    id_op_lshift = rb_intern("<<");
    id_read = rb_intern("read");

    VALUE cEncoder = rb_define_class_under(extlz4_mLZ4, "Encoder", rb_cObject);
    rb_define_alloc_func(cEncoder, fenc_alloc);
    rb_define_method(cEncoder, "initialize", RUBY_METHOD_FUNC(fenc_init), -1);
    rb_define_method(cEncoder, "write", RUBY_METHOD_FUNC(fenc_write), -1);
    rb_define_method(cEncoder, "<<", RUBY_METHOD_FUNC(fenc_push), 1);
    rb_define_method(cEncoder, "flush", RUBY_METHOD_FUNC(fenc_flush), 0);
    rb_define_method(cEncoder, "close", RUBY_METHOD_FUNC(fenc_close), 0);
    rb_define_alias(cEncoder, "finish", "close");
    rb_define_method(cEncoder, "outport", RUBY_METHOD_FUNC(fenc_getoutport), 0);
    rb_define_method(cEncoder, "outport=", RUBY_METHOD_FUNC(fenc_setoutport), 1);
    rb_define_method(cEncoder, "prefs_level", RUBY_METHOD_FUNC(fenc_prefs_level), 0);
    rb_define_method(cEncoder, "prefs_blocksize", RUBY_METHOD_FUNC(fenc_prefs_blocksize), 0);
    rb_define_method(cEncoder, "prefs_blocklink", RUBY_METHOD_FUNC(fenc_prefs_blocklink), 0);
    rb_define_method(cEncoder, "prefs_checksum", RUBY_METHOD_FUNC(fenc_prefs_checksum), 0);
    rb_define_method(cEncoder, "inspect", RUBY_METHOD_FUNC(fenc_inspect), 0);

    VALUE cDecoder = rb_define_class_under(extlz4_mLZ4, "Decoder", rb_cObject);
    rb_define_alloc_func(cDecoder, fdec_alloc);
    rb_define_method(cDecoder, "initialize", RUBY_METHOD_FUNC(fdec_init), -1);
    rb_define_method(cDecoder, "read", RUBY_METHOD_FUNC(fdec_read), -1);
    rb_define_method(cDecoder, "getc", RUBY_METHOD_FUNC(fdec_getc), 0);
    rb_define_method(cDecoder, "getbyte", RUBY_METHOD_FUNC(fdec_getbyte), 0);
    rb_define_method(cDecoder, "close", RUBY_METHOD_FUNC(fdec_close), 0);
    rb_define_alias(cDecoder, "finish", "close");
    rb_define_method(cDecoder, "eof", RUBY_METHOD_FUNC(fdec_eof), 0);
    rb_define_method(cDecoder, "inport", RUBY_METHOD_FUNC(fdec_inport), 0);
    rb_define_alias(cDecoder, "eof?", "eof");
    rb_define_method(cDecoder, "prefs_blocksize", RUBY_METHOD_FUNC(fdec_prefs_blocksize), 0);
    rb_define_method(cDecoder, "prefs_blocklink", RUBY_METHOD_FUNC(fdec_prefs_blocklink), 0);
    rb_define_method(cDecoder, "prefs_checksum", RUBY_METHOD_FUNC(fdec_prefs_checksum), 0);
    rb_define_method(cDecoder, "inspect", RUBY_METHOD_FUNC(fdec_inspect), 0);
}
