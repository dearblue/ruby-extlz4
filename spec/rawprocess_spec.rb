#vim: set fileencoding:utf-8

require "tmpdir"
basedir = File.join(Dir.tmpdir, "$ruby$lz4-test-work")
Dir.mkdir basedir unless File.directory? basedir
filepath = File.join(basedir, "file1")

require "extlz4"

Dir.chdir basedir do
  describe LZ4 do
    src = ((?a .. ?z).to_a.shuffle * 5).join.freeze
    buf = ""

    before(:all) do
    end

    it "raw_encode (valid arguments)" do
      expect(LZ4.raw_encode(src)).to be_a_kind_of String
      expect(LZ4.raw_encode(src, 1000)).to be_a_kind_of String
      expect(LZ4.raw_encode(src, buf)).to eq buf
      expect(LZ4.raw_encode(src, 1000, buf)).to eq buf
      expect(LZ4.raw_encode(nil, src, 1000, buf)).to eq buf

      # high compression
      expect(LZ4.raw_encode(0, src)).to be_a_kind_of String
      expect(LZ4.raw_encode(0, src, 1000)).to be_a_kind_of String
      expect(LZ4.raw_encode(0, src, buf)).to eq buf
      expect(LZ4.raw_encode(0, src, 1000, buf)).to eq buf
    end

    it "raw encode (invalid arguments)" do
      expect { LZ4.raw_encode }.to raise_error ArgumentError # no arguments
      expect { LZ4.raw_encode(-1, src) }.to raise_error ArgumentError # wrong level
      expect { LZ4.raw_encode(100) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_encode(nil) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_encode(:bad_input) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_encode(/bad-input/) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_encode(src, "bad-destbuf".freeze) }.to raise_error RuntimeError # can't modify frozen String
      expect { LZ4.raw_encode(src, 1) }.to raise_error LZ4::Error # maxdest is too small
      expect { LZ4.raw_encode(src, 1, buf) }.to raise_error LZ4::Error # maxdest is too small
      expect { LZ4.raw_encode(src, "bad-maxsize", "a") }.to raise_error TypeError # "bad-maxsize" is not integer
    end

    lz4d = LZ4.raw_encode(src)
    it "raw_decode (valid arguments)" do
      expect(LZ4.raw_decode(lz4d)).to be_a_kind_of String
      expect(LZ4.raw_decode(lz4d, 1000)).to be_a_kind_of String
      expect(LZ4.raw_decode(lz4d, buf)).to eq buf
      expect(LZ4.raw_decode(lz4d, 1000, buf)).to eq buf
    end

    it "raw decode (invalid arguments)" do
      expect { LZ4.raw_decode }.to raise_error ArgumentError # no arguments
      expect { LZ4.raw_decode(-1, lz4d) }.to raise_error TypeError # do not given level
      expect { LZ4.raw_decode(100) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_decode(nil) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_decode(:bad_input) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_decode(/bad-input/) }.to raise_error TypeError # source is not string
      expect { LZ4.raw_decode(lz4d, "bad-destbuf".freeze) }.to raise_error RuntimeError # can't modify frozen String
      expect { LZ4.raw_decode(lz4d, "bad-maxsize", "a") }.to raise_error TypeError # "bad-maxsize" is not integer
    end

    it "raw encode and decode" do
      expect(LZ4.raw_decode(LZ4.raw_encode(src))).to eq src
      expect(LZ4.raw_decode(LZ4.raw_encode(src, 100))).to eq src
      expect(LZ4.raw_decode(LZ4.raw_encode(src), 131)).to eq src
      expect(LZ4.raw_decode(LZ4.raw_encode(src, 100), 131)).to eq src
    end

    after(:all) do
    end
  end
end
