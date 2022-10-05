/*
 *
 * This code is under public domain (CC0)
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 *
 * To the extent possible under law, dearblue has waived all copyright
 * and related or neighboring rights to this work.
 *
 *     dearblue <dearblue@users.osdn.me>
 */

#include "hashargs.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define aux_noreturn _Noreturn
#else
# define aux_noreturn
#endif

struct rbx_scanhash_args
{
    struct rbx_scanhash_arg *args;
    const struct rbx_scanhash_arg *end;
    VALUE rest;
};

aux_noreturn static void
rbx_scanhash_error(ID given, struct rbx_scanhash_arg *args, const struct rbx_scanhash_arg *end)
{
    // 引数の数が㌧でもない数の場合、よくないことが起きそう。

    VALUE names = rb_ary_new();
    for (; args < end; args ++) {
        rb_ary_push(names, ID2SYM(args->name));
    }

    size_t namenum = RARRAY_LEN(names);
    if (namenum > 2) {
        VALUE w = rb_ary_pop(names);
        names = rb_ary_join(names, rb_str_new_cstr(", "));
        names = rb_ary_new4(1, &names);
        rb_ary_push(names, w);
        names = rb_ary_join(names, rb_str_new_cstr(" or "));
    } else if (namenum > 1) {
        names = rb_ary_join(names, rb_str_new_cstr(" or "));
    }

    {
        VALUE key = rb_sym_to_s(ID2SYM(given));
        rb_raise(rb_eArgError,
                "unknown keyword (%s for %s)",
                StringValueCStr(key), StringValueCStr(names));
    }
}

static inline ID
rbx_scanhash_intern(VALUE key)
{
    if (RB_TYPE_P(key, RUBY_T_SYMBOL)) {
        return SYM2ID(key);
    } else {
        key = rb_String(key);
        return rb_intern_str(key);
    }
}

static int
rbx_scanhash_foreach(VALUE key, VALUE value, struct rbx_scanhash_args *args)
{
    struct rbx_scanhash_arg *p = args->args;
    const struct rbx_scanhash_arg *end = args->end;
    ID keyid = rbx_scanhash_intern(key);

    for (; p < end; p ++) {
        if (p->name == keyid) {
            if (p->dest) {
                *p->dest = value;
            }
            return 0;
        }
    }

    if (RTEST(args->rest)) {
        rb_hash_aset(args->rest, key, value);
    } else {
        rbx_scanhash_error(keyid, args->args, args->end);
    }

    return 0;
}

static VALUE
rbx_scanhash_to_hash(VALUE hash)
{
    if (NIL_P(hash)) { return Qnil; }

    static ID id_to_hash;
    if (!id_to_hash) { id_to_hash = rb_intern_const("to_hash"); }
    VALUE hash1 = rb_funcall2(hash, id_to_hash, 0, 0);
    if (TYPE(hash1) != T_HASH) {
        rb_raise(rb_eTypeError,
                "converted object is not a hash (<#%s:%p>)",
                rb_obj_classname(hash), (void *)hash);
    }
    return hash1;
}

static inline void
rbx_scanhash_setdefaults(struct rbx_scanhash_arg *args, struct rbx_scanhash_arg *end)
{
    for (; args < end; args ++) {
        if (args->dest) {
            *args->dest = args->initval;
        }
    }
}


static inline void
rbx_scanhash_check_missingkeys(struct rbx_scanhash_arg *args, struct rbx_scanhash_arg *end)
{
    for (; args < end; args ++) {
        if (args->dest && *args->dest == Qundef) {
            VALUE key = rb_sym_to_s(ID2SYM(args->name));
            rb_raise(rb_eArgError,
                    "missing keyword: `%s'",
                    StringValueCStr(key));
        }
    }
}

VALUE
rbx_scanhash(VALUE hash, VALUE rest, struct rbx_scanhash_arg *args, struct rbx_scanhash_arg *end)
{
    if (RTEST(rest)) {
        if (rest == Qtrue) {
            rest = rb_hash_new();
        } else if (!rb_obj_is_kind_of(rest, rb_cHash)) {
            rb_raise(rb_eArgError,
                    "`rest' is not a hash");
        }
    } else {
        rest = Qnil;
    }

    rbx_scanhash_setdefaults(args, end);

    hash = rbx_scanhash_to_hash(hash);
    if (!NIL_P(hash) && !RHASH_EMPTY_P(hash)) {
        struct rbx_scanhash_args argset = { args, end, rest };
        rb_hash_foreach(hash, (int (*)(VALUE, VALUE, VALUE))rbx_scanhash_foreach, (VALUE)&argset);
    }

    rbx_scanhash_check_missingkeys(args, end);

    return rest;
}
