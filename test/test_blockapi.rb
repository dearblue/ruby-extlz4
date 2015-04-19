#!ruby

require "test-unit"
require "openssl" # for OpenSSL::Random.random_bytes
require "extlz4"

require_relative "sampledata"

class TestBlockAPI < Test::Unit::TestCase
  SAMPLES.each_pair do |name, data|
    define_method("test_block_encode_decode_sample:#{name}", -> {
      assert(data, LZ4.block_decode(LZ4.block_encode(data)))
    })
  end

  def test_block_encode
    buf = ""
    assert_kind_of(String, LZ4.block_encode(SAMPLES["\\0 (small size)"]))
    assert_kind_of(String, LZ4.block_encode(SAMPLES["\\0 (small size)"], 1000))
    assert_same(buf, LZ4.block_encode(SAMPLES["\\0 (small size)"], buf))
    assert_same(buf, LZ4.block_encode(SAMPLES["\\0 (small size)"], 1000, buf))
    assert_same(buf, LZ4.block_encode(nil, SAMPLES["\\0 (small size)"], 1000, buf))

    # high compression
    assert_kind_of(String, LZ4.block_encode(0, SAMPLES["\\0 (small size)"]))
    assert_kind_of(String, LZ4.block_encode(0, SAMPLES["\\0 (small size)"], 1000))
    assert_same(buf, LZ4.block_encode(0, SAMPLES["\\0 (small size)"], buf))
    assert_same(buf, LZ4.block_encode(0, SAMPLES["\\0 (small size)"], 1000, buf))
  end

  def test_block_encode_invalid_args
    src = SAMPLES["\\0 (small size)"]
    buf = ""
    assert_raise(ArgumentError) { LZ4.block_encode } # no arguments
    assert_raise(ArgumentError) { LZ4.block_encode(-1, src) } # wrong level
    assert_raise(TypeError) { LZ4.block_encode(100) } # source is not string
    assert_raise(TypeError) { LZ4.block_encode(nil) } # source is not string
    assert_raise(TypeError) { LZ4.block_encode(:bad_input) } # source is not string
    assert_raise(TypeError) { LZ4.block_encode(/bad-input/) } # source is not string
    assert_raise(RuntimeError) { LZ4.block_encode(src, "bad-destbuf".freeze) } # can't modify frozen String
    assert_raise(LZ4::Error) { LZ4.block_encode(src, 1) } # maxdest is too small
    assert_raise(LZ4::Error) { LZ4.block_encode(src, 1, buf) } # maxdest is too small
    assert_raise(TypeError) { LZ4.block_encode(src, "bad-maxsize", "a") } # "bad-maxsize" is not integer
  end

  def test_block_decode
    src = LZ4.block_encode(SAMPLES["\\0 (small size)"])
    buf = ""
    assert_kind_of(String, LZ4.block_decode(src))
    assert_kind_of(String, LZ4.block_decode(src, 1000))
    assert_same(buf, LZ4.block_decode(src, buf))
    assert_same(buf, LZ4.block_decode(src, 1000, buf))
  end

  def test_block_decode_invalid_args
    src = LZ4.block_encode(SAMPLES["\\0 (small size)"])
    buf = ""
    assert_raise(ArgumentError) { LZ4.block_decode } # no arguments
    assert_raise(TypeError) { LZ4.block_decode(-1, src) } # do not given level
    assert_raise(TypeError) { LZ4.block_decode(100) } # source is not string
    assert_raise(TypeError) { LZ4.block_decode(nil) } # source is not string
    assert_raise(TypeError) { LZ4.block_decode(:bad_input) } # source is not string
    assert_raise(TypeError) { LZ4.block_decode(/bad-input/) } # source is not string
    assert_raise(RuntimeError) { LZ4.block_decode(src, "bad-destbuf".freeze) } # can't modify frozen String
    assert_raise(TypeError) { LZ4.block_decode(src, "bad-maxsize", "a") } # "bad-maxsize" is not integer

    src2 = SAMPLES["\\xaa (small size)"]
    assert_raise(LZ4::Error) { LZ4.block_decode(src2) } # encounted invalid end of sequence
    assert_raise(LZ4::Error) { LZ4.block_decode(src2, 100000) } # max_dest_size is too small, or data is corrupted
  end
end
