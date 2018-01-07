unless File.read("README.md", 4096) =~ /^\s*\*\s*version:{1,2}\s*(.+)/i
  raise "バージョン情報が README.md に見つかりません"
end

ver = $1
verfile = "lib/extlz4/version.rb"
LIB << verfile

file verfile => "README.md" do |*args|
  File.binwrite args[0].name, <<-VERSION_FILE
module LZ4
  VERSION = "#{ver}"
end
  VERSION_FILE
end


unmatch = %r(\bcontrib/lz4/(?:Makefile|appveyor\.yml|contrib|doc|examples|lib/Makefile|lib/dll|programs|tests|visual)(?:$|/))

DOC.reject! { |e| e =~ unmatch }

contrib = FileList["contrib/**/*"]
contrib.reject! { |e| e =~ unmatch }
EXTRA.concat(contrib)


GEMSTUB = Gem::Specification.new do |s|
  s.name = "extlz4"
  s.version = ver
  s.summary = "ruby bindings for LZ4"
  s.description = <<EOS
ruby bindings for LZ4 <https://github.com/lz4/lz4>.
EOS
  s.homepage = "https://github.com/dearblue/ruby-extlz4"
  s.license = "BSD-2-Clause"
  s.author = "dearblue"
  s.email = "dearblue@users.noreply.github.com"

  s.add_development_dependency "rake"
end
