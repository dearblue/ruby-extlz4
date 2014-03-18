
# LZ4 for ruby

圧縮伸張ライブラリ lz4 の ruby バインディングライブラリです。

LZ4 データストリームを圧縮・伸張できます。lz4cli で扱うことが出来ます。



	$ lz4 < /boot/kernel/freebsd.ko | ruby -r lz4 -e 'data = LZ4.decode($stdin.binmode.read)'


## 概要 (Summary)

- 名称 (Package name): lz4 for ruby
- インストール手順 (How to install): `gem install lz4`
- バージョン情報 (Version): 0.1
- 品質 (Release quality): alpha
- 依存する gem パッケージ (Dependency gems):
  - xxhash - `gem install xxhash`
- 依存する C ライブラリ (Dependency external libraries):
  - (なし) (lz4 はパッケージに同梱)


## 機能 (Functions)

- Generic LZ4 data stream
  - Decode LZ4 data stream: `LZ4.decode`
  - Encode LZ4 data stream: `LZ4.encode`
- Raw LZ4 data process
  - Decode LZ4 data: `LZ4.raw_decode`
  - Encode LZ4 data: `LZ4.raw_encode`
  - Encode LZ4 data with high compression: `LZ4.raw_encode_hc`
  - Streaming Decode LZ4 data: `LZ4.raw_stream_decode`
  - Streaming Encode LZ4 data: `LZ4.raw_stream_encode`
  - Streaming Encode LZ4 data with high compression: `LZ4.raw_stream_encode_hc`


## サンプルコード

### 伸張処理

	require "lz4"
	uncompressed_data_string = LZ4.decode(compressed_data_string)

### 通常圧縮処理

	require "lz4"
	compressed_data_string = LZ4.encode(uncompressed_data_string)

### 高効率圧縮処理

	require "lz4"
	compressed_data_string = LZ4.encode(uncompressed_data_string, 9)
