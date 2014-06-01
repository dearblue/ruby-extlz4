#vim: set fileencoding:utf-8

# check has xxhash
if $:.find { |dir| File.exist?(File.join(dir, "xxhash.rb")) } ||
   Object.const_defined?(:Gem) && !Gem.find_files("xxhash").empty?
  require "xxhash"
else
  unless ENV["RUBY_EXTLZ4_NOT_WARN"]
    warn "#{File.basename caller(0, 1)[0]}: warn - ``xxhash'' library is not found. Posible feature is limited only. If you want full features, try ``gem install xxhash'' or ``ruby --enable gems''."
  end
end

require "stringio"

ver = RbConfig::CONFIG["ruby_version"]
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(File.dirname(__FILE__), ver, soname)
if File.file?(lib)
  require_relative File.join(ver, soname)
else
  require_relative soname
end

#
# LZ4 data and streaming data processor.
#
module LZ4
  LZ4 = self

  #
  # call-seq:
  #   decode_file(inpath, outpath) -> nil
  #
  # Decode lz4 file to regular file.
  #
  # [RETURN]
  #   Return nil always.
  #
  # [inpath]
  #   Give input file path, or input io (liked) object its has ``read'' method.
  #
  # [outpath]
  #   Give output file path, or output io (liked) object its has ``<<'' method.
  #
  def self.decode_file(inpath, outpath)
    open_file(inpath, "rb") do |infile|
      decode(infile) do |lz4|
        open_file(outpath, "wb") do |outfile|
          inbuf = ""
          slicesize = 1 << 20
          outfile << inbuf while lz4.read(slicesize, inbuf)
        end
      end
    end

    nil
  end

  #
  # call-seq:
  #   encode_file(inpath, outpath, level = 1, opt = {}) -> nil
  #
  # Encode regular file to lz4 file.
  #
  # [RETURN]
  #   Return nil always.
  #
  # [inpath]
  #   Give input file path, or input io (liked) object its has ``read'' method.
  #
  # [outpath]
  #   Give output file path, or output io (liked) object its has ``<<'' method.
  #
  # [level = 1 (Integer)]
  #   See LZ4.encode method.
  #
  # [opt = {} (Hash)]
  #   See LZ4.encode method.
  #
  def self.encode_file(inpath, outpath, level = 1, opt = {})
    open_file(inpath, "rb") do |infile|
      open_file(outpath, "wb") do |outfile|
        encode(outfile, level, opt) do |lz4|
          inbuf = ""
          slicesize = 1 << 20
          lz4 << inbuf while infile.read(slicesize, inbuf)
        end
      end
    end

    nil
  end

  def self.test_file(inpath)
    open_file(inpath, "rb") do |infile|
      decode(infile) do |lz4|
        inbuf = ""
        slicesize = 1 << 20
        nil while lz4.read(slicesize, inbuf)
      end
    end

    nil
  end

  def self.open_file(file, mode)
    case
    when file.kind_of?(String)
      File.open(file, mode, &proc)
    when file.respond_to?(:binmode)
      file.binmode rescue nil
      yield(file)
    else
      yield(file)
    end
  end

  #
  # call-seq:
  #   encode(source_string, level = 1, opts = {}) -> encoded_lz4_data
  #   encode(output_io, level = 1, opts = {}) -> stream_encoder
  #   encode(output_io, level = 1, opts = {}) { |stream_encoder| ... } -> yield_status
  #
  # Encode to LZ4 stream data. This is available streaming process, but posible sequential write only.
  #
  # Created data is decodable by lz4-cli.
  #
  # ==== 共通引数
  #
  # [level = 1 (Integer)]
  #   圧縮レベルを指定します。0 から 9 までの整数値が指定出来ます。
  #
  #   現時点では lz4cli (lz4io) に倣って、3以下が標準圧縮、4以上が高圧縮となります。
  #
  #   4位上の値は、高効率圧縮器の圧縮レベルとして渡されます。
  #
  # [legacy: false (true or false)]
  #   THIS IS NOT SUPPORTED FUNCTION.
  #
  # [block_dependency: false (true or false)]
  #   Enable or disable block dependency funcion. Default is false.
  #
  #   真を与えた場合、ストリームの圧縮効率が向上しますが、特定のブロックのみを取り出すことが難しくなります。
  #
  # [block_checksum: false (true or false)]
  #   ブロックごとのチェックサム (XXhash32) の有効・無効を切り替えます。
  #
  # [stream_size: nil (nil or Integer)]
  #   THIS IS NOT IMPLEMENTED FUNCTION YET.
  #
  # [stream_checksum: true (true or false)]
  #   ストリーム全体のチェックサム (XXhash32) の有効・無効を切り替えます。
  #
  # [preset_dictionary: nil (String)]
  #   THIS IS NOT IMPLEMENTED FUNCTION.
  #
  #   Because, original lz4 library is not implemented yet. (Feb. 2014)
  #
  # ==== encode(source_string, level = 1, opts = {}) -> encoded_data
  #
  # Basic stream encode method.
  #
  # [RETURN (String)]
  #   Encoded data as LZ4 stream
  #
  # [source_string (String)]
  #   LZ4 ストリームとして圧縮したいバイナリデータ列としての文字列です。
  #
  #   文字符号情報は無視されて純粋なバイナリデータ列として処理されます。
  #
  # ==== encode(output_io, level = 1, opts = {}) -> stream_encoder
  #
  # Available LZ4 stream encode.
  #
  # Write to encoder for data encoding.
  #
  # After finished encode process, you must call +StreamEncoder#close+.
  #
  # Return stream encoder if given an IO object (or psudo-object).
  #
  # この圧縮器に『書き込む』ことでデータは圧縮されます。圧縮処理を完了するときには #close を呼び出す必要があります。
  #
  # [RETURN (LZ4::StreamEncoder)]
  #
  # [output_io (IO)]
  #   LZ4 ストリームの出力先を指定します。IO#<< と同等の機能を持つオブジェクトである必要があります。
  #
  #   一例を挙げると、IO、StringIO、Array などのインスタンスが当てはまります。
  #
  # ==== encode(output_io, level = 1, opts = {}) { |stream_encoder| ... } -> yield_status
  #
  # IO オブジェクトとともにブロックを渡した場合、ブロック引数として圧縮器が渡されます。この場合は #close を呼び出す必要がありません。
  #
  # [RETURN]
  #   return value of given block
  #
  # [YIELD (stream_encoder)]
  #
  # [YIELDRETURN]
  #   return as method return value
  #
  # ==== example: directly stream encode
  #
  #   LZ4.encode("abcdefghijklmn") # => Encoded LZ4 stream data (string object)
  #
  # ==== example: stream encode with block
  #
  # この用例は、encode_file の実装とほぼ同じです。丸写しで利用するよりは encode_file の利用を推奨します。
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

  #
  # call-seq:
  #   decode(encoded_data_string) -> decoded data
  #   decode(input_io) -> stream_decoder
  #   decode(input_io) { |stream_decoder| ... } -> yield_status
  #
  # Decode LZ4 stream data. This is available streaming process.
  #
  # ==== decode(encoded_data_string)
  #
  # [RETURN (String)]
  #   decoded_data
  #
  # ==== decode(input_io)
  #
  # [RETURN (LZ4::StreamDecoder)]
  #   ストリーム展開オブジェクトです。簡素な機能の読み込み専用IOオブジェクトとして扱うことが出来ます。
  #
  #   stream_decoder は GC によって開放処理が行われますが、利用しなくなった時点で利用者が明示的に close を呼び出すことが望まれます。
  #
  # [input_io (IO)]
  #   This is IO like object. Need read method. 'extlz4' is call as <tt>read(size, buf)</tt> style.
  #
  # ==== decode(input_io) { |stream_decoder| ... }
  #
  # [RETURN]
  #   returned value from given block
  #
  # [YIELD (stream_decoder)]
  #   ブロックなしで与えた場合の戻り値と等価です。
  #
  #   ただしこちらはブロックを抜けたらすぐに開放処理が実施されます。利用者が明示的に close を呼んだり、GC されるのを待ったりせずに行われると言うことです。
  #
  # ==== note
  #
  # Current implementation is possible 'sequential read' only.
  #
  # 'read behind' or 'random read' are not available.
  #
  # ==== example: directly decode
  #
  #   LZ4.decode(lz4_stream_encoded_string) # => decoded binary string
  #
  # ==== example: stream decode
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

    @@debugprint = ->(*args) { p caller(1, 1), *args; return *args }

    def initialize(io, level, blocksize, block_dependency,
                   block_checksum, stream_checksum)
      @block_checksum = !!block_checksum
      @stream_checksum = XXhash::Internal::StreamingHash.new(0) if stream_checksum

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
      header << [XXhash.xxh32(desc, 0) >> 8].pack("C")
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
      workencbuf = ""
      if block_dependency
        streamencoder = LZ4::RawStreamEncoder.new(@blocksize, level)
        ->(src) { streamencoder.update(level, src, workencbuf) }
      else
        ->(src) { LZ4.raw_encode(level, src, workencbuf) }
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
        @io << [XXhash.xxh32(w, 0)].pack("V")
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
          @decoder = LZ4.method(:raw_decode)
        else
          @decoder = LZ4::RawStreamDecoder.new.method(:update)
        end
      when MAGIC_NUMBER_LEGACY
        @version = -1
        @blockindependence = true
        @blockchecksum = false
        @streamchecksum = false
        @blockmaximum = 1 << 23 # 8 MiB
        @streamsize = nil
        @presetdict = nil
        @decoder = LZ4.method(:raw_decode)
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

  #
  # Call LZ4::RawStreamEncoder.new.
  #
  def self.raw_stream_encode(*args)
    lz4 = RawStreamEncoder.new(*args)
    if block_given?
      yield(lz4)
    else
      lz4
    end
  end

  #
  # Call LZ4::RawStreamDecoder.new.
  #
  def self.raw_stream_decode(*args)
    lz4 = RawStreamDecoder.new(*args)
    if block_given?
      yield(lz4)
    else
      lz4
    end
  end
end
