#!ruby

=begin
# 必要と思われる試験項目

* LZ4.encode
  * LZ4.decode で伸長できるか
  * lz4-cli で伸長できるか
  * 汚染状態の伝搬
  * security level
* LZ4.encode_file
  * LZ4.decode_file で伸長できるか
  * lz4-cli で伸長できるか
* LZ4.decode
* LZ4.decode_file
* LZ4.test_file

* 試験で用いる試料
  * /usr/ports/INDEX-10
  * /boot/kernel/kernel
  * 長さ 0 の空データ
  * 0 で埋められた小さなデータ
  * 0 で埋められたでかいデータ
  * 0xaa で埋められた小さなデータ
  * 0xaa で埋められたでかいデータ
  * /dev/random (4000 bytes)
  * /dev/random (12000000 bytes)
  * 可能であれば数十 GB レベルのファイル
=end

require "test-unit"
require "extlz4"

require_relative "common"

class TestFrameAPI < Test::Unit::TestCase
  SAMPLES.each_pair do |name, data|
    define_method("test_encode_decode_sample:#{name}", -> {
      assert_equal(Digest::MD5.hexdigest(data), Digest::MD5.hexdigest(LZ4.decode(LZ4.encode(data))))
    })
  end

  def test_encode_args
    assert_kind_of(LZ4::Encoder, LZ4.encode)
    assert_kind_of(LZ4::Encoder, LZ4.encode(StringIO.new("")))
    assert_kind_of(String, LZ4.encode {})
    io = StringIO.new("")
    assert_same(io, LZ4.encode(io) {})
    assert_kind_of(LZ4::Encoder, LZ4.encode(io, 16))
    assert_kind_of(LZ4::Encoder, LZ4::Encoder.new)
  end

  def test_decode_args
    assert_raise(ArgumentError) { LZ4.decode }
    assert_raise(NoMethodError) { LZ4.decode(nil) } # undefined method `read' for nil:NilClass
    assert_raise(LZ4::Error) { LZ4.decode("") } # read error (or already EOF)
    assert_raise(ArgumentError) { LZ4.decode(nil, nil) } # wrong number of arguments (2 for 1)
  end
end
