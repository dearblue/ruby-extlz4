 # encoding:utf-8 ;

# extlz4 - LZ4 for ruby

圧縮伸張ライブラリ [lz4 (http://code.google.com/p/lz4/)](http://code.google.com/p/lz4/) の ruby バインディングライブラリです。

LZ4 データストリームを圧縮・伸張できます。lz4-cli で扱うことが出来ます。

    $ dmesg | ruby -r extlz4 -e 'LZ4.encode_file($stdin.binmode, $stdout.binmode)' | lz4c -d | more

ほかの ruby 向けの lz4 バインディングライブラリとしては KOMIYA Atsushi さんによる [lz4-ruby (http://rubygems.org/gems/lz4-ruby)](http://rubygems.org/gems/lz4-ruby) があります。


## Summary (概要)

- Package name (名称): extlz4
- Author (制作者): dearblue <dearblue@users.sourceforge.jp>
- How to install (インストール手順): `gem install extlz4`
- Version (バージョン情報): 0.1
- Release quality (品質): prototype
- Licensing (ライセンス): 2-clause BSD License (二条項 BSD ライセンス)
- Dependency gems (依存する gem パッケージ):
    - xxhash - `gem install xxhash` : This is not has automation install.
- Dependency external C libraries (依存する外部 C ライブラリ):
    - NONE (なし) (lz4 はパッケージに同梱)
- Bundled external C libraries (同梱される外部 C ライブラリ):
    - lz4 (Yann Collet さんによる) <http://code.google.com/p/lz4/> (r117)
- Report issue to (問題の報告先): <http://sourceforge.jp/projects/rutsubo/ticket/>


## Attentions (注意事項)

- Library quality is yet experimentally status.
    (ソフトウェアはまだ実験的なものです)
- Many documents are written in japanese.
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


## Features (機能)

- Generic LZ4 streaming data process
    - Decode LZ4 streaming data: LZ4.decode
    - Encode LZ4 streaming data: LZ4.encode
- Generic LZ4 streaming data file process
    - Decode LZ4 streaming data file: LZ4.decode\_file
    - Encode LZ4 streaming data file: LZ4.encode\_file
- Primitive LZ4 data process
    - Decode LZ4 data: LZ4.raw\_decode
    - Encode LZ4 data: LZ4.raw\_encode (supporting high compression level)
    - Streaming Decode LZ4 data: LZ4.raw\_stream\_decode and LZ4::RawStreamDecoder#update
    - Streaming Encode LZ4 data: LZ4.raw\_stream\_encode and LZ4::RawStreamEncoder#update (supporting high compression level)


## About security (セキュリティについて)

extlz4 はセキュリティレベルとオブジェクトの汚染状態を確認し、禁止される処理を決定します。

セーフレベルが4未満であれば、禁止される処理はありません。

セーフレベルが4以上の場合、入力と出力 (ストリーム処理の場合はストリーム処理器が含まれる) のすべてが汚染状態でなければ禁止されます。

いずれのセーフレベルにおいても、オブジェクト間で汚染状態が一方向伝播されます。オブジェクトの汚染伝播については『入力 -> 出力』となり、ストリーム処理の場合は『入力 -> 圧縮器・伸張器 -> 出力』というようになります。


## Examples (用例)

First, load extlz4. (最初に extlz4 を読み込んでください)

    require "extlz4"

### Decoding (伸張処理)

    uncompressed_data_string = LZ4.decode(compressed_data_string)

### Encoding (通常圧縮処理)

    compressed_data_string = LZ4.encode(uncompressed_data_string)

### High compression encoding (高効率圧縮処理)

    compressed_data_string = LZ4.encode(uncompressed_data_string, 9)

### Stream decoding

    File.open("sample.txt.lz4", "rb") do |file|
      LZ4.decode(file) do |lz4|
        lz4.read(50)  # read 50 bytes as string
        lz4.getc      # read 1 byte as integer
        lz4.read      # read rest bytes as string
      end
    end

### Stream encoding by high compression

    File.open("sample.txt.lz4", "wb") do |file|
      LZ4.encode(file, 9) do |lz4|
        lz4 << "#{Time.now}: abcdefghijklmnopqrstuvwxyz\n"
        lz4.write "#{Time.now}: abcdefghijklmnopqrstuvwxyz\n"
      end
    end

### Stream encoding without block

    file = File.open("sample.txt.lz4", "wb")
    lz4 = LZ4.encode(file)
    lz4 << "abcdefghijklmnopqrstuvwxyz\n"
    lz4.close  # VERY IMPORTANT!

### Raw data processing (high compression encoding and decoding)

    src = "abcdefg" * 100
    lz4data = LZ4.raw_encode(16, src)
    data = LZ4.raw_decode(lz4data)
    p src == data  # => true

### Raw stream data processing (high compression encoding and decoding)

    blocksize = 4 * 1024  # 4 KiB (REQEUIRED PARAMETER)
    ishighcompress = true  # use high compression method (OPTIONAL PARAMETER)
    encoder = LZ4.raw_stream_encode(blocksize, ishighcompress)

    src = "abcdefg" * 100
    lz4data1 = encoder.update(16, src)

    decoder = LZ4.raw_stream_decode  # not required blocksize

    data = decoder.update(lz4data1)
    p src == data  # => true

    lz4data2 = encoder.update(src)  # default high compression level
    p "lz4data1.bytesize" => lz4data1.bytesize,
      "lz4data2.bytesize" => lz4data2.bytesize

    data = decoder.update(lz4data2)
    p src == data  # => true


## おまけ

コマンドラインプログラムとして `extlz4` が追加されます。

これは lz4 と同程度の機能を持ちます (車輪の再発明とも言う)。

とはいえ、引数のとり方を変えてあり、gzip のような形で利用できます。
