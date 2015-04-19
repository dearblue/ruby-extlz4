
module LZ4
  class << self
    alias raw_encode block_encode
    alias raw_decode block_decode
    alias raw_stream_encode block_stream_encode
    alias raw_stream_decode block_stream_decode
  end

  RawStreamEncoder = BlockEncoder
  RawStreamDecoder = BlockDecoder
end
