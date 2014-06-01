require_relative "../extlz4"

module LZ4
  class StreamFixerForBug_0_1 < LZ4::StreamDecoder
    def fix(output, &block)
      export_header(output)
      export_fixedblocks(output, &block)
      export_streamsum(output) if @streamchecksum

      yield("fixed block translation is acompleshed", @io.pos, @io.size) if block

      self
    end

    def export_header(output)
      case @version
      when 1
        blocksize = BLOCK_MAXIMUM_SIZES.rassoc(@blockmaximum)[0]
        header = [MAGIC_NUMBER].pack("V")
        sd = VERSION_NUMBER |
             (@blockindependence ? 0 : BLOCK_INDEPENDENCY) |
             (@blockchecksum ? BLOCK_CHECKSUM : 0) |
             (false ? STREAM_SIZE : 0) |
             (@streamchecksum ? STREAM_CHECKSUM : 0) |
             (false ? PRESET_DICTIONARY : 0)
        bd = (blocksize << 4)
        desc = [sd, bd].pack("CC")
        header << desc
        header << [XXhash.xxh32(desc, 0) >> 8].pack("C")
        header << [@streamsize].pack("Q<") if @streamsize
        header << [XXhash.xxh32(@predict)].pack("V") if @predict
        output << header
      else
        raise LZ4::Error, "un-supported version"
      end
    end

    BLOCK_PIVOT_SIZE = 4

    def export_fixedblocks(output)
      # base is copied from LZ4::StreamDecoder#getnextblock

      total = @io.size
      canyield = block_given?
      endofblock = @io.size - BLOCK_PIVOT_SIZE
      endofblock -= 4 if @blockchecksum
      endofblock -= 4 if @streamchecksum

      while true
        yield("reading block", @io.pos, total) if canyield
 
        flags = @io.read(4).unpack("V")[0]
        iscomp = (flags >> 31) == 0 ? true : false
        blocksize = flags & 0x7fffffff
        unless blocksize > 0
          output << [flags].pack("V")
          break
        end

        unless iscomp
          blocksize1 = endofblock - @io.pos
          blocksize1 = @blockmaximum if blocksize1 > @blockmaximum

          if blocksize > blocksize1
            blocksize = blocksize1
            flags = LITERAL_DATA_BLOCK_FLAG | blocksize
            yield("correct block size", @io.pos - 4, total) if canyield
          else
          end
        end

        w = @io.read(blocksize, @readbuf)
        unless w.bytesize == blocksize
          raise IOError, "can not read block (readsize=#{w.bytesize}, needsize=#{blocksize} (#{"0x%x" % blocksize}))"
        end
        output << [flags].pack("V") << w
        output << @io.read(4) if @blockchecksum
      end
    end

    def export_streamsum(output)
      output << @io.read(4)
    end
  end

  def self.fix_extlz4_0_1_bug(inpath, outpath, &block)
    open_file(inpath, "rb") do |infile|
      open_file(outpath, "wb") do |outfile|
        fixer = LZ4::StreamFixerForBug_0_1.new(infile)
        fixer.fix(outfile, &block)
      end
    end

    nil
  end
end
