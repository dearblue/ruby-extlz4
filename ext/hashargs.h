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

#ifndef RBX_HASHARGS_H
#define RBX_HASHARGS_H 1

#include <ruby.h>
#include <ruby/intern.h>

#define RBX_SCANHASH_ELEMENTOF(v) (sizeof((v)) / sizeof((v)[0]))
#define RBX_SCANHASH_ENDOF(v) ((v) + RBX_SCANHASH_ELEMENTOF(v))

#if defined(__cplusplus__)
#   define RBX_SCANHASH_CEXTERN         extern "C"
#   define RBX_SCANHASH_CEXTERN_BEGIN   RBX_SCANHASH_CEXTERN {
#   define RBX_SCANHASH_CEXTERN_END     }
#else
#   define RBX_SCANHASH_CEXTERN
#   define RBX_SCANHASH_CEXTERN_BEGIN
#   define RBX_SCANHASH_CEXTERN_END
#endif

RBX_SCANHASH_CEXTERN_BEGIN

struct rbx_scanhash_arg
{
    ID name;
    VALUE *dest;
    VALUE initval;
};

/*
 * RBX_SCANHASH マクロから呼ばれる
 */
VALUE rbx_scanhash(VALUE hash, VALUE rest, struct rbx_scanhash_arg *args, struct rbx_scanhash_arg *end);

/**
 * メソッドが受け取るキーワード引数を解析します。
 *
 * マクロ引数はそれぞれが一度だけ評価されます (多重評価はされません)。
 *
 * 可変長部分の引数 (第3引数以降) の評価順は全ての環境で左から右に固定されます。
 *
 * 第1引数と第2引数の評価順は環境依存となります。
 *
 * @param hash
 *      解析対象のハッシュオブジェクト。nil の場合、受け取り変数を初期化するだけです。
 * @param rest
 *      解析後に残ったキーワードを受け取るハッシュオブジェクトを指定します。
 *      true を指定した場合、内部で新規ハッシュオブジェクトを用意します。
 *      NULL / false / nil の場合、任意キーワードの受け取りを認めません。
 * @param ...
 *      RBX_SCANHASH_ARG[SI] が並びます。終端を表すものの記述は不要です。
 * @return
 *      受け取り対象外のハッシュオブジェクト (rest で与えたもの) が返ります。
 *
 * @sample
 *
 *  // 走査するハッシュオブジェクト
 *  VALUE user_hash_object = rb_hash_new();
 *
 *  VALUE a, b, c, d, e, f; // これらの変数に受け取る
 *  RBX_SCANHASH(user_hash_object, Qnil,
 *          RBX_SCANHASH_ARGS("a", &a, Qnil),
 *          RBX_SCANHASH_ARGS("b", &b, Qtrue),
 *          RBX_SCANHASH_ARGS("c", &c, rb_str_new_cstr("abcdefg")),
 *          RBX_SCANHASH_ARGS("d", &d, INT2FIX(5)));
 *
 * RBX_SCANHASH_ARG 系の第2引数に NULL を与えると、名前の確認だけして、Cレベルの変数への代入は行わない。
 *          RBX_SCANHASH_ARGS("e", NULL, Qnil)
 *
 * RBX_SCANHASH_ARG 系の第3引数に Qundef を与えると、省略不可キーワード引数となる
 *          RBX_SCANHASH_ARGS("f", &f, Qundef)
 */
#define RBX_SCANHASH(hash, rest, ...)                                  \
    ({                                                                 \
        struct rbx_scanhash_arg RBX_SCANHASH_argv[] = { __VA_ARGS__ }; \
        rbx_scanhash((hash), (rest), RBX_SCANHASH_argv,                \
                RBX_SCANHASH_ENDOF(RBX_SCANHASH_argv));                \
    })                                                                 \

/*
 * 評価順は左から右に固定される。
 *
 * [name]   C の文字列を与える
 * [dest]   キーワード引数の代入先。NULL を指定した場合、名前の確認だけして、Cレベルの変数への代入は行わない
 * [vdefault] 規定値。Qundef を指定した場合、省略不可キーワードとなる
 */
#define RBX_SCANHASH_ARGS(name, dest, vdefault) { rb_intern((name)), (dest), (vdefault), }

/*
 * 評価順は左から右に固定される。
 *
 * [name]   ruby の ID をあたえる。
 * [dest]   キーワード引数の代入先。NULL を指定した場合、名前の確認だけして、Cレベルの変数への代入は行わない
 * [vdefault] 規定値。Qundef を指定した場合、省略不可キーワードとなる
 */
#define RBX_SCANHASH_ARGI(name, dest, vdefault) { (name), (dest), (vdefault), }

RBX_SCANHASH_CEXTERN_END

#endif /* !defined(RBX_HASHARGS_H) */
