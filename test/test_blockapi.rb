#!ruby

require "test-unit"
require "extlz4"
require "openssl" # for OpenSSL::Random.random_bytes

SMALLSIZE = 400
BIGSIZE = 12000000

SAMPLES = [
  "",
  "\0".b * SMALLSIZE,
  "\0".b * BIGSIZE,
  "\xaa".b * SMALLSIZE,
  "\xaa".b * BIGSIZE,
  OpenSSL::Random.random_bytes(SMALLSIZE),
  OpenSSL::Random.random_bytes(BIGSIZE),
]

SAMPLES << File.read("/usr/ports/INDEX-10", mode: "rb") rescue nil # if on FreeBSD
SAMPLES << File.read("/boot/kernel/kernel", mode: "rb") rescue nil # if on FreeBSD

$stderr.puts "%s:%d: prepaired sample data (%d samples)\n" % [__FILE__, __LINE__, SAMPLES.size]

class TestRawAPI < Test::Unit::TestCase
  def test_encode_decode
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[0])) == SAMPLES[0] }
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[2])) == SAMPLES[2] }
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[3])) == SAMPLES[3] }
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[4])) == SAMPLES[4] }
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[5])) == SAMPLES[5] }
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[6])) == SAMPLES[6] }
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[7])) == SAMPLES[7] } if SAMPLES.size > 7
    assert { LZ4.raw_decode(LZ4.raw_encode(SAMPLES[8])) == SAMPLES[8] } if SAMPLES.size > 8
  end
end
