#vim: set fileencoding:utf-8

require "extlz4"
require "digest"

if false
require "tmpdir"
basedir = File.join(Dir.tmpdir, "$ruby$lz4-test-work")
Dir.mkdir basedir unless File.directory? basedir
filepath = File.join(basedir, "file1")
end

HASLZ4C = system("lz4c", in: File::NULL, out: File::NULL, err: File::NULL) ?
          true : false
HASLZ4 = system("lz4", in: File::NULL, out: File::NULL, err: File::NULL) ?
         true : false

unless HASLZ4 || HASLZ4C
  warn "#{File.basename __FILE__}: not found ``lz4'' or ``lz4c''. skiped test with lz4-cli."
end

#Dir.chdir basedir do
  describe LZ4 do
    src = ((?a .. ?z).to_a.shuffle * 5).join.freeze
    buf = ""

    before(:all) do
    end

    $stderr.print "#{File.basename __FILE__}: generating source data."
    orderdsrc = 256.times.to_a * 65536 # generate 16 MiB data
    $stderr.print ?.
    randomsrc = orderdsrc.shuffle.pack("C*").freeze
    $stderr.print ?.
    orderdsrc = orderdsrc.pack("C*").freeze
    $stderr.puts "done"

    it "self encode and decode" do
      expect(Digest::MD5.digest(LZ4.decode(LZ4.encode(orderdsrc)))).to eq Digest::MD5.digest(orderdsrc)
      expect(Digest::MD5.digest(LZ4.decode(LZ4.encode(randomsrc)))).to eq Digest::MD5.digest(randomsrc)
    end

    data = File.read(__FILE__, mode: "r:binary")
    lz4data = LZ4.encode(data)

    it "read test" do
      LZ4.decode(StringIO.new(lz4data)) do |lz4io|
        expect(lz4io.read).to eq data
      end

      dest = ""
      LZ4.decode(StringIO.new(lz4data)) do |lz4io|
        dest << lz4io.read(19) until lz4io.eof?
      end
      expect(dest).to eq data

      dest.clear
      LZ4.decode(StringIO.new(lz4data)) do |lz4io|
        dest << lz4io.read(23)
        dest << lz4io.read
      end
      expect(dest).to eq data
    end

    it "write test" do
      dest = ""
      LZ4.encode(StringIO.new(dest)) do |lz4io|
        data.each_line do |t|
          lz4io << t
        end
      end
      expect(LZ4.decode(dest)).to eq data
    end

    after(:all) do
    end
  end
#end
