#
# This code is under public domain (CC0)
# <http://creativecommons.org/publicdomain/zero/1.0/>.
#
# To the extent possible under law, dearblue has waived all copyright
# and related or neighboring rights to this work.
#
#     dearblue <dearblue@users.sourceforce.jp>
#

require_relative "../extlz4"
require "stringio"

require "rubygems"
gem "xxhash", "~> 0.3"
require "xxhash"

module LZ4
  def self.encode_old(first, *args)
    case args.size
    when 0
      level = nil
      opts = StreamEncoder::OPTIONS
    when 1
      level = args[0]
      if level.respond_to?(:to_hash)
        opts = StreamEncoder::OPTIONS.merge(level)
        level = nil
      else
        level = level.to_i
        opts = StreamEncoder::OPTIONS
      end
    when 2
      level = args[0].to_i
      opts = StreamEncoder::OPTIONS.merge(args[1])
    else
      raise ArgumentError, "wrong number of arguments (#{args.size + 1} for 1 .. 3)"
    end

    left = opts.keys - StreamEncoder::OPTIONS.keys
    unless left.empty?
      if left.size > 10
        raise ArgumentError, "unknown key - #{left[0]} (for #{StreamEncoder::OPTIONS.keys.slice(0, 10).join(", ")} and more...)"
      else
        raise ArgumentError, "unknown key - #{left[0]} (for #{StreamEncoder::OPTIONS.keys.join(", ")})"
      end
    end

    if first.kind_of?(String)
      src = first
      dest = StringIO.new("".b)
    else
      src = nil
      dest = first
    end

    lz4 = StreamEncoder.new(dest, level || 1,
                            opts[:blocksize], opts[:block_dependency],
                            opts[:block_checksum], opts[:stream_checksum])

    case
    when src
      lz4 << src
      lz4.close
      dest.string
    when block_given?
      begin
        yield(lz4)
      ensure
        lz4.close
      end
    else
      lz4
    end
  end

  def self.decode_old(io, &block)
    if io.kind_of?(String)
      lz4 = StreamDecoder.new(StringIO.new(io))
      dest = lz4.read
      lz4.close
      return dest
    end

    dec = StreamDecoder.new(io)
    return dec unless block_given?

    begin
      yield(dec)
    ensure
      dec.close
    end
  end


  module BasicStream
    MAGIC_NUMBER = 0x184D2204
    MAGIC_NUMBER_LEGACY = 0x184C2102

    BLOCK_MAXIMUM_SIZES = {
      # 0 => not available
      # 1 => not available
      # 2 => not available
      # 3 => not available
      4 => 1 << 16, # 64 KiB
      5 => 1 << 18, # 256 KiB
      6 => 1 << 20, # 1 MiB
      7 => 1 << 22, # 4 MiB
    }

    LITERAL_DATA_BLOCK_FLAG = 0x80000000

    VERSION_NUMBER = 1 << 6
    VERSION_NUMBER_MASK = 0x03 << 6
    BLOCK_INDEPENDENCY = 1 << 5
    BLOCK_CHECKSUM = 1 << 4
    STREAM_SIZE = 1 << 3
    STREAM_CHECKSUM = 1 << 2
    PRESET_DICTIONARY = 1 << 0

    Header = Struct.new(:magic,
                        :version,
                        :blockindependence,
                        :blockchecksum,
                        :streamchecksum,
                        :blocksize,
                        :streamsize,
                        :predictid)

    class Header
      def self.load(io)
        case magic = io.read(4).unpack("V")
        when MAGIC_NUMBER_LEGACY
          new(magic, -1, true, false, false, 8 * 1024 * 1024, nil, nil)
        when MAGIC_NUMBER
          (sf, bd) = io.read(2).unpack("CC")
          version = (sf >> 6) & 0x03
          raise "stream header error - wrong version number" unless version == 0x01
          blockindependence = ((sf >> 5) & 0x01) == 0 ? false : true
          blockchecksum = ((sf >> 4) & 0x01) == 0 ? false : true
          streamsize = ((sf >> 3) & 0x01) == 0 ? false : true
          streamchecksum = ((sf >> 2) & 0x01) == 0 ? false : true
          # reserved = (sf >> 1) & 0x01
          predictid = ((sf >> 0) & 0x01) == 0 ? false : true

          # reserved = (bd >> 7) & 0x01
          blockmax = (bd >> 4) & 0x07
          # reserved = (bd >> 0) & 0x0f

          blocksize = BLOCK_MAXIMUM_SIZES[blockmax]
          raise Error, "stream header error - wrong block maximum size (#{blockmax} for 4 .. 7)" unless blocksize

          streamsize = io.read(8).unpack("Q<")[0] if streamsize
          predictid = io.read(4).unpack("V")[0] if predictid

          headerchecksum = io.getbyte

          new(magic, version, blockindependence, blockchecksum, streamchecksum, blocksize, streamsize, predictid)
        else
          raise "could not recognized magic number (0x%08x)" % (magic || nil)
        end
      end

      def self.pack(*args)
        new(*args).pack
      end

      def pack
        raise "wrong magic number" unless magic == MAGIC_NUMBER
        raise "wrong version number" unless version == VERSION_NUMBER

        header = [magic].pack("V")
        sd = version |
             (blockindependence ? BLOCK_INDEPENDENCY : 0) |
             (blockchecksum ? BLOCK_CHECKSUM : 0) |
             (streamsize ? STREAM_SIZE : 0) |
             (streamchecksum ? STREAM_CHECKSUM : 0) |
             (predictid ? PRESET_DICTIONARY : 0)
        bd = (BLOCK_MAXIMUM_SIZES.rassoc(blocksize)[0] << 4)
        desc = [sd, bd].pack("CC")
        header << desc
        header << [streamsize].pack("Q<") if streamsize
        header << [predictid].pack("V") if predictid
        header << [XXhash.xxh32(desc) >> 8].pack("C")
      end
    end

    BlockHeader = Struct.new(:iscompress,
                             :packedsize)

    class BlockHeader
      alias compress? iscompress
      undef iscompress
      undef iscompress=
      undef packedsize=

      def pack
        [(compress? ? 0 : LITERAL_DATA_BLOCK_FLAG) | packedsize].pack("V")
      end

      def self.pack(iscompress, packedsize)
        new(iscompress, packedsize).pack
      end

      def self.unpack(data)
        d = data.unpack("V")[0]
        new((d & LITERAL_DATA_BLOCK_FLAG) == 0 ? true : false,
            packedsize & ~LITERAL_DATA_BLOCK_FLAG)
      end

      def self.load(io)
        unpack io.read(4)
      end
    end
  end

  #
  # LZ4 stream encoder
  #
  class StreamEncoder
    include BasicStream

    OPTIONS = {
      legacy: false,
      blocksize: 7,
      block_dependency: false,
      block_checksum: false,
      stream_checksum: true,
    }

    def initialize(io, level, blocksize, block_dependency,
                   block_checksum, stream_checksum)
      @block_checksum = !!block_checksum
      @stream_checksum = XXhash::XXhashInternal::StreamingHash32.new(0) if stream_checksum

      @blocksize = BLOCK_MAXIMUM_SIZES[blocksize]
      raise ArgumentError, "wrong blocksize (#{blocksize})" unless @blocksize

      @block_dependency = !!block_dependency
      level = level ? level.to_i : nil
      case
      when level.nil? || level < 4
        level = nil
      when level > 16
        level = 16
      end
      @encoder = get_encoder(level, @block_dependency)
      @io = io
      @buf = "".force_encoding(Encoding::BINARY)

      header = [MAGIC_NUMBER].pack("V")
      sd = VERSION_NUMBER | 
           (@block_dependency ? 0 : BLOCK_INDEPENDENCY) |
           (@block_checksum ? BLOCK_CHECKSUM : 0) |
           (false ? STREAM_SIZE : 0) |
           (@stream_checksum ? STREAM_CHECKSUM : 0) |
           (false ? PRESET_DICTIONARY : 0)
      bd = (blocksize << 4)
      desc = [sd, bd].pack("CC")
      header << desc
      # TODO: header << [stream_size].pack("Q<") if stream_size
      # TODO: header << [XXhash.xxh32(predict)].pack("V") if predict # preset dictionary
      header << [XXhash.xxh32(desc) >> 8].pack("C")
      @io << header
    end

    #
    # call-seq:
    #   write(data) -> nil or self
    #
    # Write data to lz4 stream.
    #
    # If data is nil, return to process nothing.
    #
    # [RETURN (self)]
    #   Success write process.
    #
    # [RETURN (nil)]
    #   Given nil to data.
    #
    # [data (String)]
    #
    def write(data)
      return nil if data.nil?
      @slicebuf ||= ""
      @inputproxy ||= StringIO.new
      @inputproxy.string = String(data)
      until @inputproxy.eof?
        slicesize = @blocksize - @buf.bytesize
        slicesize = @blocksize if slicesize > @blocksize
        @buf << @inputproxy.read(slicesize, @slicebuf)
        export_block if @buf.bytesize >= @blocksize
      end

      self
    end

    #
    # Same as `write` method, but return self always.
    #
    def <<(data)
      write data
      self
    end

    def close
      export_block unless @buf.empty?
      @io << [0].pack("V")
      @io << [@stream_checksum.digest].pack("V") if @stream_checksum
      @io.flush if @io.respond_to?(:flush)
      @io = nil
    end

    private
    def get_encoder(level, block_dependency)
      workencbuf = "".force_encoding(Encoding::BINARY)
      if block_dependency
        streamencoder = LZ4::BlockEncoder.new(level)
        ->(src) { streamencoder.update(src, workencbuf) }
      else
        ->(src) { LZ4.block_encode(level, src, workencbuf) }
      end
    end

    private
    def export_block
      w = @encoder.(@buf)
      @stream_checksum.update(@buf) if @stream_checksum
      if w.bytesize < @buf.bytesize
        # 上限を超えずに圧縮できた
        @io << [w.bytesize].pack("V") << w
      else
        # 圧縮後は上限を超過したため、無圧縮データを出力する
        @io << [@buf.bytesize | LITERAL_DATA_BLOCK_FLAG].pack("V") << @buf
        w = @buf
      end

      if @block_checksum
        @io << [XXhash.xxh32(w)].pack("V")
      end
      @buf.clear
    end
  end

  #
  # LZ4 ストリームを伸張するためのクラスです。
  #
  class StreamDecoder
    include BasicStream

    attr_reader :version
    attr_reader :blockindependence
    attr_reader :blockchecksum
    attr_reader :streamchecksum
    attr_reader :blockmaximum
    attr_reader :streamsize
    attr_reader :presetdict

    def initialize(io)
      magic = io.read(4).unpack("V")[0]
      case magic
      when MAGIC_NUMBER
        sf = io.getbyte
        @version = (sf >> 6) & 0x03
        raise "stream header error - wrong version number" unless @version == 0x01
        @blockindependence = ((sf >> 5) & 0x01) == 0 ? false : true
        @blockchecksum = ((sf >> 4) & 0x01) == 0 ? false : true
        streamsize = ((sf >> 3) & 0x01) == 0 ? false : true
        @streamchecksum = ((sf >> 2) & 0x01) == 0 ? false : true
        # reserved = (sf >> 1) & 0x01
        presetdict = ((sf >> 0) & 0x01) == 0 ? false : true

        bd = io.getbyte
        # reserved = (bd >> 7) & 0x01
        blockmax = (bd >> 4) & 0x07
        # reserved = (bd >> 0) & 0x0f

        @blockmaximum = BLOCK_MAXIMUM_SIZES[blockmax]
        raise Error, "stream header error - wrong block maximum size (#{blockmax} for 4 .. 7)" unless @blockmaximum

        @streamsize = io.read(8).unpack("Q<")[0] if streamsize
        @presetdict = io.read(4).unpack("V")[0] if presetdict

        headerchecksum = io.getbyte

        if @blockindependence
          @decoder = LZ4.method(:block_decode)
        else
          @decoder = LZ4::BlockDecoder.new.method(:update)
        end
      when MAGIC_NUMBER_LEGACY
        @version = -1
        @blockindependence = true
        @blockchecksum = false
        @streamchecksum = false
        @blockmaximum = 1 << 23 # 8 MiB
        @streamsize = nil
        @presetdict = nil
        @decoder = LZ4.method(:block_decode)
      else
        raise Error, "stream header error - wrong magic number"
      end

      @io = io
      @pos = 0

      @readbuf = "".b
      @decodebuf = "".b
    end

    def close
      @io = nil
    end

    #
    # call-seq:
    #   read -> string or nil
    #   read(size) -> string or nil
    #   read(size, dest) -> string or nil
    #
    def read(*args)
      case args.size
      when 0
        read_all
      when 1
        read_part(args[0].to_i, "")
      when 2
        read_part(args[0].to_i, args[1])
      else
        raise ArgumentError, "wrong number of arguments (#{args.size} for 0 .. 2)"
      end
    end

    def getbyte
      w = read(1) or return nil
      w.getbyte(0)
    end

    def eof
      !@pos
    end

    alias eof? eof

    def tell
      raise NotImplementedError
    end

    def seek(off, cur)
      raise NotImplementedError
    end

    def pos
      raise NotImplementedError
    end

    def pos=(pos)
      raise NotImplementedError
    end

    private
    def read_all
      if @buf
        dest = @buf.read
      else
        dest = ""
      end
      @buf = nil
      w = nil
      dest << w while w = getnextblock
      @pos = nil
      dest
    end

    private
    def read_part(size, dest)
      dest.clear
      return dest unless size > 0
      return nil unless @pos

      @slicebuf ||= ""

      begin
        unless @buf && !@buf.eof?
          unless w = getnextblock
            @pos = nil
            if dest.empty?
              return nil
            else
              return dest
            end
          end

          # NOTE: StringIO を用いている理由について
          #       ruby-2.1 で String#slice 系を使って新しい文字列を生成すると、ヒープ領域の確保量が㌧でもない状況になる。
          #       StringIO#read に読み込みバッファを与えることで、この問題を軽減している。

          @buf ||= StringIO.new
          @buf.string = w
        end

        dest << @buf.read(size, @slicebuf)
        size -= @slicebuf.bytesize
      end while size > 0

      dest
    end

    private
    def getnextblock
      return nil if @version == -1 && @io.eof?

      flags = @io.read(4).unpack("V")[0]
      iscomp = (flags >> 31) == 0 ? true : false
      blocksize = flags & 0x7fffffff
      return nil unless blocksize > 0
      unless blocksize <= @blockmaximum
        raise LZ4::Error, "block size is too big (blocksize is #{blocksize}, but blockmaximum is #{@blockmaximum}. may have damaged)."
      end
      w = @io.read(blocksize, @readbuf)
      unless w.bytesize == blocksize
        raise LZ4::Error, "can not read block (readsize=#{w.bytesize}, needsize=#{blocksize} (#{"0x%x" % blocksize}))"
      end
      w = @decoder.(w, @blockmaximum, @decodebuf) if iscomp
      @io.read(4) if @blockchecksum # TODO: IMPLEMENT ME! compare checksum
      w
    end
  end
end
