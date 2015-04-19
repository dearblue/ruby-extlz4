#vim: set fileencoding:utf-8

require "stringio"

ver = RUBY_VERSION.slice(/\d+\.\d+/)
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(File.dirname(__FILE__), ver, soname)
case
when File.file?(lib)
  require_relative File.join(ver, soname)
when File.file?(File.join(File.dirname(__FILE__), ver))
  require_relative soname
else
  require soname
end

require_relative "extlz4/version"

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
  #   Give input file path, or input IO (liked) object its has ``read'' method.
  #
  # [outpath]
  #   Give output file path, or output IO (liked) object its has ``<<'' method.
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
  #   encode_file(inpath, outpath, level = 1, opts = {}) -> nil
  #
  # Encode regular file to lz4 file.
  #
  # [RETURN]
  #   Return nil always.
  #
  # [inpath]
  #   Give input file path, or input IO (liked) object its has ``read'' method.
  #
  # [outpath]
  #   Give output file path, or output IO (liked) object its has ``<<'' method.
  #
  # [level = 1 (Integer)]
  #   See LZ4.encode method.
  #
  # [opts = {} (Hash)]
  #   See LZ4.encode method.
  #
  def self.encode_file(inpath, outpath, *args, **opts)
    open_file(inpath, "rb") do |infile|
      open_file(outpath, "wb") do |outfile|
        encode(outfile, *args, **opts) do |lz4|
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
  #   encode(source_string, level = 1, opts = {}) -> lz4 frame'd data
  #   encode(output_io, level = 1, opts = {}) -> stream encoder
  #   encode(output_io, level = 1, opts = {}) { |stream_encoder| ... } -> yield_status
  #
  # Encode to LZ4 Frame data. This is available streaming process.
  #
  # Created data is decodable by lz4-cli.
  #
  # ==== Common parameters
  #
  # [level = 1 (Integer)]
  #   圧縮レベルを指定します。0 から 9 までの整数値が指定出来ます。
  #
  #   現時点では lz4cli (lz4io) に倣って、3以下が標準圧縮、4以上が高圧縮となります。
  #
  #   4以上の値は、高効率圧縮器の圧縮レベルとして渡されます。
  #
  # [blocklink: false (true or false)]
  #   Enable or disable block dependency funcion. Default is false.
  #
  #   真を与えた場合、ストリームの圧縮効率が向上します。
  #
  # [checksum: true (true or false)]
  #   ストリーム全体のチェックサム (XXhash32) の有効・無効を切り替えます。
  #
  # ==== encode(source_string, level = 1, opts = {}) -> encoded_data
  #
  # Basic encode method.
  #
  # [RETURN (String)]
  #   Encoded data as LZ4 stream
  #
  # [source_string (String)]
  #   LZ4 ストリームとして圧縮したいバイナリデータ列としての文字列です。
  #
  #   文字符号情報は無視されて純粋なバイナリデータ列として処理されます。
  #
  # ==== encode(output_io, level = 1, opts = {}) -> encoder
  #
  # Available streaming LZ4 Frame encode.
  #
  # Write to encoder for data encoding.
  #
  # After finished encode process, you must call Encoder#close.
  #
  # Return stream encoder if given an IO (liked) object.
  #
  # この圧縮器に『書き込む』ことでデータは圧縮されます。
  # 圧縮処理を完了するときには #close を呼び出す必要があります。
  #
  # [RETURN (LZ4::Encoder)]
  #
  # [output_io (IO)]
  #   LZ4 ストリームの出力先を指定します。IO#<< と同等の機能を持つオブジェクトである必要があります。
  #
  #   一例を挙げると、IO、StringIO、Array などのインスタンスが当てはまります。
  #
  # ==== encode(output_io, level = 1, opts = {}) { |encoder| ... } -> yield_status
  #
  # IO オブジェクトとともにブロックを渡した場合、ブロック引数として圧縮器が渡されます。この場合は #close を呼び出す必要がありません。
  #
  # [RETURN]
  #   return value of given block
  #
  # [YIELD (encoder)]
  #
  # [YIELDRETURN]
  #   return as method return value
  #
  # ==== example: directly encode
  #
  #   LZ4.encode("abcdefghijklmn") # => Encoded LZ4 stream data (string object)
  #
  # ==== example: streaming encode with block
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
  def self.encode(*args, **opts)
    if args.empty? || !args[0].kind_of?(String)
      lz4 = LZ4::Encoder.new(*args, **opts)
      return lz4 unless block_given?
      begin
        yield(lz4)
        lz4.outport
      ensure
        lz4.close
      end
    else
      obj = args.shift
      outport = "".force_encoding(Encoding::BINARY)
      lz4 = LZ4::Encoder.new(outport, *args, **opts)
      lz4 << obj
      lz4.close
      outport
    end
  end

  #
  # call-seq:
  #   decode(encoded_data_string) -> decoded data
  #   decode(input_io) -> decoder
  #   decode(input_io) { |decoder| ... } -> yield_status
  #
  # Decode LZ4 Frame data. This is available streaming process.
  #
  # ==== decode(encoded_data_string)
  #
  # [RETURN (String)]
  #   decoded_data
  #
  # ==== decode(input_io)
  #
  # [RETURN (LZ4::Decoder)]
  #   ストリーム展開オブジェクトです。簡素な機能の読み込み専用IOオブジェクトとして扱うことが出来ます。
  #
  #   decoder は GC によって開放処理が行われますが、利用しなくなった時点で利用者が明示的に close を呼び出すことが望まれます。
  #
  # [input_io (IO)]
  #   This is IO like object. Need read method. 'extlz4' is call as <tt>read(size, buf)</tt> style.
  #
  # ==== decode(input_io) { |decoder| ... }
  #
  # [RETURN]
  #   returned value from given block
  #
  # [YIELD (decoder)]
  #   ブロックなしで与えた場合の戻り値と等価です。
  #
  #   ただしこちらはブロックを抜けたらすぐに開放処理が実施されます。利用者が明示的に close を呼んだり、GC されるのを待ったりせずに行われると言うことです。
  #
  # ==== example: directly decode
  #
  #   LZ4.decode(lz4_encoded_string) # => decoded binary string
  #
  # ==== example: streaming decode
  #
  #   File.open("sample.lz4", "rb") do |fd|
  #     LZ4.decode(fd) do |lz4dec|
  #       lz4dec.read(16)  # string with read 16 bytes
  #       lz4dec.getbyte   # integer with a byte
  #       lz4dec.read      # string with rest data
  #     end
  #   end
  #
  def self.decode(obj, *args)
    if obj.kind_of?(String)
      lz4 = Decoder.new(StringIO.new(obj), *args)
      dest = lz4.read
      lz4.close
      return (dest || "".b)
    end

    lz4 = Decoder.new(obj, *args)
    return lz4 unless block_given?

    begin
      yield(lz4)
    ensure
      lz4.close
    end
  end

  def self.block_encode(*args)
    BlockEncoder.encode(*args)
  end

  def self.block_decode(*args)
    BlockDecoder.decode(*args)
  end

  #
  # Call LZ4::BlockEncoder.new.
  #
  def self.block_stream_encode(*args)
    lz4 = BlockEncoder.new(*args)
    if block_given?
      yield(lz4)
    else
      lz4
    end
  end

  #
  # Call LZ4::BlockDecoder.new.
  #
  def self.block_stream_decode(*args)
    lz4 = BlockDecoder.new(*args)
    if block_given?
      yield(lz4)
    else
      lz4
    end
  end

  class << self
    alias compress encode
    alias decompress decode
    alias uncompress decode
    alias block_compress block_encode
    alias block_decompress block_decode
    alias block_uncompress block_decode
    alias block_stream_compress block_stream_encode
    alias block_stream_decompress block_stream_decode
    alias block_stream_uncompress block_stream_decode
  end
end

require_relative "extlz4/compat"
