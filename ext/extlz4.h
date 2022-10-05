#ifndef EXTLZ4_H
#define EXTLZ4_H 1

#include <ruby.h>
#include <ruby/thread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#ifndef RB_OBJ_FROZEN
#   define RB_OBJ_FROZEN    OBJ_FROZEN
#endif

extern VALUE extlz4_mLZ4;      /* module LZ4 */
extern VALUE extlz4_eError;    /* class LZ4::Error < ::RuntimeError */

extern void extlz4_init_blockapi(void);
extern void extlz4_init_frameapi(void);

#ifndef RB_EXT_RACTOR_SAFE
# define RB_EXT_RACTOR_SAFE(FEATURE) ((void)(FEATURE))
#endif

#define AUX_FUNCALL(RECV, METHOD, ...)                          \
    ({                                                          \
        VALUE args__[] = { __VA_ARGS__ };                       \
        rb_funcall2((RECV), (METHOD),                           \
                sizeof(args__) / sizeof(args__[0]), args__);    \
    })                                                          \

static inline void *
aux_thread_call_without_gvl(void *(*func)(va_list *), void (*cancel)(va_list *), ...)
{
    va_list va1, va2;
    va_start(va1, cancel);
    va_start(va2, cancel);
    void *s = rb_thread_call_without_gvl((void *(*)(void *))func, &va1, (void (*)(void *))cancel, &va2);
    va_end(va1);
    va_end(va2);
    return s;
}

static inline void
aux_str_reserve(VALUE str, size_t size)
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
/*
 * rb_str_drop_bytes は共有オブジェクトを生成して返すので、
 * それとは違う、破壊的メソッドとしての関数。
 */
static inline VALUE
aux_str_drop_bytes(VALUE str, size_t dropsize)
{
    char *p;
    size_t size;
    RSTRING_GETMEM(str, p, size);
    if (dropsize > size) {
        dropsize = size;
    } else {
        memmove(p, p + dropsize, size - dropsize);
    }
    rb_str_set_len(str, size - dropsize);
    return str;
}

static inline char *
aux_str_getmem(VALUE str, char **ptr, size_t *size)
{
    if (rb_type_p(str, RUBY_T_STRING)) {
        RSTRING_GETMEM(str, *ptr, *size);
    } else {
        *ptr = NULL;
        *size = 0;
    }

    return *ptr;
}

static inline void *
checkref(VALUE obj, void *p)
{
    if (!p) {
        rb_raise(rb_eRuntimeError,
                "not initialized object or invalid reference - #<%s:%p>",
                rb_obj_classname(obj), (void *)obj);
    }
    return p;
}

static inline void *
getrefp(VALUE obj, const rb_data_type_t *type)
{
    void *p;
    TypedData_Get_Struct(obj, void, type, p);
    return p;
}

static inline void *
getref(VALUE obj, const rb_data_type_t *type)
{
    return checkref(obj, getrefp(obj, type));
}

static inline int
aux_size2int(size_t n)
{
    int m = (int)n;

    if (m < 0 || (size_t)m != n) {
        rb_raise(rb_eRangeError, "out of range integer conversion (size_t to int)");
    }

    return m;
}

#endif /* !EXTLZ4_H */
