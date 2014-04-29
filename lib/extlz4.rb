#vim: set fileencoding:utf-8

# check has xxhash
unless $:.find { |dir| File.exist?(File.join(dir, "xxhash.rb")) } ||
       Object.const_defined?(:Gem) && !Gem.find_latest_files("xxhash").empty?
  raise LoadError, "``xxhash'' library is not found. Need ``gem install xxhash'', or ``ruby --enable gems''"
end

require "xxhash"
require "stringio"

ver = RbConfig::CONFIG["ruby_version"]
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(File.dirname(__FILE__), ver, soname)
if File.file?(lib)
  require_relative File.join(ver, soname)
else
  require_relative soname
end

module LZ4
  module_function
  def file_decode(inpath, outpath)
    File.open(inpath, "rb") do |infile|
      LZ4.decode(infile) do |lz4|
        File.open(outpath, "wb") do |outfile|
          inbuf = ""
          outfile << inbuf while lz4.read(262144, inbuf)
        end
      end
    end
  end

  module_function
  def file_encode(inpath, outpath, level = 1, opt = {})
    File.open(inpath, "rb") do |infile|
      File.open(outpath, "wb") do |outfile|
        LZ4.encode(outfile, level, opt) do |lz4|
          inbuf = ""
          nil while lz4 << infile.read(262144, inbuf)
        end
      end
    end
  end

  #
  # @overload encode(string, level = 1, opts = {})
  #
  #   Basic stream encode method.
  #
  #   @return [String] encoded data as LZ4 stream
  #
  #   @param [String] string
  #     LZ4 ストリームとして圧縮したいバイナリデータ列としての文字列です。文字符号情報は無視されて純粋なバイナリデータ列として処理されます。
  #
  #   @param [Integer] level
  #     圧縮レベルを指定します。0 から 9 までの整数値が指定出来ます。
  #
  #     現時点では lz4cli (lz4io) に倣って、3未満が標準圧縮、3以上が高圧縮となります。
  #
  #     raw_encode には高圧縮レベルが指定できますが、現在の実装では常に0(規定値)扱いとなります。
  #
  #   @option opts [true, false] :legacy (false) THIS IS NOT SUPPORTED FUNCTION.
  #
  #   @option opts [true, false] :block_dependency (false)
  #     Enable or disable block dependency funcion. Default is false.
  #
  #     真を与えた場合、ストリームの圧縮効率が向上しますが、特定のブロックのみを取り出すことが難しくなります。
  #
  #   @option opts [true,false] :block_checksum (false)
  #     ブロックごとにチェックサム (XXhash32) を付随させます。
  #
  #   @option opts [nil, Integer] :stream_size (nil)
  #     THIS IS NOT IMPLEMENTED FUNCTION.
  #
  #   @option opts [true, false] :stream_checksum (true)
  #     ストリーム全体のチェックサム (XXhash32) を付随させます。
  #
  #   @option opts [String] :preset_dictionary (nil)
  #     THIS IS NOT IMPLEMENTED FUNCTION.
  #
  #     Because, original lz4 library is not implemented yet. (Feb. 2014)
  #
  # @overload encode(output_io, level = 1, opts = {})
  #
  #   Available LZ4 stream encode.
  #
  #   Write to encoder for data encoding.
  #
  #   After finished encode process, you must call +StreamEncoder#close+.
  #
  #   If given an IO object (or psudo-object), return stream encoder.
  #   この圧縮器に『書き込む』ことでデータは圧縮されます。圧縮処理を完了するときには #close を呼び出す必要があります。
  #
  #   @return [LZ4::StreamEncoder]
  #
  #   @param [IO] output_io
  #     LZ4 ストリームの出力先を指定します。IO#<< と同等の機能を持つオブジェクトである必要があります。
  #
  #     一例を挙げると、IO、StringIO、String などのインスタンスが当てはまります。
  #
  # @overload encode(output_io, level = 1, opts = {}) { |stream_encoder| ... }
  #
  #   IO オブジェクトとともにブロックを渡した場合、ブロック引数として圧縮器が渡されます。この場合は #close を呼び出す必要がありません。
  #
  #   @return return status of given block
  #
  #   @yield [stream_encoder]
  #
  #   @yieldreturn return as method return value
  #
  # Encode to LZ4 stream data. This is available streaming process.
  #
  # Created data is decodable by lz4-cli.
  #
  # @example directly stream encode
  #
  #   LZ4.encode("abcdefghijklmn") # => Encoded LZ4 stream data (string object)
  #
  # @example stream stream encode with block
  #
  #   File.open("hello.txt", "rb") do |src|
  #     File.open("hello.txt.lz4", "wb") do |dest|
  #       srcbuf = ""
  #       LZ4.encode(dest) do |lz4encoder|
  #         nil while lz4encoder << src.read(4096, srcbuf)
  #       end
  #     end
  #   end
  #
  def self.encode(first, *args)
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
    raise ArgumentError, "unknown key - #{left[0]}" unless left.empty?

    if first.kind_of?(String)
      src = first
      dest = StringIO.new("")
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

  #
  # @overload decode(encoded_data_string)
  #   @return (String) decoded_data
  # @overload decode(input_io)
  #   @return (LZ4::StreamDecoder) stream_decoder
  # @overload decode(input_io) { |stream_decoder| ... }
  #   @return yield_status
  #
  # Decode LZ4 stream data. This is available streaming process.
  #
  # @note
  #   Current implementation is possible 'sequential read' only.
  #   'read behind' or 'random read' are not available.
  #
  # @param [IO] input_io
  #   This is IO like object. Need read method.
  #
  # @return [StreamDecoder]
  #   ストリーム展開オブジェクトです。簡素な機能の読み込み専用IOオブジェクトとして扱うことが出来ます。
  #
  #   stream_decoder は GC によって開放処理が行われますが、利用しなくなった時点で利用者が明示的に close を呼び出すことが望まれます。
  #
  # @return [yield_status]
  #   decode に渡したブロックの戻り値をそのまま返します。
  #
  # @yield [stream_decoder]
  #   ブロックなしで与えた場合の戻り値と等価です。
  #
  #   ただしこちらはブロックを抜けたらすぐに開放処理が実施されます。利用者が明示的に close を呼んだり、GC されるのを待ったりせずに行われると言うことです。
  #
  # @example directly decode
  #
  #   LZ4.decode(lz4_stream_encoded_string) # => decoded binary string
  #
  # @example stream decode
  #
  #   File.open("sample.lz4", "rb") do |fd|
  #     LZ4.decode(fd) do |lz4dec|
  #       lz4dec.read(16)  # string with read 16 bytes
  #       lz4dec.getbyte   # integer with a byte
  #       lz4dec.read      # string with rest data
  #     end
  #   end
  #
  def self.decode(io, &block)
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

    # stream
    #     +4: magic number (little endian)
    #     +3-15: tream descriptor
    #       +1: stream flags
    #         (MSB to LSB)
    #         :2: version number (= 1) (CONSTANT)
    #         :1: block independence (= 1)
    #         :1: block checksum (= 0)
    #         :1: stream size (= 0)
    #         :1: stream checksum (= 1)
    #         :1: reserved (= 0) (CONSTANT)
    #         :1: preset dictionary (= 0)
    #       +1: block flags
    #         (MSB to LSB)
    #         :1: RESERVED (= 0) (CONSTANT)
    #         :3: block maximum size (= 7)
    #         :4: RESERVED (= 0) (CONSTANT)
    #       +0 or 8: stream size (used if stream size of stream flags)
    #       +0 or 4: dictionary ID (used if preset dictionary of stream flags)
    #       +1: header checksum
    #     **: blocks ...
    #     +4: end of stream
    #     +0-4: stream checksum

    # data block
    #
    #   +4: block size (little endian)
    #   ++: compressed data
    #   +0-4: block checksum

    # stream format (little endian)
    #   +4: magic number (= 0x184c2102)
    #   ++: data block
    #     +4: compressed size
    #     ++: compressed data
    #   <end of file>
  end

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
      raise ArgumentError, "not supported for block_checksum" if block_checksum
      @stream_checksum = XXhash::Internal::StreamingHash.new(0) if stream_checksum

      @blocksize = BLOCK_MAXIMUM_SIZES[blocksize]
      raise ArgumentError, "wrong blocksize (#{blocksize})" unless @blocksize

      @block_dependency = !!block_dependency
      ishc = level < 3 ? false : true
      case
      when block_dependency
        @encoder = LZ4::RawStreamEncoder.new(@blocksize, ishc).method(:update)
      when ishc
        @encoder = ->(*args) { LZ4.raw_encode(0, *args) }
      else
        @encoder = ->(*args) { LZ4.raw_encode(*args) }
      end
      @io = io
      @buf = "".force_encoding(Encoding::BINARY)
      @dest = @buf.dup # destination buffer for encoder

      header = [MAGIC_NUMBER].pack("V")
      sd = VERSION_NUMBER | 
           (@block_dependency ? 0 : BLOCK_INDEPENDENCY) |
           (block_checksum ? BLOCK_CHECKSUM : 0) |
           (false ? STREAM_SIZE : 0) |
           (@stream_checksum ? STREAM_CHECKSUM : 0) |
           (false ? PRESET_DICTIONARY : 0)
      bd = (blocksize << 4)
      desc = [sd, bd].pack("CC")
      header << desc
      # TODO: header << [stream_size].pack("Q<") if stream_size
      # TODO: header << [XXhash.xxh32(predict)].pack("V") if predict # preset dictionary
      header << [XXhash.xxh32(desc, 0) >> 8].pack("C")
      @io << header
    end

    #
    # Export data with lz4 compress.
    #
    # If data is nil, return to process nothing.
    #
    # @param [String] data
    #
    # @return [self]
    #
    # @return [nil] given nil to data
    #
    def write(data)
      return nil if data.nil?
      data = data.to_s
      off = 0
      while off < data.bytesize
        @buf << w = data.byteslice(off, [@blocksize - @buf.bytesize, @blocksize].min)
        off += w.bytesize
        next if @buf.bytesize < @blocksize
        export_block
      end

      self
    end

    alias << write

    def close
      export_block unless @buf.empty?
      @io << [0].pack("V")
      @io << [@stream_checksum.digest].pack("V") if @stream_checksum
      @io.flush if @io.respond_to?(:flush)
      @io = nil
    end

    private
    def export_block
      w = @encoder.(@buf, @dest)
      @stream_checksum.update(@buf) if @stream_checksum
      if w.bytesize < @buf.bytesize
        # 上限を超えずに圧縮できた
        @io << [w.bytesize].pack("V") << w
      else
        # 圧縮後は上限を超過したため、無圧縮データを出力する
        @io << [w.bytesize | LITERAL_DATA_BLOCK_FLAG].pack("V") << @buf
      end
      # @io << [@blockchecksum].pack("V") # TODO: export block checksum
      @buf.clear
    end
  end

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
        raise LZ4Error, "stream header error - wrong block maximum size (#{blockmax} for 4 .. 7)" unless @blockmaximum

        @streamsize = io.read(8).unpack("Q<")[0] if streamsize
        @presetdict = io.read(4).unpack("V")[0] if presetdict

        headerchecksum = io.getbyte

        if @blockindependence
          @decoder = LZ4.method(:raw_decode)
        else
          @decoder = LZ4::RawStreamDecoder.new.method(:update)
        end
      when MAGIC_LEGACY_NUMBER
        @version = -1
        @blockindependence = true
        @blockchecksum = false
        @streamchecksum = false
        @blockmaximum = 1 << 23 # 8 MiB
        @streamsize = nil
        @presetdict = nil
        @decoder = LZ4.method(:raw_decode)
      else
        raise LZ4Error, "stream header error - wrong magic number"
      end

      @io = io
      @pos = 0
    end

    def close
      @io = nil
    end

    # call-seq:
    #   read -> string or nil
    #   read(size) -> string or nil
    #   read(size, dest) -> string or nil
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
      dest = @buf || ""
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

      while true
        unless @buf && !@buf.empty?
          unless @buf = getnextblock
            @pos = nil
            if dest.empty?
              return nil
            else
              return dest
            end
          end
        end

        dest << @buf.slice!(0, size)
        size -= dest.bytesize
        return dest unless size > 0
      end
    end

    private
    def getnextblock
      flags = @io.read(4).unpack("V")[0]
      iscomp = (flags >> 31) == 0 ? true : false
      blocksize = flags & 0x7fffffff
      return nil unless blocksize > 0
      w = @io.read(blocksize)
      raise IOError, "can not read block (readsize=#{w.bytesize}, needsize=#{blocksize} (#{"0x%x" % blocksize}))" unless w.bytesize == blocksize
      w = @decoder.(w) if iscomp
      @io.read(4) if @blockchecksum # TODO: IMPLEMENT ME! compare checksum
      w
    end
  end
end
