# extlz4-0.2.4 (平成30年1月7日 日曜日)

  * Fix compilation error under Windows 10 (#1) (thanks to @pyjarrett)
  * Upgrade lz4 to 1.8.0


# extlz4-0.2.3 (平成29年4月16日 日曜日)

  * ``LZ4::BlockEncoder#update`` と ``LZ4::BlockDecoder#update`` を nogvl 化


# extlz4-0.2.2 (平成29年4月15日 土曜日)

  * ``LZ4::FrameEncoder.new`` のフレームブロックサイズに対する既定値を
    ``LZ4F_max4MB`` から ``LZ4F_default`` へと修正

  * LZ4HC に関する定数を追加

  * 外部シンボルの可視性を抑制するように修正

    * コンパイラが "-fvisibility=hidden" を利用できる (gcc や clang などの)
      場合、extlz4.so が持つ外部シンボルを必要最低限度となるように修正しました。

  * **(BUGFIX)** ``LZ4::BlockEncoder``、``LZ4::BlockDecoder`` が SEGV
    を起こしていた問題の修正

  * **(BUGFIX)** ``LZ4::BlockEncoder``、``LZ4::BlockDecoder``、
    ``LZ4::FrameEncoder``、``LZ4::FrameDecoder`` がメモリリークを起こしていた問題の修正


# extlz4-0.2.1 (平成27年10月16日 金曜日)

  * lz4 ライブラリを r131 (https://github.com/Cyan4973/lz4/tree/r131) に更新
  * LZ4\_compress\_fast に対応
  * LZ4::BlockEncoder.compressbound の不具合修正

    引数として与えられた整数値を ruby レベルから C レベルに変換し、その値を
    ruby レベルの文字列オブジェクト (RString) として扱っていました。
    この不具合を修正しています。


# extlz4-0.2 (2015-04-19)

## いくつかの名称の変更

  * ストリームをフレーム (frame) に変更しました。
  * これまで extlz4 において raw\*\*\* と呼んできた名称を block\*\*\* に変更しました。

## LZ4 ストリームの独自実装から LZ4 Frame API への移行

  * LZ4.encode、LZ4.decode の引数は互換性を失いました。
  * LZ4 Frame API による圧縮・伸長処理を行うためのクラスは
    LZ4::Encoder と LZ4::Decoder として利用できます。
  * 独自実装版は LZ4::StreamEncoder、LZ4::StreamDecoder のまま残されました。
      * ***これらのクラスは将来的に廃止される予定です。***
      * 利用する場合は ``require "extlz4/oldstream"`` とする必要があります。
      * ruby gems の xxhash-0.3 を必要とします。
      * 以前の LZ4.encode は LZ4.encode\_old、LZ4.decode は LZ4.decode\_old
        として利用できます。

## 新しい LZ4 Block Streaming API への移行

  * ``LZ4_create()`` 系から ``LZ4_createStream()`` 系の API に移行しました。
  * ``LZ4_decompress_safe_withPrefix64k()`` から ``LZ4_createStreamDecode()`` 系の API に移行しました。
  * LZ4::RawStreamEncoder が LZ4::BlockEncoder になりました。
  * LZ4::RawStreamDecoder が LZ4::BlockDecoder になりました。

## セーフレベルの確認処理を削除

  * セーフレベルの確認処理を削除しました。

    今まではセーフレベルが4以上の場合に汚染状態を移す必要のある場合は、
    SecurityError 例外を発生させていましたが、この方針を変更して常に
    汚染状態を伝搬させるだけの処理にしました。


# extlz4-0.1.1 (2014-06-01)

## 不具合の修正

* lib/extlz4.rb: ストリーム圧縮時に、未圧縮ブロックの格納サイズが常に圧縮した時のサイズを格納してしまい、不正な lz4 ストリームを生成していましたが、これを修正しました。

    extlz4-0.1 で作成した lz4 ストリームファイルは圧縮前のファイルを削除する前に検査して、整合性を確認して下さい。

    また、これ以降 extlz4-0.1 の利用はしないで下さい。

    * `bin/extlz4 --fix-extlz4-0.1-bug <filename>` で不正な lz4 ストリームファイルを修正できます。修正したファイル名は "fixed-" + &lt;指定したファイル名&gt; の形となります。

* lib/extlz4.rb: ブロック依存ストリーム生成の場合、高効率圧縮時に圧縮レベルが常に規定値になっていましたが、これを変動するように修正しました。

* lib/extlz4.rb (`LZ4::StreamEncoder#initialize`): `block_encode` / `RawStreamEncoder#update` に渡す `level` の値が [nil, 0 .. 16] になるように修正しました。

* bin/extlz4: lz4 ストリーム検査の時、標準入力を利用した場合でも『-f』スイッチが必要となっていましたが、これを不要とするように修正しました。

## メモリ使用量の改善

* lib/extlz4.rb: File#read に常に同じ文字列オブジェクトを渡し、さらに String#slice 系のメソッドを StringIO#read へ置き換えることで、常に同じ文字列オブジェクトを再利用するように修正しました。

    これによって ruby-2.1 系で数10GBを大きく超えるデータを扱う場合でも、利用するメモリ量の低減が期待できます。


# extlz4-0.1 (2014-04-29)

## 初犯
