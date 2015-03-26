#!ruby

# 必要と思われる試験項目
#
# * frameapi の圧縮処理
#   * LZ4.encode
#     * LZ4.decode で伸長できるか
#     * lz4-cli で伸長できるか
#     * 汚染状態の伝搬
#     * security level
#   * LZ4.encode_file
#     * LZ4.decode_file で伸長できるか
#     * lz4-cli で伸長できるか
# * frameapi の伸長処理
#   * LZ4.decode
#   * LZ4.decode_file
#   * LZ4.test_file
#
# * 試験で用いる試料
#   * /usr/ports/INDEX-10
#   * /boot/kernel/kernel
#   * 長さ 0 の空データ
#   * 0 で埋められた小さなデータ
#   * 0 で埋められたでかいデータ
#   * 0xaa で埋められた小さなデータ
#   * 0xaa で埋められたでかいデータ
#   * /dev/random (4000 bytes)
#   * /dev/random (12000000 bytes)
#   * 可能であれば数十 GB レベルのファイル

require "test-unit"
require "extlz4"
require "openssl" # for OpenSSL::Random.random_bytes
#require "zlib" # for Zlib.crc32

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

class TestFrameAPI < Test::Unit::TestCase
  def test_encode_decode
    assert { LZ4.decode(LZ4.encode(SAMPLES[0])) == SAMPLES[0] }
    assert { LZ4.decode(LZ4.encode(SAMPLES[1])) == SAMPLES[1] }
    assert { LZ4.decode(LZ4.encode(SAMPLES[2])) == SAMPLES[2] }
    assert { LZ4.decode(LZ4.encode(SAMPLES[3])) == SAMPLES[3] }
    assert { LZ4.decode(LZ4.encode(SAMPLES[4])) == SAMPLES[4] }
    assert { LZ4.decode(LZ4.encode(SAMPLES[5])) == SAMPLES[5] }
    assert { LZ4.decode(LZ4.encode(SAMPLES[6])) == SAMPLES[6] }
    assert { LZ4.decode(LZ4.encode(SAMPLES[7])) == SAMPLES[7] } if SAMPLES.size > 7
    assert { LZ4.decode(LZ4.encode(SAMPLES[8])) == SAMPLES[8] } if SAMPLES.size > 8
  end
end
