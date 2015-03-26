#vim: set fileencoding:utf-8

require "stringio"

ver = RbConfig::CONFIG["ruby_version"]
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(File.dirname(__FILE__), ver, soname)
if File.file?(lib)
  require_relative File.join(ver, soname)
else
  require_relative soname
end

require_relative "extlz4/version"

#
# LZ4 data and streaming data processor.
#
module LZ4
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
  def self.encode_file(inpath, outpath, level = 1, **opt)
    open_file(inpath, "rb") do |infile|
      open_file(outpath, "wb") do |outfile|
        encode(outfile, level, **opt) do |lz4|
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
  #   4以上の値は、高効率圧縮器の圧縮レベルとして渡されます。
  #
  # [block_dependency: false (true or false)]
  #   Enable or disable block dependency funcion. Default is false.
  #
  #   真を与えた場合、ストリームの圧縮効率が向上しますが、特定のブロックのみを取り出すことが難しくなります。
  #
  # [block_checksum: false (true or false)]
  #   ブロックごとのチェックサム (XXhash32) の有効・無効を切り替えます。
  #
  # [stream_checksum: true (true or false)]
  #   ストリーム全体のチェックサム (XXhash32) の有効・無効を切り替えます。
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

  def self.encode(obj, level = 1, **opts)
    if obj.kind_of?(String)
      lz4 = LZ4::Encoder.new(out = "".force_encoding(Encoding::BINARY), level, **opts)
      lz4 << obj
      lz4.close
      out
    else
      lz4 = LZ4::Encoder.new(obj, level, **opts)
      return lz4 unless block_given?
      begin
        yield(lz4)
        obj
      ensure
        lz4.close
      end
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
  def self.decode(obj, *opts)
    if obj.kind_of?(String)
      lz4 = Decoder.new(StringIO.new(obj), *opts)
      dest = lz4.read
      lz4.close
      return (dest || "".b)
    end

    lz4 = Decoder.new(obj, *opts)
    return lz4 unless block_given?

    begin
      yield(lz4)
    ensure
      lz4.close
    end
  end

  def self.raw_encode(*args)
    RawEncoder.encode(*args)
  end

  def self.raw_decode(*args)
    RawDecoder.decode(*args)
  end

  #
  # Call LZ4::RawEncoder.new.
  #
  def self.raw_stream_encode(*args)
    lz4 = RawEncoder.new(*args)
    if block_given?
      yield(lz4)
    else
      lz4
    end
  end

  #
  # Call LZ4::RawDecoder.new.
  #
  def self.raw_stream_decode(*args)
    lz4 = RawDecoder.new(*args)
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
    alias raw_compress raw_encode
    alias raw_decompress raw_decode
    alias raw_uncompress raw_decode
    alias raw_stream_compress raw_stream_encode
    alias raw_stream_decompress raw_stream_decode
    alias raw_stream_uncompress raw_stream_decode
  end
end
