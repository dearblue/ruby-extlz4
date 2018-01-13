
# extlz4 - LZ4 for ruby

圧縮伸張ライブラリ [lz4 (https://github.com/lz4/lz4/)](https://github.com/lz4/lz4/) の非公式 ruby バインディングライブラリです。

LZ4 データストリームを圧縮・伸張できます。lz4-cli で扱うことが出来ます。

``` shell:shell
$ dmesg | ruby -r extlz4 -e 'LZ4.encode_file($stdin.binmode, $stdout.binmode)' | lz4c -d | more
```

ほかの ruby 向けの lz4 バインディングライブラリとしては KOMIYA Atsushi さんによる [lz4-ruby (http://rubygems.org/gems/lz4-ruby)](http://rubygems.org/gems/lz4-ruby) があります。


## SUMMARY (概要)

  * package name: extlz4
  * author: dearblue (mailto:dearblue@users.noreply.github.com)
  * report issue to: <https://github.com/dearblue/ruby-extlz4/issues>
  * how to install: `gem install extlz4`
  * version: 0.2.4.3
  * product quality: technical preview
  * licensing: BSD-2-Clause License
  * dependency gems: none
  * dependency external c libraries: none
  * bundled external c libraries:
      * lz4-1.8 <https://github.com/lz4/lz4/tree/v1.8.0>
        under [BSD 2-Clause license](https://github.com/lz4/lz4/tree/v1.8.0/LICENSE)
        by [Yann Collet](https://github.com/Cyan4973)


## ATTENTIONS (注意事項)

  * Many documents are written in japanese.

    (ドキュメントの多くは日本語で記述されています)


## extlz4-0.1 による不正な lz4 ストリームを出力する不具合について

extlz4-0.1 に不正な lz4 ストリームを出力する不具合がありました。

詳しく説明すると、ブロックデータとともに格納時ブロックデータサイズも出力しますが、常に圧縮後のデータサイズを出力していました。圧縮していない (圧縮後のデータサイズが圧縮前のデータサイズを上回った) 場合であれば、圧縮前のデータサイズを出力するべきところですが、そうはなっていません。

これによって無圧縮ブロックデータが正しく読み込めず、ひいてはそのブロック以降のデータが正しく読み込めないということになります。

extlz4-0.1.1 でその不具合の修正を行いました。

また、不正な lz4 ストリームファイルを生成しなおかつオリジナルファイルも失われた場合は、同梱してある `bin/extlz4` プログラムに `--fix-extlz4-0.1-bug` を指定することで正しい lz4 ストリームファイルを出力できます。

    extlz4 --fix-extlz4-0.1-bug <file>...

出力ファイル名は、入力ファイル名の頭に "fixed-" を追加したものとなります。

必要であれば `-f` スイッチが利用できます。

`-k` スイッチは無視されます。修復した lz4 ストリームファイルが正しいかを検査したあとで不正な lz4 ストリームファイルと差し替えて下さい。

修復できるのはあくまで extlz4-0.1 のこの不具合に起因するファイルのみとなります。


## FEATURES (機能)

  * Generic LZ4 frame data process
      * Decode LZ4 Frame data : LZ4.decode
      * Encode LZ4 Frame data : LZ4.encode
  * Generic LZ4 frame data file process
      * Decode LZ4 Frame data file : LZ4.decode\_file
      * Encode LZ4 Frame data file : LZ4.encode\_file
  * LZ4 block data process
      * Decode LZ4 block data : LZ4.block\_decode
      * Encode LZ4 block data : LZ4.block\_encode (supporting high compression level)
      * Streaming Decode LZ4 block data : LZ4.block\_stream\_decode and LZ4::BlockDecoder#update
      * Streaming Encode LZ4 block data : LZ4.block\_stream\_encode and LZ4::BlockEncoder#update (supporting high compression level)


## ABOUT TAINT STATE AND SECURITY (汚染状態とセキュリティについて)

extlz4 はオブジェクト間での汚染状態を一方向伝播します。

オブジェクトの汚染伝播については『入力 -> 出力』となり、
ストリーム処理の場合は『入力 -> 圧縮器・伸張器 -> 出力』というようになります。

セキュリティレベルによる処理の拒否は行いません。


## EXAMPLES (用例)

First, load extlz4. (最初に extlz4 を読み込んでください)

``` ruby:ruby
require "extlz4"
```

### Decoding (伸張処理)

``` ruby:ruby
uncompressed_data_string = LZ4.decode(compressed_data_string)
```

### Encoding (通常圧縮処理)

``` ruby:ruby
compressed_data_string = LZ4.encode(uncompressed_data_string)
```

### High compression encoding (高圧縮処理)

``` ruby:ruby
compressed_data_string = LZ4.encode(uncompressed_data_string, 9)
```

### Frame decoding

``` ruby:ruby
File.open("sample.txt.lz4", "rb") do |file|
  LZ4.decode(file) do |lz4|
    lz4.read(50)  # read 50 bytes as string
    lz4.getc      # read 1 byte as integer
    lz4.read      # read rest bytes as string
  end
end
```

### Frame encoding by high compression

``` ruby:ruby
File.open("sample.txt.lz4", "wb") do |file|
  LZ4.encode(file, 9) do |lz4|
    lz4 << "#{Time.now}: abcdefghijklmnopqrstuvwxyz\n"
    lz4.write "#{Time.now}: abcdefghijklmnopqrstuvwxyz\n"
  end
end
```

### Frame encoding without block

``` ruby:ruby
file = File.open("sample.txt.lz4", "wb")
lz4 = LZ4.encode(file)
lz4 << "abcdefghijklmnopqrstuvwxyz\n"
lz4.close  # VERY IMPORTANT!
```

### Block data processing (fast compression encoding and decoding)

``` ruby:ruby
src = "abcdefg" * 100
lz4data = LZ4.block_encode(src)
data = LZ4.block_decode(lz4data)
p src == data  # => true
```

### Block data processing (high compression encoding and decoding)

``` ruby:ruby
src = "abcdefg" * 100
level = 8
lz4data = LZ4.block_encode(level, src)
data = LZ4.block_decode(lz4data)
p src == data  # => true
```

### Block data processing (high speed encoding)

``` ruby:ruby
src = "abcdefg" * 100
level = -19 # transform to one's complement as acceleration
lz4data = LZ4.block_encode(level, src)
```

### Block stream data processing (high compression encoding and decoding)

``` ruby:ruby
level = 8 # (OPTIONAL PARAMETER)
predict = "abcdefg" # with preset dictionary (OPTIONAL PARAMETER)
encoder = LZ4.block_stream_encode(level, predict)

src = "abcdefg" * 100
lz4data1 = encoder.update(src)

decoder = LZ4.block_stream_decode(predict)

data = decoder.update(lz4data1)
p src == data  # => true

src2 = "ABCDEFG" * 100
lz4data2 = encoder.update(src2)
p "lz4data1.bytesize" => lz4data1.bytesize,
  "lz4data2.bytesize" => lz4data2.bytesize

data = decoder.update(lz4data2)
p src2 == data  # => true
```


## BONUS (おまけ)

コマンドラインプログラムとして ``extlz4`` が追加されます。

これは lz4 と同程度の機能を持ちます (車輪の再発明とも言う)。

とはいえ、引数のとり方を変えてあり、gzip のような形で利用できます。
