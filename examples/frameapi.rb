#!ruby

#
# This code is under public domain (CC0)
# <http://creativecommons.org/publicdomain/zero/1.0/>.
#
# To the extent possible under law, dearblue has waived all copyright
# and related or neighboring rights to this work.
#
#     dearblue <dearblue@users.sourceforce.jp>
#

# need for calcration crc32 in this example
class String
  require "zlib"

  def crc32
    Zlib.crc32(self)
  end
end

########

# first, load library
require "extlz4"

# prepair source data
src = File.read(ARGV[0] || __FILE__, mode: "rb")
puts "%s:%d: src.bytesize=%d, src.crc32=0x%08X\n" %
        [__FILE__, __LINE__, src.bytesize, src.crc32]

# compress data by LZ4 Frame
encdata = LZ4.encode(src)
# OR, encdata = LZ4.encode(src, level = 1)
puts "%s:%d: encdata.bytesize=%d, encdata.crc32=0x%08X\n" %
        [__FILE__, __LINE__, encdata.bytesize, encdata.crc32]

decdata = LZ4.decode(encdata)
puts "%s:%d: decdata.bytesize=%d, decdata.crc32=0x%08X\n" %
        [__FILE__, __LINE__, decdata.bytesize, decdata.crc32]

puts "%s:%d: comparison source data and decompressed data: %s\n" %
        [__FILE__, __LINE__, src == decdata ? "SAME" : "NOT SAME (BUG!)"]
