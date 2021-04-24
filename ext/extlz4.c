#include "extlz4.h"
#include <lz4.h>

VALUE extlz4_eError;

/*
 * version information
 */

static VALUE
libver_major(VALUE ver)
{
    return INT2FIX(LZ4_VERSION_MAJOR);
}

static VALUE
libver_minor(VALUE ver)
{
    return INT2FIX(LZ4_VERSION_MINOR);
}

static VALUE
libver_release(VALUE ver)
{
    return INT2FIX(LZ4_VERSION_RELEASE);
}

static VALUE
libver_to_s(VALUE ver)
{
    return rb_sprintf("%d.%d.%d", LZ4_VERSION_MAJOR, LZ4_VERSION_MINOR, LZ4_VERSION_RELEASE);
}

/*
 * initialize library
 */

VALUE extlz4_mLZ4;

RBEXT_API void
Init_extlz4(void)
{
    RB_EXT_RACTOR_SAFE(true);

    extlz4_mLZ4 = rb_define_module("LZ4");

    /*
     * Document-const: LZ4::LIBVERSION
     *
     * This constant value means api version number of original lz4 library as array'd integers.
     *
     * And it's has any singleton methods, so they are +#major+, +#minor+, +#release+ and +#to_s+.
     *
     * 実体が配列である理由は、比較を行いやすくすることを意図しているためです。
     */
    VALUE ver = rb_ary_new3(3,
            INT2FIX(LZ4_VERSION_MAJOR),
            INT2FIX(LZ4_VERSION_MINOR),
            INT2FIX(LZ4_VERSION_RELEASE));
    rb_define_singleton_method(ver, "major", libver_major, 0);
    rb_define_singleton_method(ver, "minor", libver_minor, 0);
    rb_define_singleton_method(ver, "release", libver_release, 0);
    rb_define_singleton_method(ver, "to_s", libver_to_s, 0);
    rb_obj_freeze(ver);
    rb_define_const(extlz4_mLZ4, "LIBVERSION", ver);

    extlz4_eError = rb_define_class_under(extlz4_mLZ4, "Error", rb_eRuntimeError);

    extlz4_init_blockapi();
    extlz4_init_frameapi();
}
