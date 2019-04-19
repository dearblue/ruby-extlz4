# Quick reference for extlz4

  - Utilities
      - `LZ4.encode_file(inpath, outpath, level = 1, opts = {}) -> nil'
      - `LZ4.decode_file(inpath, outpath) -> nil'
      - `LZ4.encode(*args)` &lt; short cut to LZ4::Encoder.encode &gt;
      - `LZ4.decode(*args)` &lt; short cut to LZ4::Decoder.decode &gt;
      - `LZ4.block_encode(*args)` &lt; short cut to LZ4::BlockEncoder.encode &gt;
      - `LZ4.block_decode(*args)` &lt; short cut to LZ4::BlockDecoder.decode &gt;
      - `LZ4.block_stream_encode(*args)` &lt; short cut to LZ4::BlockEncoder.new &gt;
      - `LZ4.block_stream_decode(*args)` &lt; short cut to LZ4::BlockDecoder.new &gt;
  - Refinements (by `using LZ4`)
      - `object.to_lz4frame(*args)` is same as `LZ4.encode(object, *args)`
      - `object.unlz4frame(*args)` is same as `LZ4.decode(object, *args)`
      - `string.to_lz4frame(*args)` is same as `LZ4.encode(string, *args)`
      - `string.unlz4frame(*args)` is same as `LZ4.decode(string, *args)`
      - `string.to_lz4block(*args)` is same as `LZ4.block_encode(string, *args)`
      - `string.unlz4block(*args)` is same as `LZ4.block_decode(string, *args)`
  - LZ4 Frame API (compression)
      - `LZ4::Encoder.new(outport, level = 1, legacy: false, blocklink: false, blocksum: false, streamsize: nil, streamsum: true, predict: nil)`
      - `LZ4::Encoder#close`
      - `LZ4::Encoder#write(src)`
      - `LZ4::Encoder#<<(src)`
      - `LZ4::Encoder#flush(flush = nil)`
  - LZ4 Frame API (decompression)
      - `LZ4::Decoder.new(inport, readblocksize = 256 * 1024, predict: nil)`
      - `LZ4::Decoder#close`
      - `LZ4::Decoder#read(size = nil, dest = nil) -> dest`
  - LZ4 Block API (compression)
      - `LZ4::BlockEncoder.encode(level = nil, src, dest = nil) -> dest`  
        `LZ4::BlockEncoder.encode(level = nil, src, max_dest_size, dest = nil) -> dest`
      - `LZ4::BlockEncoder.new(blocksize, is_high_compress = nil, preset_dictionary = nil) -> block encoder`
      - `LZ4::BlockEncoder#update(level = nil, src, dest = nil) -> dest`  
        `LZ4::BlockEncoder#update(level = nil, src, max_dest_size, dest = nil) -> dest`
      - `LZ4::BlockEncoder#reset(blocksize = nil, is_high_compress = nil, preset_dictionary = nil) -> self`
      - `LZ4::BlockEncoder#release -> nil`
  - LZ4 Block API (decompression)
      - `LZ4::BlockDecoder.decode(src, dest = nil) -> dest`  
        `LZ4::BlockDecoder.decode(src, max_dest_size = nil, dest = nil) -> dest`
      - `LZ4::BlockDecoder.new(preset_dictionary = nil) -> block decoder`
      - `LZ4::BlockDecoder#update(src, dest = nil) -> dest`  
        `LZ4::BlockDecoder#update(src, max_dest_size = nil, dest = nil) -> dest`
      - `LZ4::BlockDecoder#reset(preset_dictionary = nil) -> self`
      - `LZ4::BlockDecoder#release -> nil`
