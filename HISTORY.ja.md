
# extlz4-0.2 (2015-03-26)

## ファイル構成の変更

* ext/: ext/extlz4.c を複数のファイルに分割しました。

## LZ4 ストリームの独自実装から LZ4 Frame API への移行

* lib/extlz4.rb: LZ4.encode、LZ4.decode の引数は互換性を失いました。
* lib/extlz4/oldstream.rb: 独自実装版は LZ4::StreamEncoder、LZ4::StreamDecoder のまま残されました。
    これ以上保守されませんし、将来的にこのクラスは廃止されます。
    利用する場合は ``require "extlz4/oldstream"`` とする必要があります。

## LZ4 streaming API に対する更新

* ext/: ``LZ4_create()`` 系から ``LZ4_createStream()`` 系の API に移行しました。
* ext/: ``LZ4_decompress_safe_withPrefix64k()`` から ``LZ4_createStreamDecode()`` 系の API に移行しました。
* ext/: ``LZ4::RawStreamEncoder`` が ``LZ4::RawEncoder`` になりました。
* ext/: ``LZ4::RawStreamDecoder`` が ``LZ4::RawDecoder`` になりました。


# extlz4-0.1.1 (2014-06-01)

## 不具合の修正

* lib/extlz4.rb: ストリーム圧縮時に、未圧縮ブロックの格納サイズが常に圧縮した時のサイズを格納してしまい、不正な lz4 ストリームを生成していましたが、これを修正しました。

    extlz4-0.1 で作成した lz4 ストリームファイルは圧縮前のファイルを削除する前に検査して、整合性を確認して下さい。

    また、これ以降 extlz4-0.1 の利用はしないで下さい。

    * `bin/extlz4 --fix-extlz4-0.1-bug <filename>` で不正な lz4 ストリームファイルを修正できます。修正したファイル名は "fixed-" + &lt;指定したファイル名&gt; の形となります。

* lib/extlz4.rb: ブロック依存ストリーム生成の場合、高効率圧縮時に圧縮レベルが常に規定値になっていましたが、これを変動するように修正しました。

* lib/extlz4.rb (`LZ4::StreamEncoder#initialize`): `raw_encode` / `RawStreamEncoder#update` に渡す `level` の値が [nil, 0 .. 16] になるように修正しました。

* bin/extlz4: lz4 ストリーム検査の時、標準入力を利用した場合でも『-f』スイッチが必要となっていましたが、これを不要とするように修正しました。

## メモリ使用量の改善

* lib/extlz4.rb: File#read に常に同じ文字列オブジェクトを渡し、さらに String#slice 系のメソッドを StringIO#read へ置き換えることで、常に同じ文字列オブジェクトを再利用するように修正しました。

    これによって ruby-2.1 系で数10GBを大きく超えるデータを扱う場合でも、利用するメモリ量の低減が期待できます。


# extlz4-0.1 (2014-04-29)

## 初犯
